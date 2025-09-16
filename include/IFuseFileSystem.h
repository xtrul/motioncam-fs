#pragma once

#include <string>
#include <optional>

#include "Types.h"

namespace motioncam {

using MountId = int;

constexpr auto InvalidMountId = -1;

struct FileInfo {
    float fps;
    int totalFrames;
    int droppedFrames;
    int width;
    int height;
};

class IFuseFileSystem {
public:
    virtual ~IFuseFileSystem() = default;

    IFuseFileSystem(const IFuseFileSystem&) = delete;
    IFuseFileSystem& operator=(const IFuseFileSystem&) = delete;

    virtual MountId mount(FileRenderOptions options, int draftScale, const std::string& srcFile, const std::string& dstPath, const std::string& customCameraModel = "") = 0;
    virtual void unmount(MountId mountId) = 0;
    virtual void updateOptions(MountId mountId, FileRenderOptions options, int draftScale, const std::string& customCameraModel = "") = 0;
    virtual std::optional<FileInfo> getFileInfo(MountId mountId) = 0;

protected:
    IFuseFileSystem() = default;
};

} // namespace motioncam
