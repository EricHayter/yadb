#pragma once

#include "table/table.h"
#include "storage/on_disk/buffer_manager/page_buffer_manager.h"
#include <filesystem>

class DiskTable : public Table {
    friend class DiskTableManager;

public:
    static bool CreateTable(std::string_view table_name, PageBufferManager& page_buffer_manager);
    static std::shared_ptr<DiskTable> GetTable(std::string_view table_name, const Schema& schema, PageBufferManager& page_buffer_manager);

    ~DiskTable() override = default;

    std::unique_ptr<TableIterator> iter() override;
    row_id_t update_row(const row_id_t& rid, std::span<const std::byte> data) override;
    void delete_row(const row_id_t& rid) override;

    TableType GetType() const override;

protected:
    row_id_t insert_row_impl(std::span<const std::byte> row) override;

private:
    static std::filesystem::path GetTableFileName(std::string_view table_name);

private:
    DiskTable(file_id_t file_id, const Schema& schema, PageBufferManager& page_buffer_manager);
    file_id_t file_id_m;
    PageBufferManager& page_buffer_manager_m;
};
