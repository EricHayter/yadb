#include "storage/disk/disk_manager.h"

DiskManager::DiskManager(const std::filesystem::path& db_file)
{
    db_io_m = std::fstream(db_file);
    // TODO check to see if the open fails
}
