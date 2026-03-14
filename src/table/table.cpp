#include "table/table.h"
#include "core/row_builder.h"
#include <format>
#include <stdexcept>

Table::Table(const Schema& schema) : schema_m(schema) {}

Table::~Table() = default;

row_id_t Table::insert_row(const std::vector<Value>& values) {
    // Validate value count
    if (values.size() != schema_m.size()) {
        throw std::runtime_error(
            std::format("Value count mismatch: expected {}, got {}",
                schema_m.size(), values.size())
        );
    }

    // Build row with type validation
    RowBuilder builder;
    for (size_t i = 0; i < values.size(); ++i) {
        const auto& value = values[i];
        const auto expected_type = schema_m[i].type;

        std::visit([&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            constexpr DataType actual_type = enum_for<T>;

            if (actual_type != expected_type) {
                throw std::runtime_error(
                    std::format("Type mismatch at column {}: expected {}, got {}",
                        i, ToString(expected_type), ToString(actual_type))
                );
            }

            builder.Push<actual_type>(v);
        }, value);
    }

    // Delegate to subclass implementation
    return insert_row_impl(builder.Data());
}

const Schema& Table::GetSchema() const {
    return schema_m;
}
