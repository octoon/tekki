#pragma once

#include <variant>
#include <string>
#include <stdexcept>
#include <utility>
#include <source_location>

namespace tekki::core {

// Forward declaration
template<typename T>
class Result;

// Error class - represents failures with context
class Error {
public:
    std::string Message;
    std::source_location Location;

    Error(std::string message,
          std::source_location location = std::source_location::current())
        : Message(std::move(message)), Location(location) {}

    const char* What() const noexcept { return Message.c_str(); }

    // Context addition (like anyhow's context)
    Error WithContext(const std::string& context) const {
        return Error(context + ": " + Message, Location);
    }
};

// Helper to create errors
inline Error MakeError(const std::string& message,
                       std::source_location location = std::source_location::current()) {
    return Error(message, location);
}

// Result<T> - similar to Rust's Result<T, E> and anyhow::Result<T>
template<typename T>
class Result {
private:
    std::variant<T, Error> data_;

public:
    // Constructors
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}

    // Factory methods
    static Result Ok(T value) { return Result(std::move(value)); }
    static Result Err(Error error) { return Result(std::move(error)); }
    static Result Err(const std::string& message) {
        return Result(MakeError(message));
    }

    // Check state
    bool IsOk() const noexcept { return std::holds_alternative<T>(data_); }
    bool IsErr() const noexcept { return std::holds_alternative<Error>(data_); }
    explicit operator bool() const noexcept { return IsOk(); }

    // Access value (throws if error)
    T& Value() & {
        if (IsErr()) {
            throw std::runtime_error(GetError().Message);
        }
        return std::get<T>(data_);
    }

    T&& Value() && {
        if (IsErr()) {
            throw std::runtime_error(GetError().Message);
        }
        return std::get<T>(std::move(data_));
    }

    const T& Value() const & {
        if (IsErr()) {
            throw std::runtime_error(GetError().Message);
        }
        return std::get<T>(data_);
    }

    // Access error (undefined if ok)
    Error& GetError() & { return std::get<Error>(data_); }
    const Error& GetError() const & { return std::get<Error>(data_); }
    Error&& GetError() && { return std::get<Error>(std::move(data_)); }

    // Get value or default
    T ValueOr(T default_value) const & {
        return IsOk() ? Value() : std::move(default_value);
    }

    T ValueOr(T default_value) && {
        return IsOk() ? std::move(*this).Value() : std::move(default_value);
    }

    // Unwrap (throws on error) - like Rust's unwrap()
    T Unwrap() && {
        if (IsErr()) {
            throw std::runtime_error(GetError().Message);
        }
        return std::get<T>(std::move(data_));
    }

    // Expect with custom message - like Rust's expect()
    T Expect(const std::string& message) && {
        if (IsErr()) {
            throw std::runtime_error(message + ": " + GetError().Message);
        }
        return std::get<T>(std::move(data_));
    }

    // Map the value if Ok
    template<typename F>
    auto Map(F&& func) -> Result<decltype(func(std::declval<T>()))> {
        using U = decltype(func(std::declval<T>()));
        if (IsOk()) {
            return Result<U>::Ok(func(std::move(*this).Value()));
        }
        return Result<U>::Err(std::move(*this).GetError());
    }

    // Add context to error (like anyhow)
    Result WithContext(const std::string& context) && {
        if (IsErr()) {
            return Result::Err(GetError().WithContext(context));
        }
        return std::move(*this);
    }
};

// Specialization for Result<void>
template<>
class Result<void> {
private:
    std::variant<std::monostate, Error> data_;

public:
    // Constructors
    Result() : data_(std::monostate{}) {}
    Result(Error error) : data_(std::move(error)) {}

    // Factory methods
    static Result Ok() { return Result(); }
    static Result Err(Error error) { return Result(std::move(error)); }
    static Result Err(const std::string& message) {
        return Result(MakeError(message));
    }

    // Check state
    bool IsOk() const noexcept { return std::holds_alternative<std::monostate>(data_); }
    bool IsErr() const noexcept { return std::holds_alternative<Error>(data_); }
    explicit operator bool() const noexcept { return IsOk(); }

    // Access error (undefined if ok)
    Error& GetError() & { return std::get<Error>(data_); }
    const Error& GetError() const & { return std::get<Error>(data_); }
    Error&& GetError() && { return std::get<Error>(std::move(data_)); }

    // Unwrap (throws on error)
    void Unwrap() const {
        if (IsErr()) {
            throw std::runtime_error(GetError().Message);
        }
    }

    // Expect with custom message
    void Expect(const std::string& message) const {
        if (IsErr()) {
            throw std::runtime_error(message + ": " + GetError().Message);
        }
    }

    // Add context to error
    Result WithContext(const std::string& context) && {
        if (IsErr()) {
            return Result::Err(GetError().WithContext(context));
        }
        return std::move(*this);
    }
};

// Macros for convenience (similar to Rust's ? operator and anyhow's bail!)

// TRY - propagate error if Result is Err
#define TRY(expr) \
    ({ \
        auto __result = (expr); \
        if (!__result.IsOk()) { \
            return std::move(__result).GetError(); \
        } \
        std::move(__result).Value(); \
    })

// BAIL - return an error immediately
#define BAIL(msg) \
    return ::tekki::core::Result<void>::Err(msg)

// ENSURE - assert condition or return error
#define ENSURE(cond, msg) \
    if (!(cond)) { \
        return ::tekki::core::MakeError(msg); \
    }

} // namespace tekki::core
