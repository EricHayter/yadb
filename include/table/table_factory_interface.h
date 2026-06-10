#pragma once

#include "table/table.h"
#include <memory>
#include <string_view>

class ITableFactory {
public:
    virtual ~ITableFactory() = default;

    virtual bool TableExists(std::string_view table_name) const = 0;
    virtual bool CreateTable(std::string_view table_name) = 0;
    virtual std::shared_ptr<Table> GetTable(std::string_view table_name) = 0;
    virtual bool DeleteTable(std::string_view table_name) = 0;
};
