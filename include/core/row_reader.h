#pragma once

#include "common/definitions.h"
#include "catalog/catalog.h"
#include <cstring>
#include <format>

class RowReader {
    public:
    RowReader(std::span<const std::byte> data, const Schema& schema);

    template<DataType T>
    type_for<T> Get(std::size_t pos);

    // TODO maybe create an iterator for more efficient reading if there's lots
    // of variable length types in the row. Not super high priority I don't
    // think it will have a massive difference.

    std::size_t NumValues() const { return schema_m.size(); }

    private:
    // Calculates the offset of the pos inside of the row based on the schema
    // information.
    std::size_t CalculateOffset(size_t pos);

    const Schema& schema_m;
    std::span<const std::byte> data_m;
};

template<DataType T>
type_for<T> RowReader::Get(std::size_t pos) {
    YADB_ASSERT(T == schema_m[pos].type,
        std::format("Popped type does not conform with schema. Expected: {}, Actual: {}",
            ToString(T),
            ToString(schema_m[pos].type)
        ).c_str()
    );

    type_for<T> popped;
    std::size_t offset = CalculateOffset(pos);
    switch (T) {
    case DataType::INTEGER: {
        std::memcpy(&popped, data_m.data() + offset, sizeof(popped));
        break;
    }
    case DataType::TEXT: {
        string_length_t str_len;
        std::memcpy(&str_len, data_m.data() + offset, sizeof(string_length_t));
        offset += sizeof(string_length_t);
        popped = std::string((char*)(data_m.data() + offset), str_len);
        break;
    }
    }
    return popped;
}
