#pragma once

#include "Types.h"

#include <optional>
#include <string>
#include <vector>

namespace motioncam {

class IVirtualFileSystem {
public:
    virtual ~IVirtualFileSystem() = default;

    IVirtualFileSystem(const IVirtualFileSystem&) = delete;
    IVirtualFileSystem& operator=(const IVirtualFileSystem&) = delete;

    virtual const std::vector<Entry>& listFiles(const std::string& filter) const = 0;
    virtual std::optional<Entry> findEntry(const std::string& filter) const = 0;
    virtual int readFile(const Entry& entry, FileRenderOptions options, const size_t pos, const size_t len, void* dst) const = 0;
    virtual void updateOptions(FileRenderOptions options) = 0;

protected:
    IVirtualFileSystem() = default;
};

}
