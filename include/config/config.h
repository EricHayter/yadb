#pragma once

#include "spdlog/logger.h"

#include <filesystem>
#include <memory>
#include <string>

/*
 * The database config is a shared object to pass down the internal stack of
 * components to share database configuration.
 *
 * In particular the main benefit of the database config is to allow for a
 * dependency injection pattern for the loggers of each of the components.
 *
 * This class comes will some sensible default logging options but can be
 * customized further if desired.
 */
struct DatabaseConfig {
    static inline const std::filesystem::path DEFAULT_DATABASE_FILE { "data.db" };

    static inline const std::string DEFAULT_DISK_MANAGER_LOGGER_NAME { "disk_manager" };
    static inline const std::string DEFAULT_DISK_SCHEDULER_LOGGER_NAME { "disk_scheduler" };
    static inline const std::string DEFAULT_PAGE_BUFFER_MANAGER_LOGGER_NAME { "page_buffer_manager" };

    std::filesystem::path database_file { DEFAULT_DATABASE_FILE };

    std::shared_ptr<spdlog::logger> disk_manager_logger;
    std::shared_ptr<spdlog::logger> disk_scheduler_logger;
    std::shared_ptr<spdlog::logger> page_buffer_manager_logger;

    /*
     * Create database config for writing logs to console
     */
    static DatabaseConfig CreateDefaultConsole();

    /*
     * Create database config for writing logs to file
     */
    static DatabaseConfig CreateDefaultFile();

    /*
     * Create database config that logs no output anywhere
     */
    static DatabaseConfig CreateNull();
};
