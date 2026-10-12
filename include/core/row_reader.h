#pragma once

#include "catalog/catalog.h"
#include "common/definitions.h"
#include "core/assert.h"
#include <format>

class RowReader {
public:
    RowReader(std::span<const std::byte> data, const Schema& schema);

    template <DataType T>
    T Get(std::size_t pos);

    // Reads the value at pos as whatever type the schema says it is.
    Value GetValue(std::size_t pos);

    // TODO maybe create an iterator for more efficient reading if there's lots
    // of variable length types in the row. Not super high priority I don't
    // think it will have a massive difference.

    std::size_t NumValues() const { return schema_m.size(); }

    // Low-level accessors for efficient field copying (e.g., in projection)
    std::size_t GetOffset(std::size_t pos);
    std::size_t GetSize(std::size_t pos);
    std::span<const std::byte> GetRawData() const { return data_m; }

private:
    // Calculates the offset of the pos inside of the row based on the schema
    // information.
    std::size_t CalculateOffset(size_t pos);
    // Size of the encoded value of the given type at the start of offset.
    std::size_t SizeAt(TypeId type, std::size_t offset);

    const Schema& schema_m;
    std::span<const std::byte> data_m;
};

template <DataType T>
T RowReader::Get(std::size_t pos)
{
    YADB_ASSERT(type_id<T> == schema_m[pos].type,
        std::format("Popped type does not conform with schema. Expected: {}, Actual: {}",
            T::type_str,
            TypeName(schema_m[pos].type))
            .c_str());

    auto value = T::from_bytes(data_m.subspan(CalculateOffset(pos)));
    YADB_ASSERT(value.has_value(),
        std::format("Failed to read {} at position {}: {}", T::type_str, pos, value ? "" : value.error()->what()).c_str());
    return std::move(*value);
}
