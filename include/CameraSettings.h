#pragma once
#include <optional>
#include <string>
#include <map>

namespace motioncam {
struct CameraSettings {
    std::optional<std::string> uniqueCameraModel;
};

std::map<std::string, CameraSettings> loadCameraSettings(const std::string& file);
}
