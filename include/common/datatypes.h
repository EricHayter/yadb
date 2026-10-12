#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "core/assert.h"
#include "core/error.h"

template <typename T>
concept StringConvertible = requires(const T t, std::string_view str_repr) {
    { T::from_string(str_repr) } -> std::convertible_to<std::expected<T, yadb::Error::Ptr>>;
    { t.to_string() } -> std::convertible_to<std::string>;
};

// serialized_size() is the number of bytes to_bytes() will write, and the
// static serialized_size(buffer) is the number of bytes the encoded value at
// the front of buffer occupies (needed to walk variable length rows).
template <typename T>
concept Serializable = requires(const T t, std::span<const std::byte> buffer, std::span<std::byte> mut_buffer) {
    { T::from_bytes(buffer) } -> std::convertible_to<std::expected<T, yadb::Error::Ptr>>;
    { t.to_bytes(mut_buffer) } -> std::convertible_to<std::optional<yadb::Error::Ptr>>;
    { t.serialized_size() } -> std::convertible_to<std::size_t>;
    { T::serialized_size(buffer) } -> std::convertible_to<std::expected<std::size_t, yadb::Error::Ptr>>;
};

template <typename T>
concept DataType = StringConvertible<T> && Serializable<T> && std::totally_ordered<T> && requires(T t) {
    t.value;
    { T::type_str } -> std::convertible_to<std::string_view>;
};

class TypeError : public yadb::Error {
public:
    explicit TypeError(std::string reason)
        : reason_m(std::move(reason))
    {
    }

    std::string what() const override { return reason_m; }

private:
    std::string reason_m;
};

struct String {
    static constexpr std::string_view type_str = "String";
    // strings are stored as a length prefix followed by the raw characters
    using length_t = std::uint16_t;

    std::string value;

    static std::expected<String, yadb::Error::Ptr> from_bytes(std::span<const std::byte> buffer);
    std::optional<yadb::Error::Ptr> to_bytes(std::span<std::byte> buffer) const;
    std::size_t serialized_size() const;
    static std::expected<std::size_t, yadb::Error::Ptr> serialized_size(std::span<const std::byte> buffer);

    static std::expected<String, yadb::Error::Ptr> from_string(std::string_view str_repr);
    std::string to_string() const;

    auto operator<=>(const String&) const = default;
};
static_assert(DataType<String>);

struct I32 {
    static constexpr std::string_view type_str = "I32";

    std::int32_t value;

    static std::expected<I32, yadb::Error::Ptr> from_bytes(std::span<const std::byte> buffer);
    std::optional<yadb::Error::Ptr> to_bytes(std::span<std::byte> buffer) const;
    std::size_t serialized_size() const { return sizeof(value); }
    static std::expected<std::size_t, yadb::Error::Ptr> serialized_size(std::span<const std::byte>) { return sizeof(value); }

    static std::expected<I32, yadb::Error::Ptr> from_string(std::string_view str_repr);
    std::string to_string() const;

    auto operator<=>(const I32&) const = default;
};
static_assert(DataType<I32>);

struct U32 {
    static constexpr std::string_view type_str = "U32";

    std::uint32_t value;

    static std::expected<U32, yadb::Error::Ptr> from_bytes(std::span<const std::byte> buffer);
    std::optional<yadb::Error::Ptr> to_bytes(std::span<std::byte> buffer) const;
    std::size_t serialized_size() const { return sizeof(value); }
    static std::expected<std::size_t, yadb::Error::Ptr> serialized_size(std::span<const std::byte>) { return sizeof(value); }

    static std::expected<U32, yadb::Error::Ptr> from_string(std::string_view str_repr);
    std::string to_string() const;

    auto operator<=>(const U32&) const = default;
};
static_assert(DataType<U32>);

struct I64 {
    static constexpr std::string_view type_str = "I64";

    std::int64_t value;

    static std::expected<I64, yadb::Error::Ptr> from_bytes(std::span<const std::byte> buffer);
    std::optional<yadb::Error::Ptr> to_bytes(std::span<std::byte> buffer) const;
    std::size_t serialized_size() const { return sizeof(value); }
    static std::expected<std::size_t, yadb::Error::Ptr> serialized_size(std::span<const std::byte>) { return sizeof(value); }

    static std::expected<I64, yadb::Error::Ptr> from_string(std::string_view str_repr);
    std::string to_string() const;

    auto operator<=>(const I64&) const = default;
};
static_assert(DataType<I64>);

struct F64 {
    static constexpr std::string_view type_str = "F64";

    double value;

    static std::expected<F64, yadb::Error::Ptr> from_bytes(std::span<const std::byte> buffer);
    std::optional<yadb::Error::Ptr> to_bytes(std::span<std::byte> buffer) const;
    std::size_t serialized_size() const { return sizeof(value); }
    static std::expected<std::size_t, yadb::Error::Ptr> serialized_size(std::span<const std::byte>) { return sizeof(value); }

    static std::expected<F64, yadb::Error::Ptr> from_string(std::string_view str_repr);
    std::string to_string() const;

    auto operator<=>(const F64&) const = default;
};
static_assert(DataType<F64>);

// A value of any of the supported types. The index of an alternative is its
// TypeId, which is persisted in the catalog, so only ever append new types to
// the end of this list.
using Value = std::variant<I32, String, U32, I64, F64>;

/* Verify that everything in our Value variant is in fact a datatype.
 *
 * This is really ugly I know but there's really not any other good options to
 * iterate through all of the alternatives of a template pack unfortunately
 * other than this insane insane index_sequence crap. C++26 has template for
 * expansion which would solve this I believe but it's not available with
 * GCC 14.
 *
 * In english terms what this is doing is the following:
 * We have this "Index-sequence" parameter this is essentially just a parameter
 * pack of numbers from 0 to # of variants - 1 (inclusive), then for each of
 * these indices of the pack, we get the datatype of the alternative at 
 * that index using std::variant_alternative_t, we then check to see if that
 * type at that index satisfies the DataType constraint.
 * */
static_assert([]<std::size_t... Is>(std::index_sequence<Is...>) {
    return (DataType<std::variant_alternative_t<Is, Value>> && ...);
}(std::make_index_sequence<std::variant_size_v<Value>>()),
"Variant alternatives of Value should all satisfy the \"DataType\" concept");

using TypeId = std::uint8_t;

namespace detail {
template <typename T, typename V>
struct VariantIndex;

template <typename T, typename... Ts>
struct VariantIndex<T, std::variant<Ts...>> {
    static constexpr std::size_t value = [] {
        constexpr bool matches[] = { std::is_same_v<T, Ts>... };
        for (std::size_t i = 0; i < sizeof...(Ts); i++)
            if (matches[i])
                return i;
        return sizeof...(Ts);
    }();
    static_assert(value < sizeof...(Ts), "type is not an alternative of Value");
};
}

template <DataType T>
constexpr TypeId type_id = static_cast<TypeId>(detail::VariantIndex<T, Value>::value);

// Takes any integral type so ids read back as a wider type (e.g. the I32 stored
// in the catalog) can be checked before narrowing them to a TypeId.
constexpr bool IsValidTypeId(std::integral auto id)
{
    return std::cmp_greater_equal(id, 0) && std::cmp_less(id, std::variant_size_v<Value>);
}

/* This is extremely ugly I hate it with a passion. C++26 should have something
 * for this with something like template for ()... or whatever but not
 * available in GCC 14.
 *
 * idea is given a function that is templated on a given type we can use a
 * run-time type (TypeId) to return the function that we want. For instance 
 * for each of our datatypes we have a from_string method that is a static
 * function per class that is a DataType. Then what we can do is based
 * on the run-time type information we can resolve the function we need */
template<typename F>
auto dispatch_type(TypeId id, F&& f)
{
    YADB_ASSERT(IsValidTypeId(id), "dispatch_type called with an invalid TypeId");
    using R = decltype(f.template operator()<std::variant_alternative_t<0, Value>>());
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) -> R {
        if constexpr (std::is_void_v<R>) {
            (void)((id == Is ? (f.template operator()<std::variant_alternative_t<Is, Value>>(), true) : false) || ...);
        } else {
            std::optional<R> result;
            (void)((id == Is ? (result.emplace(f.template operator()<std::variant_alternative_t<Is, Value>>()), true) : false) || ...);
            return std::move(*result);
        }
    }(std::make_index_sequence<std::variant_size_v<Value>>());
}

std::string_view TypeName(TypeId id);
TypeId TypeOf(const Value& value);

std::optional<TypeId> TypeFromName(std::string_view name);

// Parses the textual representation of a value as the given type.
std::expected<Value, yadb::Error::Ptr> ValueFromString(TypeId id, std::string_view str_repr);
std::string ToString(const Value& value);
