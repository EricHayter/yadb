#pragma once

#include "table/table_manager.h"

class BPTreeTableManager : public TableManager {
    public:
    ~BPTreeTableManager() = default;
    void CreateTable(Catalog& catalog, std::string_view name, const Schema& schema) override;
    void DeleteTable(Catalog& catalog, std::string_view name) override;
    virtual bool TableExists(std::string_view name) override;
    virtual std::unique_ptr<Table> GetTable(std::string_view name) override;
};
