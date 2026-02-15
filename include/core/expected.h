#pragma once

/* A (very) rough Implementation approximation of std::expected. Since compiler
 * support for C++23 (and thus std::expected) isn't super common I'm creating my
 * own which will not be as robust but will work for my limited usecases.
 *
 * https://en.cppreference.com/w/cpp/utility/expected.html
 */

#include <exception>
#include <utility>
#include <variant>

/* Exception thrown when accessing value/error incorrectly */
class bad_expected_access : public std::exception {
public:
    const char* what() const noexcept override {
        return "bad expected access";
    }
};

/* Wrapper type to disambiguate error construction */
template<typename E>
struct Unexpected {
    E error;

    explicit Unexpected(const E& e) : error(e) {}
    explicit Unexpected(E&& e) : error(std::move(e)) {}
};

/* Helper function to create Unexpected values */
template<typename E>
Unexpected<E> make_unexpected(E&& error) {
    return Unexpected<E>(std::forward<E>(error));
}

template<typename T, typename E>
class Expected {
public:
    /* Construct from a value (success case) */
    Expected(const T& value) : data_m(value) {}
    Expected(T&& value) : data_m(std::move(value)) {}

    /* Construct from an error (failure case) */
    Expected(const Unexpected<E>& unex) : data_m(unex.error) {}
    Expected(Unexpected<E>&& unex) : data_m(std::move(unex.error)) {}

    /* Check if contains a value */
    operator bool() const { return has_value(); }
    bool has_value() const { return std::holds_alternative<T>(data_m); }

    /* Access value (throws if contains error) */
    T& value() {
        if (!has_value())
            throw bad_expected_access();
        return std::get<T>(data_m);
    }

    const T& value() const {
        if (!has_value())
            throw bad_expected_access();
        return std::get<T>(data_m);
    }

    /* Access error (throws if contains value) */
    E& error() {
        if (has_value())
            throw bad_expected_access();
        return std::get<E>(data_m);
    }

    const E& error() const {
        if (has_value())
            throw bad_expected_access();
        return std::get<E>(data_m);
    }

    /* Dereference operators - access value */
    T& operator*() {
        return value();
    }

    const T& operator*() const {
        return value();
    }

    T* operator->() {
        return &value();
    }

    const T* operator->() const {
        return &value();
    }

    /* Return value if present, otherwise return default */
    template<typename U>
    T value_or(U&& default_value) const {
        return has_value() ? std::get<T>(data_m) : static_cast<T>(std::forward<U>(default_value));
    }

private:
    std::variant<T, E> data_m;
};
