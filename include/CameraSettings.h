#pragma once

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <optional>
#include <string>

namespace motioncam {

struct CameraSettings {
    std::optional<std::string> uniqueCameraModel;

    static CameraSettings fromJson(const nlohmann::json& j);
};

using CameraSettingsMap = std::unordered_map<std::string, CameraSettings>;

CameraSettingsMap loadCameraSettings(const std::string& jsonString);
CameraSettingsMap loadCameraSettingsFromFile(const std::string& filename);

} // namespace motioncam

