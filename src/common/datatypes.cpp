#include "common/datatypes.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstring>
#include <format>
#include <limits>
#include <memory>

namespace {

yadb::Error::Ptr MakeTypeError(std::string reason)
{
    return std::make_shared<TypeError>(std::move(reason));
}

// Fixed width types are stored as their native in-memory representation.
template <DataType T>
std::expected<T, yadb::Error::Ptr> FixedFromBytes(std::span<const std::byte> buffer)
{
    T result;
    if (buffer.size() < sizeof(result.value))
        return std::unexpected(MakeTypeError(std::format("not enough bytes to read {}: need {}, have {}",
            T::type_str, sizeof(result.value), buffer.size())));
    std::memcpy(&result.value, buffer.data(), sizeof(result.value));
    return result;
}

template <DataType T>
std::optional<yadb::Error::Ptr> FixedToBytes(const T& t, std::span<std::byte> buffer)
{
    if (buffer.size() < sizeof(t.value))
        return MakeTypeError(std::format("not enough space to write {}: need {}, have {}",
            T::type_str, sizeof(t.value), buffer.size()));
    std::memcpy(buffer.data(), &t.value, sizeof(t.value));
    return std::nullopt;
}

template <DataType T>
std::expected<T, yadb::Error::Ptr> NumberFromString(std::string_view str_repr)
{
    T result;
    const char* end = str_repr.data() + str_repr.size();
    auto [ptr, ec] = std::from_chars(str_repr.data(), end, result.value);
    if (ec == std::errc::result_out_of_range)
        return std::unexpected(MakeTypeError(std::format("'{}' is out of range for {}", str_repr, T::type_str)));
    if (ec != std::errc {} || ptr != end)
        return std::unexpected(MakeTypeError(std::format("'{}' is not a valid {}", str_repr, T::type_str)));
    return result;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b)
{
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

} // namespace

/* String */

std::expected<String, yadb::Error::Ptr> String::from_bytes(std::span<const std::byte> buffer)
{
    auto size = serialized_size(buffer);
    if (!size)
        return std::unexpected(size.error());
    if (buffer.size() < *size)
        return std::unexpected(MakeTypeError(std::format("not enough bytes to read String: need {}, have {}",
            *size, buffer.size())));
    return String { std::string(reinterpret_cast<const char*>(buffer.data() + sizeof(length_t)), *size - sizeof(length_t)) };
}

std::optional<yadb::Error::Ptr> String::to_bytes(std::span<std::byte> buffer) const
{
    if (value.size() > std::numeric_limits<length_t>::max())
        return MakeTypeError(std::format("String of length {} exceeds the maximum length {}",
            value.size(), std::numeric_limits<length_t>::max()));
    if (buffer.size() < serialized_size())
        return MakeTypeError(std::format("not enough space to write String: need {}, have {}",
            serialized_size(), buffer.size()));

    length_t length = static_cast<length_t>(value.size());
    std::memcpy(buffer.data(), &length, sizeof(length));
    std::memcpy(buffer.data() + sizeof(length), value.data(), value.size());
    return std::nullopt;
}

std::size_t String::serialized_size() const
{
    return sizeof(length_t) + value.size();
}

std::expected<std::size_t, yadb::Error::Ptr> String::serialized_size(std::span<const std::byte> buffer)
{
    length_t length;
    if (buffer.size() < sizeof(length))
        return std::unexpected(MakeTypeError("not enough bytes to read String length"));
    std::memcpy(&length, buffer.data(), sizeof(length));
    return sizeof(length) + length;
}

std::expected<String, yadb::Error::Ptr> String::from_string(std::string_view str_repr)
{
    if (str_repr.size() > std::numeric_limits<length_t>::max())
        return std::unexpected(MakeTypeError(std::format("String of length {} exceeds the maximum length {}",
            str_repr.size(), std::numeric_limits<length_t>::max())));
    return String { std::string(str_repr) };
}

std::string String::to_string() const
{
    return value;
}

/* Fixed width numeric types */

#define YADB_DEFINE_NUMERIC_TYPE(T)                                                           \
    std::expected<T, yadb::Error::Ptr> T::from_bytes(std::span<const std::byte> buffer)       \
    {                                                                                         \
        return FixedFromBytes<T>(buffer);                                                     \
    }                                                                                         \
    std::optional<yadb::Error::Ptr> T::to_bytes(std::span<std::byte> buffer) const            \
    {                                                                                         \
        return FixedToBytes(*this, buffer);                                                   \
    }                                                                                         \
    std::expected<T, yadb::Error::Ptr> T::from_string(std::string_view str_repr)              \
    {                                                                                         \
        return NumberFromString<T>(str_repr);                                                 \
    }                                                                                         \
    std::string T::to_string() const                                                          \
    {                                                                                         \
        return std::format("{}", value);                                                      \
    }

YADB_DEFINE_NUMERIC_TYPE(I32)
YADB_DEFINE_NUMERIC_TYPE(U32)
YADB_DEFINE_NUMERIC_TYPE(I64)
YADB_DEFINE_NUMERIC_TYPE(F64)

#undef YADB_DEFINE_NUMERIC_TYPE

/* Runtime type helpers */

std::string_view TypeName(TypeId id)
{
    if (!IsValidTypeId(id))
        return "<invalid type>";
    return dispatch_type(id, []<DataType T>() { return T::type_str; });
}

TypeId TypeOf(const Value& value)
{
    return static_cast<TypeId>(value.index());
}

std::optional<TypeId> TypeFromName(std::string_view name)
{
    // SQL style aliases kept for compatibility with existing queries
    static constexpr std::array<std::pair<std::string_view, TypeId>, 5> aliases { {
        { "INTEGER", type_id<I32> },
        { "INT", type_id<I32> },
        { "BIGINT", type_id<I64> },
        { "DOUBLE", type_id<F64> },
        { "TEXT", type_id<String> },
    } };

    for (TypeId id = 0; IsValidTypeId(id); id++) {
        if (EqualsIgnoreCase(name, TypeName(id)))
            return id;
    }
    for (const auto& [alias, id] : aliases) {
        if (EqualsIgnoreCase(name, alias))
            return id;
    }
    return std::nullopt;
}

std::expected<Value, yadb::Error::Ptr> ValueFromString(TypeId id, std::string_view str_repr)
{
    if (!IsValidTypeId(id))
        return std::unexpected(MakeTypeError(std::format("invalid type id {}", id)));
    return dispatch_type(id, [&]<DataType T>() -> std::expected<Value, yadb::Error::Ptr> {
        return T::from_string(str_repr);
    });
}

std::string ToString(const Value& value)
{
    return std::visit([](const DataType auto& v) { return v.to_string(); }, value);
}
