// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma once

#include <variant>
#include <string>
#include <optional>
#include <utility>
#include <stdexcept>

namespace tekki {

/**
 * @brief Result type for error handling (similar to Rust's Result<T, E>)
 *
 * Usage:
 *   Result<int> foo() {
 *     if (error) return Err<int>("error message");
 *     return Ok(42);
 *   }
 */
template<typename T>
class Result {
public:
    Result(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) = default;

    // Check if result is Ok
    bool IsOk() const { return std::holds_alternative<T>(data_); }
    bool IsErr() const { return std::holds_alternative<std::string>(data_); }

    // Conversion to bool (true if Ok)
    explicit operator bool() const { return IsOk(); }

    // Get value (throws if Err)
    T& Value() & {
        if (IsErr()) {
            throw std::runtime_error(Error());
        }
        return std::get<T>(data_);
    }

    const T& Value() const& {
        if (IsErr()) {
            throw std::runtime_error(Error());
        }
        return std::get<T>(data_);
    }

    T&& Value() && {
        if (IsErr()) {
            throw std::runtime_error(Error());
        }
        return std::move(std::get<T>(data_));
    }

    // Get value or default
    T ValueOr(T&& defaultValue) const& {
        return IsOk() ? std::get<T>(data_) : std::move(defaultValue);
    }

    T ValueOr(T&& defaultValue) && {
        return IsOk() ? std::move(std::get<T>(data_)) : std::move(defaultValue);
    }

    // Get error message
    const std::string& Error() const {
        return std::get<std::string>(data_);
    }

    // Dereference operators
    T& operator*() & { return Value(); }
    const T& operator*() const& { return Value(); }
    T&& operator*() && { return std::move(Value()); }

    T* operator->() { return &Value(); }
    const T* operator->() const { return &Value(); }

private:
    friend Result Ok<T>(T&&);
    friend Result Err<T>(std::string);

    std::variant<T, std::string> data_;

    explicit Result(T&& value) : data_(std::move(value)) {}
    explicit Result(const std::string& error) : data_(error) {}
};

// Specialization for void
template<>
class Result<void> {
public:
    Result(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) = default;

    bool IsOk() const { return !error_.has_value(); }
    bool IsErr() const { return error_.has_value(); }

    explicit operator bool() const { return IsOk(); }

    void Value() const {
        if (IsErr()) {
            throw std::runtime_error(*error_);
        }
    }

    const std::string& Error() const { return *error_; }

private:
    friend Result Ok();
    friend Result Err<void>(std::string);

    std::optional<std::string> error_;

    Result() : error_(std::nullopt) {}
    explicit Result(const std::string& error) : error_(error) {}
};

// Helper functions to create Result
template<typename T>
Result<T> Ok(T&& value) {
    return Result<T>(std::forward<T>(value));
}

inline Result<void> Ok() {
    return Result<void>();
}

template<typename T>
Result<T> Err(const std::string& error) {
    return Result<T>(error);
}

template<typename T>
Result<T> Err(std::string&& error) {
    return Result<T>(std::move(error));
}

} // namespace tekki
