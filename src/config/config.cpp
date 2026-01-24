#include "config/config.h"
#include <memory>
#include "spdlog/logger.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

DatabaseConfig DatabaseConfig::CreateDefaultConsole()
{
    DatabaseConfig config;
    config.disk_manager_logger = spdlog::stderr_color_mt(DEFAULT_DISK_MANAGER_LOGGER_NAME);
    config.disk_scheduler_logger = spdlog::stderr_color_mt(DEFAULT_DISK_SCHEDULER_LOGGER_NAME);
    config.page_buffer_manager_logger = spdlog::stderr_color_mt(DEFAULT_PAGE_BUFFER_MANAGER_LOGGER_NAME);
    return config;
}

DatabaseConfig DatabaseConfig::CreateDefaultFile()
{
    DatabaseConfig config;
    config.disk_manager_logger = spdlog::basic_logger_mt(DEFAULT_DISK_MANAGER_LOGGER_NAME, DEFAULT_DISK_MANAGER_LOGGER_NAME + ".log");
    config.disk_scheduler_logger = spdlog::basic_logger_mt(DEFAULT_DISK_SCHEDULER_LOGGER_NAME, DEFAULT_DISK_SCHEDULER_LOGGER_NAME + ".log");
    config.page_buffer_manager_logger = spdlog::basic_logger_mt(DEFAULT_PAGE_BUFFER_MANAGER_LOGGER_NAME, DEFAULT_PAGE_BUFFER_MANAGER_LOGGER_NAME + ".log");
    return config;
}

DatabaseConfig DatabaseConfig::CreateNull()
{
    DatabaseConfig config;
    config.disk_manager_logger = std::make_shared<spdlog::logger>(DEFAULT_DISK_MANAGER_LOGGER_NAME);
    config.disk_scheduler_logger = std::make_shared<spdlog::logger>(DEFAULT_DISK_SCHEDULER_LOGGER_NAME);
    config.page_buffer_manager_logger = std::make_shared<spdlog::logger>(DEFAULT_PAGE_BUFFER_MANAGER_LOGGER_NAME);
    return config;
}
