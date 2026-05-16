#include "storage/on_disk/table/disk_table_factory.h"
#include "storage/on_disk/constants.h"
#include "core/assert.h"
#include "core/row_reader.h"
#include "storage/on_disk/page/page_format.h"
#include "storage/on_disk/table/disk_table.h"
#include <memory>

bool DiskTableFactory::TableExists(std::string_view table_name) const
{
    // Special case for catalog tables - check file mapping directly
    // since they exist before the catalog is set up
    if (table_name == Catalog::TABLE_CATALOG_TABLE_NAME || table_name == Catalog::COLUMN_CATALOG_TABLE_NAME) {
        return mapping_manager_m.GetFileId(table_name).has_value();
    }

    // For regular tables, use catalog if available
    auto catalog = GetCatalog();
    if (catalog.has_value()) {
        return catalog->TableExists(table_name);
    }

    // Fallback: check if file mapping exists (table was created but catalog not set yet)
    return mapping_manager_m.GetFileId(table_name).has_value();
}

bool DiskTableFactory::CreateTableFile(std::string_view table_name, std::optional<file_id_t> file_id)
{
    file_id_t actual_file_id;

    if (file_id) {
        // Use the provided file ID (for catalog tables)
        actual_file_id = *file_id;
        page_buffer_manager_m->CreateFile(actual_file_id);
    } else {
        // Generate a new file ID (for regular tables)
        actual_file_id = page_buffer_manager_m->CreateFile();
    }

    mapping_manager_m.SaveMapping(table_name, actual_file_id);

    // Initialize first page as data page
    Page page = page_buffer_manager_m->GetPage({ actual_file_id, 0 });
    std::lock_guard<Page> lk(page);
    page::InitPage(page.GetMutView(), page::PageType::Data);

    return true;
}

DiskTableFactory::DiskTableFactory()
    : page_buffer_manager_m(std::make_unique<PageBufferManager>())
    , mapping_manager_m()
{
}

DiskTableFactory::DiskTableFactory(const Catalog& catalog)
    : DiskTableFactory()
{
    SetCatalog(catalog);
}

bool DiskTableFactory::CreateTable(std::string_view table_name, const Schema& schema)
{
    // Check if this is a catalog table
    if (table_name == Catalog::TABLE_CATALOG_TABLE_NAME || table_name == Catalog::COLUMN_CATALOG_TABLE_NAME) {
        YADB_ASSERT(!GetCatalog().has_value(), "Cannot create catalog tables after catalog has been set");

        // Create catalog table with reserved file ID
        file_id_t reserved_id = (table_name == Catalog::TABLE_CATALOG_TABLE_NAME)
            ? TABLE_CATALOG_FILE_ID
            : COLUMN_CATALOG_FILE_ID;

        if (!CreateTableFile(table_name, reserved_id)) {
            return false;
        }

        // Create table object directly and initialize it with catalog metadata
        std::shared_ptr<DiskTable> table(new DiskTable(reserved_id, schema, *page_buffer_manager_m));
        if (table_name == Catalog::TABLE_CATALOG_TABLE_NAME) {
            Catalog::InitializeTableCatalogTable(*table);
        } else {
            Catalog::InitializeColumnCatalogTable(*table);
        }

        return true;
    }

    // Regular table creation - requires catalog to be set
    YADB_ASSERT(GetCatalog().has_value(), "Cannot create regular tables before catalog is set");

    if (GetCatalog()->TableExists(table_name)) {
        return false;
    }

    // Create regular table with dynamic file ID
    if (!CreateTableFile(table_name)) {
        return false;
    }

    return true;
}

std::shared_ptr<Table> DiskTableFactory::GetTable(std::string_view table_name)
{
    // Bootstrap case: catalog tables must be retrievable before the catalog is set.
    if (table_name == Catalog::TABLE_CATALOG_TABLE_NAME || table_name == Catalog::COLUMN_CATALOG_TABLE_NAME) {
        auto file_id = mapping_manager_m.GetFileId(table_name);
        if (!file_id)
            return nullptr;
        const Schema& schema = (table_name == Catalog::TABLE_CATALOG_TABLE_NAME)
            ? Catalog::table_catalog_schema
            : Catalog::column_catalog_schema;
        return std::shared_ptr<Table>(new DiskTable(*file_id, schema, *page_buffer_manager_m));
    }

    auto catalog = GetCatalog();
    if (!catalog || !catalog->TableExists(table_name)) {
        return nullptr;
    }

    auto file_id = mapping_manager_m.GetFileId(table_name);
    if (!file_id) {
        return nullptr;
    }

    Schema schema = catalog->GetSchema(table_name);
    return std::shared_ptr<Table>(new DiskTable(
        *file_id,
        schema,
        *page_buffer_manager_m
    ));
}

bool DiskTableFactory::DeleteTable(std::string_view table_name)
{
    auto file_id = mapping_manager_m.GetFileId(table_name);
    if (!file_id) {
        return false;
    }

    page_buffer_manager_m->DeleteFile(*file_id);
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

    auto iter = mapping_table_m->iter();
    for (const auto& [row_id, data] : *iter) {
        RowReader rr(data, MAPPING_SCHEMA);
        if (rr.Get<DataType::TEXT>(0) == table_name)
            return static_cast<file_id_t>(rr.Get<DataType::INTEGER>(1));
    }
    return {};
}

bool DiskTableFactory::MappingManager::SaveMapping(std::string_view table_name, file_id_t file_id)
{
    YADB_ASSERT(mapping_table_m != nullptr, "Mapping table must be initialized before saving non-reserved table mappings");

    mapping_table_m->insert_row({
        Value(std::string(table_name)),
        Value(static_cast<std::int32_t>(file_id)),
    });
    return true;
}

bool DiskTableFactory::MappingManager::DeleteMapping(std::string_view table_name)
{
    YADB_ASSERT(!GetReservedFileId(table_name).has_value(),
        "Cannot delete mappings for reserved catalog tables");

    YADB_ASSERT(mapping_table_m != nullptr, "Mapping table must be initialized before deleting non-reserved table mappings");

    std::optional<row_id_t> found_rid;
    {
        auto iter = mapping_table_m->iter();
        for (const auto& [row_id, data] : *iter) {
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
