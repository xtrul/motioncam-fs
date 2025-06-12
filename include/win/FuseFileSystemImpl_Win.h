#pragma once

#include <map>
#include <memory>

#include "IFuseFileSystem.h"

namespace motioncam {

class VirtualizationInstance;

class FuseFileSystemImpl_Win : public IFuseFileSystem
{
public:
    FuseFileSystemImpl_Win();

    MountId mount(FileRenderOptions options,
                  int draftScale,
                  const std::string& srcFile,
                  const std::string& dstPath,
                  const CalibrationProfile* calibration,
                  const CameraSettings* cameraSettings) override;
    void unmount(MountId mountId) override;
    void updateOptions(MountId mountId,
                       FileRenderOptions options,
                       int draftScale,
                       const CalibrationProfile* calibration,
                       const CameraSettings* cameraSettings) override;

private:
    MountId mNextMountId;
    std::map<MountId, std::unique_ptr<VirtualizationInstance>> mMountedFiles;
};

} // namespace motioncam
