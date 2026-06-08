#pragma once

#include "core/error.h"
#include "storage/on_disk/types.h"
#include <format>
#include <string>

namespace yadb {

class GetPageError : public Error {
public:
    GetPageError(file_page_id_t fp_id, std::string_view reason)
        : fp_id_m(fp_id)
        , reason_m(reason)
    {
    }

    std::string what() const override
    {
        return std::format("failed to get page {} in file {}: {}",
            fp_id_m.page_id, fp_id_m.file_id, reason_m);
    }

    file_page_id_t FilePageId() const noexcept { return fp_id_m; }

private:
    file_page_id_t fp_id_m;
    std::string reason_m;
};

class FlushPageError : public Error {
public:
    FlushPageError(file_page_id_t fp_id, std::string_view reason)
        : fp_id_m(fp_id)
        , reason_m(reason)
    {
    }

    std::string what() const override
    {
        return std::format("failed to flush page {} in file {}: {}",
            fp_id_m.page_id, fp_id_m.file_id, reason_m);
    }

    file_page_id_t FilePageId() const noexcept { return fp_id_m; }

private:
    file_page_id_t fp_id_m;
    std::string reason_m;
};

class DeleteFileTimeoutError : public Error {
public:
    explicit DeleteFileTimeoutError(file_id_t file_id)
        : file_id_m(file_id)
    {
    }

    std::string what() const override
    {
        return std::format("timed out waiting for pinned pages in file {} to be released", file_id_m);
    }

    file_id_t FileId() const noexcept { return file_id_m; }

private:
    file_id_t file_id_m;
};

} // namespace yadb
