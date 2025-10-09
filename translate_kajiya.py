#!/usr/bin/env python3
"""
Intelligent Rust to C++ translator for kajiya crates.

Features:
- Dependency graph analysis from 'use' statements using tree-sitter
- Topological sorting for correct translation order
- GPT-5 translation with context from dependencies
- Generates .h/.cpp pairs with proper namespaces
- Progress tracking and resume capability
- Incremental translation
"""

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Set, Optional, Tuple
from collections import defaultdict, deque
import openai

# Tree-sitter for accurate Rust parsing
try:
    import tree_sitter_rust as ts_rust
    from tree_sitter import Language, Parser

    TREE_SITTER_AVAILABLE = True
except ImportError:
    TREE_SITTER_AVAILABLE = False
    print("Warning: tree-sitter not available, falling back to regex parsing")


@dataclass
class RustModule:
    """Represents a Rust source file module."""

    path: Path
    relative_path: Path
    crate_name: str
    module_name: str
    dependencies: Set[str] = field(default_factory=set)
    content: str = ""
    translated_header: Optional[str] = None
    translated_source: Optional[str] = None

    def __hash__(self):
        return hash(self.relative_path)

    def __eq__(self, other):
        return (
            isinstance(other, RustModule) and self.relative_path == other.relative_path
        )


class DependencyAnalyzer:
    """Analyzes Rust dependencies from use statements."""

    def __init__(self, kajiya_root: Path):
        self.kajiya_root = kajiya_root
        self.crates_root = kajiya_root / "crates"
        self.modules: Dict[str, RustModule] = {}
        self.crate_map = self._auto_detect_crates()

        # Initialize tree-sitter parser for Rust
        if TREE_SITTER_AVAILABLE:
            self.parser = Parser(Language(ts_rust.language()))
        else:
            self.parser = None

    def _auto_detect_crates(self) -> Dict[str, str]:
        """
        Automatically detect all crates in the kajiya/crates directory.

        Returns:
            Dict mapping crate identifiers (underscore format) to crate names (hyphen format)
            e.g., {"kajiya": "kajiya", "kajiya_backend": "kajiya-backend", ...}
        """
        crate_map = {}

        # Try multiple possible crate root locations
        possible_roots = [
            self.crates_root,
            self.crates_root / "lib",
            self.crates_root / "bin",
        ]

        print("Auto-detecting crates...")
        for root_dir in possible_roots:
            if not root_dir.exists():
                continue

            print(f"  Scanning: {root_dir.relative_to(self.kajiya_root)}")

            for crate_dir in root_dir.iterdir():
                if not crate_dir.is_dir():
                    continue

                # Check if it has a Cargo.toml file
                cargo_toml = crate_dir / "Cargo.toml"
                if not cargo_toml.exists():
                    continue

                # The directory name is the crate name (may contain hyphens)
                crate_name = crate_dir.name

                # Create the identifier version (replace hyphens with underscores)
                # This is how it appears in Rust code: use kajiya_backend::...
                crate_identifier = crate_name.replace("-", "_")

                # Avoid duplicates
                if crate_identifier not in crate_map:
                    crate_map[crate_identifier] = crate_name
                    print(f"    Found crate: {crate_identifier} -> {crate_name}")

        print(f"Total crates detected: {len(crate_map)}")
        return crate_map

    def scan_crates(self) -> List[RustModule]:
        """Scan all Rust source files in kajiya crates."""
        print("\nScanning kajiya crates for Rust modules...")

        # Get all detected crate directories
        crate_dirs = []
        possible_roots = [
            self.crates_root,
            self.crates_root / "lib",
            self.crates_root / "bin",
        ]

        for root_dir in possible_roots:
            if not root_dir.exists():
                continue

            for crate_dir in root_dir.iterdir():
                if not crate_dir.is_dir():
                    continue

                cargo_toml = crate_dir / "Cargo.toml"
                if cargo_toml.exists():
                    crate_dirs.append(crate_dir)

        print(f"Found {len(crate_dirs)} crates to scan")

        # Scan each crate for .rs files
        for crate_dir in crate_dirs:
            crate_name = crate_dir.name
            src_dir = crate_dir / "src"

            if not src_dir.exists():
                continue

            # Recursively find all .rs files
            for rs_file in src_dir.rglob("*.rs"):
                if "test" in rs_file.name.lower():
                    continue

                relative = rs_file.relative_to(src_dir)
                module_name = self._get_module_name(relative)

                module = RustModule(
                    path=rs_file,
                    relative_path=relative,
                    crate_name=crate_name,
                    module_name=module_name,
                )

                # Use crate_name::module_name as key to avoid collisions
                module_key = f"{crate_name}::{relative}"
                self.modules[module_key] = module
                print(f"  Found: {crate_name}::{module_name} ({relative})")

        print(f"Total modules found: {len(self.modules)}")
        return list(self.modules.values())

    def _get_module_name(self, relative_path: Path) -> str:
        """Convert file path to module name."""
        parts = list(relative_path.parts[:-1]) + [relative_path.stem]
        if parts[-1] == "mod" or parts[-1] == "lib":
            parts = parts[:-1]
        return "::".join(parts) if parts else "root"

    def analyze_dependencies(self):
        """Analyze file-level dependencies between modules."""
        print("\nAnalyzing file-level dependencies...")

        for module in self.modules.values():
            with open(module.path, "r", encoding="utf-8") as f:
                module.content = f.read()

            # Build import alias map for this module
            # Maps: local_name -> full_crate_path
            # e.g., "vulkan" -> "kajiya_backend::vulkan"
            import_aliases = self._build_import_alias_map(module.content)

            # Extract all use statement paths (with wildcard info)
            deps = self._extract_use_statements_with_wildcards(module.content)

            for dep_path, is_wildcard in deps:
                # Filter external dependencies
                if dep_path.startswith(
                    (
                        "std",
                        "log",
                        "glam",
                        "parking_lot",
                        "anyhow",
                        "image",
                        "turbosloth",
                        "lazy_static",
                        "chrono",
                        "exr",
                        "half",
                        "wchar",
                        "bytemuck",
                        "smol",
                        "radiant",
                        "parking_lot",
                        "memmap2",
                        "fern",
                    )
                ):
                    continue

                # Resolve local imports using alias map
                # e.g., "vulkan::buffer" -> "kajiya_backend::vulkan::buffer"
                dep_path = self._resolve_local_import(dep_path, import_aliases)

                # Convert to file-level dependency
                dep_parts = dep_path.split("::")
                if dep_parts[0] not in self.crate_map:
                    continue

                if is_wildcard:
                    # Handle wildcard imports: add all modules from the crate
                    resolved_modules = self._resolve_wildcard_to_modules(dep_path)
                    module.dependencies.update(resolved_modules)
                else:
                    # Handle specific imports: resolve to file level
                    dep_module_key = self._resolve_use_to_module(dep_path)
                    if dep_module_key:
                        module.dependencies.add(dep_module_key)

            if module.dependencies:
                deps_str = ", ".join(sorted(module.dependencies))
                print(
                    f"  {module.crate_name}::{module.module_name} depends on: {deps_str}"
                )

    def _build_import_alias_map(self, content: str) -> Dict[str, str]:
        """
        Build a map of local import aliases to their full paths.

        Example:
            use kajiya_backend::{ash::vk, vulkan, BackendError};

            Creates mapping:
            {
                "vulkan": "kajiya_backend::vulkan",
                "BackendError": "kajiya_backend::BackendError",
            }

        This handles the case where subsequent imports like:
            use vulkan::buffer::{Buffer, BufferDesc};

        Can be resolved to:
            use kajiya_backend::vulkan::buffer::{Buffer, BufferDesc};
        """
        alias_map = {}

        # Extract use statements with brace imports
        deps = self._extract_use_statements_with_wildcards(content)

        for dep_path, is_wildcard in deps:
            parts = dep_path.split("::")

            # Only process paths from known crates
            if parts[0] not in self.crate_map:
                continue

            # For paths like "kajiya_backend::vulkan", we want to map:
            # "vulkan" -> "kajiya_backend::vulkan"
            if len(parts) >= 2:
                # The last component could be an alias
                local_name = parts[-1]
                full_path = dep_path

                # Store the mapping
                alias_map[local_name] = full_path

        return alias_map

    def _resolve_local_import(self, dep_path: str, alias_map: Dict[str, str]) -> str:
        """
        Resolve local imports using the alias map.

        Example:
            dep_path: "vulkan::buffer::Buffer"
            alias_map: {"vulkan": "kajiya_backend::vulkan"}
            returns: "kajiya_backend::vulkan::buffer::Buffer"
        """
        parts = dep_path.split("::")
        if not parts:
            return dep_path

        first_part = parts[0]

        # Check if the first part is a local alias
        if first_part in alias_map:
            # Replace the first part with the full path
            full_path = alias_map[first_part]
            if len(parts) > 1:
                # Append the remaining parts
                return f"{full_path}::{'::'.join(parts[1:])}"
            else:
                return full_path

        # No alias found, return as-is
        return dep_path

    def _resolve_use_to_module(self, use_path: str) -> Optional[str]:
        """
        Resolve a use statement path to the actual module file.

        Example:
            use_path: "kajiya::world_renderer::AddMeshOptions"
            returns: "kajiya::world_renderer" (the file that defines AddMeshOptions)

        Strategy:
        - Remove the last component (usually a type/function name)
        - Find the module file that contains this definition
        """
        parts = use_path.split("::")
        if len(parts) < 2:
            return None

        crate_identifier = parts[0]
        if crate_identifier not in self.crate_map:
            return None

        # Map crate identifier to folder name
        crate_folder = self.crate_map[crate_identifier]

        # Try to find matching module file
        # Strategy: progressively remove components from the end until we find a match
        # We start from len(parts)-1 to skip the last component (likely a type/function name)
        for i in range(len(parts) - 1, 0, -1):
            module_parts = parts[1:i]  # Skip crate identifier

            if not module_parts:
                # If no module parts left, this is a crate-level import
                continue

            module_path_str = "::".join(module_parts)

            # Try to find this module in our scanned modules
            for module_key, module in self.modules.items():
                if module.crate_name != crate_folder:
                    continue

                # Check if this module matches
                if module.module_name == module_path_str:
                    return f"{crate_folder}::{module_path_str}"

                # Also check if the file path matches
                # e.g., vulkan/device.rs for "vulkan::device"
                expected_path = "/".join(module_parts) + ".rs"
                if str(module.relative_path).replace("\\", "/") == expected_path:
                    return f"{crate_folder}::{module_path_str}"

        # Fallback: return root module of the crate
        for module_key, module in self.modules.items():
            if module.crate_name == crate_folder and module.module_name == "root":
                return f"{crate_folder}::root"

        return None

    def _resolve_wildcard_to_modules(self, use_path: str) -> Set[str]:
        """
        Resolve a wildcard import to all modules in the specified crate or module path.

        Example:
            use_path: "kajiya_simple"
            returns: all module keys from kajiya-simple crate

            use_path: "kajiya::renderers"
            returns: all modules under kajiya/renderers/
        """
        parts = use_path.split("::")
        if not parts:
            return set()

        crate_identifier = parts[0]
        if crate_identifier not in self.crate_map:
            return set()

        crate_folder = self.crate_map[crate_identifier]
        resolved_modules = set()

        if len(parts) == 1:
            # Wildcard import of entire crate: use kajiya_simple::*
            # Add all modules from this crate
            for module_key, module in self.modules.items():
                if module.crate_name == crate_folder:
                    module_full_key = f"{crate_folder}::{module.module_name}"
                    resolved_modules.add(module_full_key)
        else:
            # Wildcard import of a specific module: use kajiya::renderers::*
            # Add all modules that start with this path
            module_prefix = "::".join(parts[1:])  # Skip crate identifier

            for module_key, module in self.modules.items():
                if module.crate_name != crate_folder:
                    continue

                # Check if this module is under the specified path
                if module.module_name.startswith(module_prefix):
                    module_full_key = f"{crate_folder}::{module.module_name}"
                    resolved_modules.add(module_full_key)

        return resolved_modules

    def _extract_use_statements_with_wildcards(
        self, content: str
    ) -> Set[Tuple[str, bool]]:
        """
        Extract all module paths from use statements with wildcard information.

        Returns:
            Set of tuples (use_path, is_wildcard)
            - use_path: the import path (e.g., "kajiya::world_renderer::AddMeshOptions")
            - is_wildcard: True if this is a wildcard import (e.g., "kajiya_simple::*")
        """
        if self.parser:
            return self._extract_with_treesitter_wildcards(content)
        else:
            return self._extract_with_regex_wildcards(content)

    def _extract_with_treesitter_wildcards(self, content: str) -> Set[Tuple[str, bool]]:
        """Extract use statements with wildcard info using tree-sitter."""
        dependencies = set()

        tree = self.parser.parse(bytes(content, "utf8"))
        root_node = tree.root_node

        def extract_path(node) -> Optional[str]:
            """Extract path from a scoped_identifier or identifier node."""
            if node.type == "identifier":
                return node.text.decode("utf8")
            elif node.type == "scoped_identifier":
                parts = []
                current = node
                while current:
                    if current.type == "identifier":
                        parts.insert(0, current.text.decode("utf8"))
                        break
                    elif current.type == "scoped_identifier":
                        # Get the name part
                        name_node = current.child_by_field_name("name")
                        if name_node:
                            parts.insert(0, name_node.text.decode("utf8"))
                        # Move to path part
                        current = current.child_by_field_name("path")
                    else:
                        break
                return "::".join(parts) if parts else None
            return None

        def process_use_clause(
            node, base_path: str = "", is_wildcard_parent: bool = False
        ):
            """Recursively process use clauses."""
            if node.type == "scoped_identifier" or node.type == "identifier":
                path = extract_path(node)
                if path:
                    full_path = f"{base_path}::{path}" if base_path else path
                    # Skip crate/super/self prefixes
                    if not path.startswith(("crate::", "super::", "self::")):
                        dependencies.add((full_path, is_wildcard_parent))

            elif node.type == "scoped_use_list":
                # use foo::{bar, baz}
                path_node = node.child_by_field_name("path")
                if path_node:
                    new_base = extract_path(path_node)
                    if new_base and not new_base.startswith(("crate", "super", "self")):
                        base_path = (
                            f"{base_path}::{new_base}" if base_path else new_base
                        )

                list_node = node.child_by_field_name("list")
                if list_node:
                    for child in list_node.children:
                        if (
                            child.type != ","
                            and child.type != "{"
                            and child.type != "}"
                        ):
                            process_use_clause(child, base_path, is_wildcard_parent)

            elif node.type == "use_wildcard":
                # use foo::* - this is a wildcard import
                if base_path:
                    dependencies.add((base_path, True))

            elif node.type == "use_as_clause":
                # use foo as bar
                path_node = node.child_by_field_name("path")
                if path_node:
                    process_use_clause(path_node, base_path, is_wildcard_parent)

            else:
                # Recursively process children
                for child in node.children:
                    process_use_clause(child, base_path, is_wildcard_parent)

        def visit_node(node):
            """Visit all use_declaration nodes in the AST."""
            if node.type == "use_declaration":
                # Process the argument (what's being used)
                for child in node.children:
                    if child.type != "use" and child.type != ";":
                        process_use_clause(child)

            # Recurse to children
            for child in node.children:
                visit_node(child)

        visit_node(root_node)
        return dependencies

    def _extract_with_regex_wildcards(self, content: str) -> Set[Tuple[str, bool]]:
        """Fallback: Extract use statements with wildcard info using regex."""
        dependencies = set()

        # Normalize multi-line use statements
        normalized = re.sub(
            r"\{([^}]+)\}",
            lambda m: "{" + m.group(1).replace("\n", " ") + "}",
            content,
            flags=re.DOTALL,
        )

        # Simple use statements
        simple_pattern = r"use\s+(?:crate|super|self)?::?([a-zA-Z_][a-zA-Z0-9_]*(?:::[a-zA-Z_][a-zA-Z0-9_]*)*)\s*(?:as\s+[a-zA-Z_][a-zA-Z0-9_]*)?\s*;"
        for match in re.finditer(simple_pattern, normalized):
            path = match.group(1)
            dependencies.add((path, False))

        # Brace imports
        brace_pattern = r"use\s+(?:crate|super|self)?::?([a-zA-Z_][a-zA-Z0-9_]*(?:::[a-zA-Z_][a-zA-Z0-9_]*)*)\s*::\s*\{([^}]+)\}"
        for match in re.finditer(brace_pattern, normalized):
            base_path = match.group(1)
            items = match.group(2)

            for item in items.split(","):
                item = item.strip()
                if not item:
                    continue

                item = re.sub(r"\s+as\s+[a-zA-Z_][a-zA-Z0-9_]*", "", item)

                if item == "*":
                    # Wildcard import
                    dependencies.add((base_path, True))
                    continue

                if "::" in item:
                    dependencies.add((f"{base_path}::{item}", False))
                else:
                    dependencies.add((f"{base_path}::{item}", False))

        # Wildcard imports
        wildcard_pattern = r"use\s+(?:crate|super|self)?::?([a-zA-Z_][a-zA-Z0-9_]*(?:::[a-zA-Z_][a-zA-Z0-9_]*)*)\s*::\s*\*\s*;"
        for match in re.finditer(wildcard_pattern, normalized):
            path = match.group(1)
            dependencies.add((path, True))

        return dependencies

    def build_dependency_graph(self) -> Dict[str, Set[str]]:
        """Build dependency graph for topological sort."""
        graph = defaultdict(set)

        for module in self.modules.values():
            key = f"{module.crate_name}::{module.module_name}"
            graph[key] = module.dependencies.copy()

        return graph


class TopologicalSorter:
    """Topological sorting for translation order."""

    @staticmethod
    def sort(graph: Dict[str, Set[str]]) -> List[str]:
        """
        Perform topological sort using Kahn's algorithm.
        Returns list of module names in translation order.
        """
        print("\nPerforming topological sort...")

        # Calculate in-degrees
        in_degree = defaultdict(int)
        all_nodes = set(graph.keys())

        for deps in graph.values():
            all_nodes.update(deps)

        for node in all_nodes:
            in_degree[node] = 0

        for node, deps in graph.items():
            for dep in deps:
                in_degree[dep] += 1

        # Start with nodes that have no dependencies
        queue = deque([node for node in all_nodes if in_degree[node] == 0])
        sorted_order = []

        while queue:
            node = queue.popleft()
            sorted_order.append(node)

            # Reduce in-degree for dependent nodes
            for other_node, deps in graph.items():
                if node in deps:
                    in_degree[other_node] -= 1
                    if in_degree[other_node] == 0:
                        queue.append(other_node)

        # Check for cycles
        if len(sorted_order) != len(all_nodes):
            print("Warning: Circular dependencies detected!")
            # Add remaining nodes
            remaining = all_nodes - set(sorted_order)
            sorted_order.extend(remaining)

        print(f"Translation order determined: {len(sorted_order)} modules")
        return sorted_order


class GPTTranslator:
    """Translates Rust code to C++ using GPT."""

    def __init__(self, api_key: str, base_url: str, model: str = "gpt-5-high"):
        self.client = openai.OpenAI(api_key=api_key, base_url=base_url)
        self.model = model
        self.translation_cache: Dict[str, Tuple[str, str]] = {}

    @staticmethod
    def strip_markdown_codeblock(content: str) -> str:
        """
        Remove markdown code block wrappers from GPT responses.

        Handles formats like:
        ```cpp
        code here
        ```

        ```c++
        code here
        ```

        Or just:
        ```
        code here
        ```
        """
        content = content.strip()

        # Pattern 1: ```cpp ... ``` or ```c++ ... ``` or ``` ... ```
        patterns = [
            r"^```(?:cpp|c\+\+|C\+\+|CPP)\s*\n(.*)\n```$",
            r"^```\s*\n(.*)\n```$",
            r"^```(?:cpp|c\+\+|C\+\+|CPP)\s*(.*)\s*```$",
            r"^```\s*(.*)\s*```$",
        ]

        for pattern in patterns:
            match = re.match(pattern, content, re.DOTALL)
            if match:
                return match.group(1).strip()

        # No markdown wrapper found, return as-is
        return content

    def translate_module(
        self,
        module: RustModule,
        dependency_context: Dict[str, Tuple[str, str]],
        dependency_paths: Dict[str, Tuple[str, str]] = None,
    ) -> Tuple[str, str]:
        """
        Translate a Rust module to C++ header and source.

        Args:
            module: The Rust module to translate
            dependency_context: Dict of {module_name: (header, source)} for dependencies
            dependency_paths: Dict of {module_name: (header_path, source_path)} for dependencies

        Returns:
            Tuple of (header_content, source_content)
        """
        print(f"\nTranslating {module.crate_name}::{module.module_name}...")

        # Build context from dependencies
        context_str = self._build_context(dependency_context, dependency_paths or {})

        # Generate header
        header = self._translate_to_header(module, context_str)

        # Generate source
        source = self._translate_to_source(module, header, context_str)

        return header, source

    def _build_context(
        self,
        dependency_context: Dict[str, Tuple[str, str]],
        dependency_paths: Dict[str, Tuple[str, str]],
    ) -> str:
        """Build context string from translated dependencies."""
        if not dependency_context:
            return ""

        context_parts = ["Previously translated dependencies:\n"]

        for dep_name, (header, _) in dependency_context.items():
            # Get file paths if available
            if dep_name in dependency_paths:
                header_path, source_path = dependency_paths[dep_name]
                context_parts.append(
                    f"\n// {dep_name}\n"
                    f"// Header: {header_path}\n"
                    f"// Source: {source_path}\n"
                    f"{header[:500]}..."
                )
            else:
                context_parts.append(f"\n// {dep_name}\n{header[:500]}...")

        return "\n".join(context_parts)

    def _translate_to_header(self, module: RustModule, context: str) -> str:
        """Translate Rust to C++ header file."""
        namespace = self._get_namespace(module)

        prompt = f"""Translate this Rust code to a C++ header file (.h).

Requirements:
- Use namespace {namespace}
- Follow tekki project naming conventions (PascalCase for classes, PascalCase for methods)
- Use modern C++20 features
- Include proper header guards
- Add necessary includes
- Use tekki::core::Result for error handling
- Convert Rust types appropriately (Vec->std::vector, Arc->std::shared_ptr, etc.)
- Keep all comments and docstrings
- Do not use Macro based header guards, use #pragma once instead
- Use exceptions for error handling and try catch instead of return Results

{context}

Original Rust code ({module.path.name}):
```rust
{module.content}
```

Generate ONLY the C++ header file content, no explanations:"""

        response = self.client.chat.completions.create(
            model=self.model,
            messages=[
                {
                    "role": "system",
                    "content": "You are an expert at translating Rust to C++. Generate only code, no explanations.",
                },
                {"role": "user", "content": prompt},
            ],
            temperature=0.3,
            max_tokens=4000,
        )

        content = response.choices[0].message.content.strip()
        return self.strip_markdown_codeblock(content)

    def _translate_to_source(
        self, module: RustModule, header: str, context: str
    ) -> str:
        """Translate Rust to C++ source file."""
        namespace = self._get_namespace(module)
        header_name = self._get_header_name(module)

        prompt = f"""Translate this Rust code to a C++ source file (.cpp).

Requirements:
- Include the corresponding header: #include "{header_name}"
- Use namespace {namespace}
- Implement all functions declared in the header
- Follow tekki project conventions
- Use modern C++20 features
- Keep all comments and docstrings
- Use exceptions for error handling and try catch instead of return Results

Header file:
```cpp
{header}
```

{context}

Original Rust code ({module.path.name}):
```rust
{module.content}
```

Generate ONLY the C++ source file content, no explanations:"""

        response = self.client.chat.completions.create(
            model=self.model,
            messages=[
                {
                    "role": "system",
                    "content": "You are an expert at translating Rust to C++. Generate only code, no explanations.",
                },
                {"role": "user", "content": prompt},
            ],
            temperature=0.3,
            max_tokens=6000,
        )

        content = response.choices[0].message.content.strip()
        return self.strip_markdown_codeblock(content)

    def _get_namespace(self, module: RustModule) -> str:
        """Get C++ namespace for module."""
        # tekki::renderer, tekki::backend, etc.
        crate_to_ns = {
            "kajiya": "renderer",
            "kajiya-backend": "backend",
            "kajiya-rg": "render_graph",
            "kajiya-asset": "asset",
        }

        base_ns = crate_to_ns.get(
            module.crate_name, module.crate_name.replace("-", "_")
        )

        # Add sub-namespaces from path
        parts = list(module.relative_path.parts[:-1])
        if parts:
            sub_ns = "::".join(parts)
            return f"tekki::{base_ns}::{sub_ns}"

        return f"tekki::{base_ns}"

    def _get_header_name(self, module: RustModule) -> str:
        """Get header file name."""
        # Convert renderers/ibl.rs -> tekki/renderer/renderers/ibl.h
        crate_to_dir = {
            "kajiya": "renderer",
            "kajiya-backend": "backend",
            "kajiya-rg": "render_graph",
            "kajiya-asset": "asset",
        }

        base_dir = crate_to_dir.get(
            module.crate_name, module.crate_name.replace("-", "_")
        )
        rel_path = module.relative_path.with_suffix(".h")

        return f"tekki/{base_dir}/{rel_path}".replace("\\", "/")


class TranslationManager:
    """Manages the translation process with progress tracking."""

    def __init__(
        self,
        analyzer: DependencyAnalyzer,
        translator: GPTTranslator,
        output_root: Path,
        progress_file: Path,
    ):
        self.analyzer = analyzer
        self.translator = translator
        self.output_root = output_root
        self.progress_file = progress_file
        self.progress: Dict[str, bool] = {}
        # Store: {module_key: (header, source, header_path, source_path)}
        self.translated_context: Dict[str, Tuple[str, str, str, str]] = {}
        # Store module info: {module_key: RustModule}
        self.module_info: Dict[str, RustModule] = {}

        self._load_progress()

    def _load_progress(self):
        """Load translation progress from file."""
        if self.progress_file.exists():
            with open(self.progress_file, "r") as f:
                self.progress = json.load(f)
            print(f"Loaded progress: {len(self.progress)} modules already translated")

    def _save_progress(self):
        """Save translation progress to file."""
        with open(self.progress_file, "w") as f:
            json.dump(self.progress, f, indent=2)

    def translate_all(self):
        """Translate all modules in dependency order."""
        # Scan and analyze
        modules = self.analyzer.scan_crates()
        self.analyzer.analyze_dependencies()

        # Build dependency graph and sort
        graph = self.analyzer.build_dependency_graph()
        sorted_modules = TopologicalSorter.sort(graph)

        print(f"\nStarting translation of {len(sorted_modules)} modules...")
        print(f"Output directory: {self.output_root}")

        # Translate in order
        for i, module_key in enumerate(sorted_modules, 1):
            # Find module
            module = None
            for m in modules:
                if f"{m.crate_name}::{m.module_name}" == module_key:
                    module = m
                    break

            if not module:
                print(f"Warning: Module {module_key} not found in scanned modules")
                continue

            # Store module info for later reference
            self.module_info[module_key] = module

            # Check if already translated
            if self.progress.get(module_key, False):
                print(
                    f"[{i}/{len(sorted_modules)}] Skipping {module_key} (already done)"
                )
                continue

            try:
                # Get dependency context
                dep_context, dep_paths = self._get_dependency_context(module)

                # Translate
                header, source = self.translator.translate_module(
                    module, dep_context, dep_paths
                )

                # Get file paths
                header_path, source_path = self._get_output_paths(module)

                # Save files
                self._save_translation(module, header, source)

                # Update context with paths
                self.translated_context[module_key] = (
                    header,
                    source,
                    str(header_path.relative_to(self.output_root)),
                    str(source_path.relative_to(self.output_root)),
                )

                # Mark as done
                self.progress[module_key] = True
                self._save_progress()

                print(f"[{i}/{len(sorted_modules)}] ✓ {module_key}")

            except Exception as e:
                print(f"[{i}/{len(sorted_modules)}] ✗ {module_key}: {e}")
                continue

        print("\n✓ Translation complete!")

    def _get_dependency_context(
        self, module: RustModule
    ) -> Tuple[Dict[str, Tuple[str, str]], Dict[str, Tuple[str, str]]]:
        """
        Get translated context for module dependencies.

        Returns:
            Tuple of (dependency_context, dependency_paths)
            - dependency_context: {module_name: (header, source)}
            - dependency_paths: {module_name: (header_path, source_path)}
        """
        context = {}
        paths = {}

        for dep in module.dependencies:
            if dep in self.translated_context:
                # Unpack all four values
                header, source, header_path, source_path = self.translated_context[dep]
                context[dep] = (header, source)
                paths[dep] = (header_path, source_path)

        return context, paths

    def _get_output_paths(self, module: RustModule) -> Tuple[Path, Path]:
        """Get output header and source paths for a module."""
        crate_to_dir = {
            "kajiya": "renderer",
            "kajiya-backend": "backend",
            "kajiya-rg": "render_graph",
            "kajiya-asset": "asset",
        }

        base_dir = crate_to_dir.get(
            module.crate_name, module.crate_name.replace("-", "_")
        )

        # Header path: include/tekki/{base_dir}/{relative_path}.h
        header_path = (
            self.output_root
            / "include"
            / "tekki"
            / base_dir
            / module.relative_path.with_suffix(".h")
        )

        # Source path: src/{base_dir}/{relative_path}.cpp
        source_path = (
            self.output_root
            / "src"
            / base_dir
            / module.relative_path.with_suffix(".cpp")
        )

        return header_path, source_path

    def _save_translation(self, module: RustModule, header: str, source: str):
        """Save translated header and source files."""
        # Determine output paths
        crate_to_dir = {
            "kajiya": "renderer",
            "kajiya-backend": "backend",
            "kajiya-rg": "render_graph",
            "kajiya-asset": "asset",
        }

        base_dir = crate_to_dir.get(
            module.crate_name, module.crate_name.replace("-", "_")
        )

        # Header path: include/tekki/{base_dir}/{relative_path}.h
        header_path = (
            self.output_root
            / "include"
            / "tekki"
            / base_dir
            / module.relative_path.with_suffix(".h")
        )

        # Source path: src/{base_dir}/{relative_path}.cpp
        source_path = (
            self.output_root
            / "src"
            / base_dir
            / module.relative_path.with_suffix(".cpp")
        )

        # Create directories
        header_path.parent.mkdir(parents=True, exist_ok=True)
        source_path.parent.mkdir(parents=True, exist_ok=True)

        # Write files
        with open(header_path, "w", encoding="utf-8") as f:
            f.write(header)

        with open(source_path, "w", encoding="utf-8") as f:
            f.write(source)

        print(f"  Written: {header_path.relative_to(self.output_root)}")
        print(f"  Written: {source_path.relative_to(self.output_root)}")


def main():
    parser = argparse.ArgumentParser(
        description="Intelligent Rust to C++ translator for kajiya crates"
    )

    parser.add_argument("--base-url", required=True, help="OpenAI API base URL")

    parser.add_argument("--api-key", required=True, help="OpenAI API key")

    parser.add_argument(
        "--model", default="gpt-5-high", help="Model name (default: gpt-5-high)"
    )

    parser.add_argument(
        "--kajiya-root",
        type=Path,
        default=Path(__file__).parent / "kajiya",
        help="Path to kajiya source root (default: ./kajiya)",
    )

    parser.add_argument(
        "--output-root",
        type=Path,
        default=Path(__file__).parent,
        help="Output root directory (default: current directory)",
    )

    parser.add_argument(
        "--progress-file",
        type=Path,
        default=Path(__file__).parent / ".translation_progress.json",
        help="Progress tracking file (default: .translation_progress.json)",
    )

    args = parser.parse_args()

    # Validate paths
    if not args.kajiya_root.exists():
        print(f"Error: kajiya root not found at {args.kajiya_root}")
        sys.exit(1)

    # Initialize components
    analyzer = DependencyAnalyzer(args.kajiya_root)
    translator = GPTTranslator(args.api_key, args.base_url, args.model)
    manager = TranslationManager(
        analyzer, translator, args.output_root, args.progress_file
    )

    # Run translation
    try:
        manager.translate_all()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user. Progress has been saved.")
        sys.exit(0)
    except Exception as e:
        print(f"\n\nError: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
