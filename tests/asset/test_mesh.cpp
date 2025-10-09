#include <catch2/catch_test_macros.hpp>
#include <tekki/asset/mesh.h>
#include <glm/glm.hpp>

using namespace tekki::asset;

TEST_CASE("TriangleMesh basic operations", "[asset][mesh]") {
    SECTION("Create empty mesh") {
        TriangleMesh mesh;

        REQUIRE(mesh.positions.empty());
        REQUIRE(mesh.normals.empty());
        REQUIRE(mesh.tangents.empty());
        REQUIRE(mesh.tex_coords.empty());
        REQUIRE(mesh.indices.empty());
    }

    SECTION("Create simple triangle mesh") {
        TriangleMesh mesh;

        // Triangle vertices
        mesh.positions = {
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.5f, 1.0f, 0.0f)
        };

        // Normals (pointing towards viewer)
        mesh.normals = {
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        };

        // UV coordinates
        mesh.tex_coords = {
            glm::vec2(0.0f, 0.0f),
            glm::vec2(1.0f, 0.0f),
            glm::vec2(0.5f, 1.0f)
        };

        // Indices
        mesh.indices = {0, 1, 2};

        REQUIRE(mesh.positions.size() == 3);
        REQUIRE(mesh.normals.size() == 3);
        REQUIRE(mesh.tex_coords.size() == 3);
        REQUIRE(mesh.indices.size() == 3);
    }

    SECTION("Mesh bounds calculation") {
        TriangleMesh mesh;

        mesh.positions = {
            glm::vec3(-1.0f, -1.0f, -1.0f),
            glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, 0.0f)
        };

        // Calculate bounds
        glm::vec3 min_bound = mesh.positions[0];
        glm::vec3 max_bound = mesh.positions[0];

        for (const auto& pos : mesh.positions) {
            min_bound = glm::min(min_bound, pos);
            max_bound = glm::max(max_bound, pos);
        }

        REQUIRE(min_bound.x == -1.0f);
        REQUIRE(min_bound.y == -1.0f);
        REQUIRE(min_bound.z == -1.0f);
        REQUIRE(max_bound.x == 1.0f);
        REQUIRE(max_bound.y == 1.0f);
        REQUIRE(max_bound.z == 1.0f);
    }
}

TEST_CASE("Material data structure", "[asset][mesh]") {
    SECTION("Default material") {
        Material mat;

        REQUIRE(mat.base_color_texture == BindlessImageHandle::INVALID);
        REQUIRE(mat.normal_texture == BindlessImageHandle::INVALID);
        REQUIRE(mat.metallic_roughness_texture == BindlessImageHandle::INVALID);
    }

    SECTION("Material with textures") {
        Material mat;

        mat.base_color_texture = BindlessImageHandle(1);
        mat.normal_texture = BindlessImageHandle(2);
        mat.metallic_roughness_texture = BindlessImageHandle(3);

        REQUIRE(mat.base_color_texture.id == 1);
        REQUIRE(mat.normal_texture.id == 2);
        REQUIRE(mat.metallic_roughness_texture.id == 3);
    }
}

TEST_CASE("PackedTriMesh data packing", "[asset][mesh]") {
    SECTION("Verify packed mesh structure") {
        // PackedTriMesh uses compressed formats for GPU efficiency
        // This test verifies the structure can be created

        PackedTriMesh packed;
        packed.positions.push_back(glm::vec3(1.0f, 2.0f, 3.0f));
        packed.normals.push_back(0); // Packed normal (11-10-11 format)
        packed.tangents.push_back(0); // Packed tangent

        REQUIRE(packed.positions.size() == 1);
        REQUIRE(packed.normals.size() == 1);
        REQUIRE(packed.tangents.size() == 1);
    }
}

TEST_CASE("Mesh handle system", "[asset][mesh]") {
    SECTION("MeshHandle creation and comparison") {
        MeshHandle handle1(0);
        MeshHandle handle2(0);
        MeshHandle handle3(1);

        REQUIRE(handle1 == handle2);
        REQUIRE(handle1 != handle3);
    }

    SECTION("Invalid mesh handle") {
        MeshHandle invalid = MeshHandle::INVALID;
        REQUIRE(invalid.is_invalid());

        MeshHandle valid(5);
        REQUIRE_FALSE(valid.is_invalid());
    }
}
