#include "storage/on_disk/table/disk_table_factory.h"
#include "catalog/catalog.h"
#include "storage/on_disk/constants.h"
#include "core/assert.h"
#include "core/row_builder.h"
#include "core/row_reader.h"
#include "storage/on_disk/page/page_format.h"
#include "storage/on_disk/table/disk_table.h"
#include <memory>

bool DiskTableFactory::TableExists(std::string_view table_name) const
{
    return mapping_manager_m.GetFileId(table_name).has_value();
}

bool DiskTableFactory::CreateTableFile(std::string_view table_name, std::optional<file_id_t> file_id)
{
    file_id_t actual_file_id;

    if (file_id) {
        actual_file_id = *file_id;
        auto err = page_buffer_manager_m->CreateFile(actual_file_id);
        YADB_ASSERT(!err.has_value(), (*err)->what().c_str());
    } else {
        auto result = page_buffer_manager_m->CreateFile();
        YADB_ASSERT(result.has_value(), result.error()->what().c_str());
        actual_file_id = *result;
    }

    mapping_manager_m.SaveMapping(table_name, actual_file_id);

    auto page_result = page_buffer_manager_m->GetPage({ actual_file_id, 0 });
    YADB_ASSERT(page_result.has_value(), page_result.error()->what().c_str());
    Page page = std::move(*page_result);
    std::lock_guard<Page> lk(page);
    page::InitPage(page.GetMutView(), page::PageType::Data);

    return true;
}

DiskTableFactory::DiskTableFactory()
    : page_buffer_manager_m(std::make_unique<PageBufferManager>())
    , mapping_manager_m()
{
}

bool DiskTableFactory::CreateTable(std::string_view table_name)
{
    if (table_name == Catalog::TABLE_CATALOG_TABLE_NAME || table_name == Catalog::COLUMN_CATALOG_TABLE_NAME) {
        file_id_t reserved_id = (table_name == Catalog::TABLE_CATALOG_TABLE_NAME)
            ? TABLE_CATALOG_FILE_ID
            : COLUMN_CATALOG_FILE_ID;
        return CreateTableFile(table_name, reserved_id);
    }

    return CreateTableFile(table_name);
}

std::shared_ptr<Table> DiskTableFactory::GetTable(std::string_view table_name)
{
    auto file_id = mapping_manager_m.GetFileId(table_name);
    if (!file_id)
        return nullptr;
    return std::shared_ptr<Table>(new DiskTable(*file_id, *page_buffer_manager_m));
}

bool DiskTableFactory::DeleteTable(std::string_view table_name)
{
    auto file_id = mapping_manager_m.GetFileId(table_name);
    if (!file_id) {
        return false;
    }

    auto err = page_buffer_manager_m->DeleteFile(*file_id);
    YADB_ASSERT(!err.has_value(), (*err)->what().c_str());
    mapping_manager_m.DeleteMapping(table_name);

    return true;
}

std::optional<file_id_t> DiskTableFactory::MappingManager::GetReservedFileId(std::string_view table_name)
{
    if (table_name == Catalog::TABLE_CATALOG_TABLE_NAME)
        return TABLE_CATALOG_FILE_ID;
    if (table_name == Catalog::COLUMN_CATALOG_TABLE_NAME)
        return COLUMN_CATALOG_FILE_ID;
    return {};
}

std::optional<file_id_t> DiskTableFactory::MappingManager::GetFileId(std::string_view table_name) const
{
    if (auto reserved = GetReservedFileId(table_name))
        return reserved;

    YADB_ASSERT(mapping_table_m != nullptr, "Mapping table must be initialized before looking up non-reserved table names");

    for (auto [row_id, data] : *mapping_table_m) {
        RowReader rr(data, MAPPING_SCHEMA);
        if (rr.Get<DataType::TEXT>(0) == table_name)
            return static_cast<file_id_t>(rr.Get<DataType::INTEGER>(1));
    }
    return {};
}

bool DiskTableFactory::MappingManager::SaveMapping(std::string_view table_name, file_id_t file_id)
{
    YADB_ASSERT(mapping_table_m != nullptr, "Mapping table must be initialized before saving non-reserved table mappings");

    RowBuilder rb;
    rb.Push<DataType::TEXT>(table_name);
    rb.Push<DataType::INTEGER>(static_cast<std::int32_t>(file_id));
    mapping_table_m->insert_row(rb.Data());
    return true;
}

bool DiskTableFactory::MappingManager::DeleteMapping(std::string_view table_name)
{
    YADB_ASSERT(!GetReservedFileId(table_name).has_value(),
        "Cannot delete mappings for reserved catalog tables");

    YADB_ASSERT(mapping_table_m != nullptr, "Mapping table must be initialized before deleting non-reserved table mappings");

    std::optional<row_id_t> found_rid;
    {
        for (auto [row_id, data] : *mapping_table_m) {
            RowReader rr(data, MAPPING_SCHEMA);
            if (rr.Get<DataType::TEXT>(0) == table_name) {
                found_rid = row_id;
                break;
            }
        }
    }
    if (found_rid) {
        mapping_table_m->delete_row(*found_rid);
        return true;
    }
    return false;
}
