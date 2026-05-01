#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <shared_mutex>
#include <fstream>
#include "catalog/catalog.h"
#include "storage/on_disk/table/disk_table.h"
#include "storage/on_disk/buffer_manager/page_buffer_manager.h"

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

class DiskTableManager {
    public:
    DiskTableManager(PageBufferManager& page_buffer_manager, Catalog& catalog);
    bool CreateTable(std::string_view table_name, const Schema& schema);
    std::shared_ptr<DiskTable> GetTable(std::string_view table_name);

    private:
    /* Class to manage the mapping of table names to file_ids */
    class MappingManager {
        public:
        const std::string MAPPING_FILE_NAME = "disk_table_mapping.bin";

        MappingManager();
        std::optional<file_id_t> GetFileId(std::string_view table_name) const;
        bool SaveMapping(std::string_view table_name, file_id_t file_id);

        private:
        std::shared_mutex mut_m;
        std::fstream fstream_m;
        std::unordered_map<std::string, file_id_t> file_id_map_m;
    };

    PageBufferManager& page_buffer_manager_m;
    Catalog& catalog_m;
    MappingManager mapping_manager_m;
};
