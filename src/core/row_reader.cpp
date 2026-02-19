#include "core/row_reader.h"
#include "common/definitions.h"
#include "core/assert.h"
#include "format"
#include <cstring>

RowReader::RowReader(std::span<const std::byte> data, const Schema& schema)
    : data_m{ data }
    , schema_m{ schema }
{}

std::size_t RowReader::CalculateOffset(size_t pos) {
    YADB_ASSERT(pos < NumValues(),
        std::format("Row position offset ({}) is out of range (row has {} values)", pos, NumValues()).c_str()
    );
    std::size_t offset = 0;
    for (std::size_t i = 0; i < pos; i++) {
        switch (schema_m[i].type) {
        case DataType::INTEGER: {
            offset += sizeof(type_for<DataType::INTEGER>);
            break;
        }
        case DataType::TEXT: {
            string_length_t str_len;
            std::memcpy(&str_len, data_m.data() + offset, sizeof(string_length_t));
            offset += sizeof(string_length_t) + str_len;
            break;
        }
        default: {
            YADB_ASSERT(false,
                std::format("Cannot pop value from row reader: unimplemented DataType {}",
                    ToString(schema_m[i].type)
                ).c_str()
            );
        }
        }
    }

    return offset;
}
