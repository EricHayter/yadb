#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <shared_mutex>
#include <fstream>
#include <optional>
#include "storage/on_disk/types.h"
#include "storage/on_disk/buffer_manager/page_buffer_manager.h"
#include "table/table_factory_interface.h"

/*
 * Disk table manager
 * The current implementation of the disk manager simply operates on file ids
 * which trivial map to file paths. This is good since it simplifies the
 * interface of the disk manager greatly and performs signficantly faster
 * as there isn't any string comparisons when performing operations.
 *
 * That being said if that management isn't being done at the disk manager level
 * it needs to be done somewhere of course! So I'm putting it here which will
 * sit directly above the disk table interface. Idea here is at the table
 * interface level we just call create/get with a table name and this does
 * the translation to a file id with some sort of mapping.
 *
 * For persistence we will have a slotted page to hold these mappings from table
 * name to file_id.
 */

class DiskTableFactory : public ITableFactory {
    public:
    // Default constructor - use SetCatalog() to attach a catalog later.
    // Use this when the catalog is not available at factory construction time.
    DiskTableFactory(PageBufferManager& page_buffer_manager);

    // Constructor with catalog - attaches the catalog immediately.
    // Use this when the catalog is available at factory construction time.
    DiskTableFactory(PageBufferManager& page_buffer_manager, const Catalog& catalog);

    bool TableExists(std::string_view table_name) const override;
    bool CreateTable(std::string_view table_name, const Schema& schema) override;
    std::shared_ptr<Table> GetTable(std::string_view table_name) override;
    bool DeleteTable(std::string_view table_name) override;

    private:
    /* Class to manage the mapping of table names to file_ids */
    class MappingManager {
        public:
        MappingManager();
        std::optional<file_id_t> GetFileId(std::string_view table_name) const;
        bool SaveMapping(std::string_view table_name, file_id_t file_id);

        private:
        mutable std::shared_mutex mut_m;
        std::fstream fstream_m;
        std::unordered_map<std::string, file_id_t> file_id_map_m;
    };

    bool CreateTableFile(std::string_view table_name, std::optional<file_id_t> file_id = {});

    PageBufferManager& page_buffer_manager_m;
    MappingManager mapping_manager_m;
};
