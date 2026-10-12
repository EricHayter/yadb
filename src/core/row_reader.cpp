#include "core/row_reader.h"
#include "common/definitions.h"
#include "core/assert.h"
#include "format"

RowReader::RowReader(std::span<const std::byte> data, const Schema& schema)
    : schema_m { schema }
    , data_m { data }
{
}

std::size_t RowReader::SizeAt(TypeId type, std::size_t offset)
{
    auto size = dispatch_type(type, [&]<DataType T>() {
        return T::serialized_size(data_m.subspan(offset));
    });
    YADB_ASSERT(size.has_value(),
        std::format("Cannot read size of {} at offset {}: {}", TypeName(type), offset, size ? "" : size.error()->what()).c_str());
    return *size;
}

std::size_t RowReader::CalculateOffset(size_t pos)
{
    YADB_ASSERT(pos < NumValues(),
        std::format("Row position offset ({}) is out of range (row has {} values)", pos, NumValues()).c_str());
    std::size_t offset = 0;
    for (std::size_t i = 0; i < pos; i++)
        offset += SizeAt(schema_m[i].type, offset);

    return offset;
}

std::size_t RowReader::GetOffset(std::size_t pos)
{
    return CalculateOffset(pos);
}

std::size_t RowReader::GetSize(std::size_t pos)
{
    YADB_ASSERT(pos < NumValues(),
        std::format("Row position ({}) is out of range (row has {} values)", pos, NumValues()).c_str());

    return SizeAt(schema_m[pos].type, CalculateOffset(pos));
}

Value RowReader::GetValue(std::size_t pos)
{
    return dispatch_type(schema_m[pos].type, [&]<DataType T>() -> Value {
        return Get<T>(pos);
    });
}
