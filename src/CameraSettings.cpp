#include "CameraSettings.h"

using json = nlohmann::json;

namespace motioncam {

CameraSettings CameraSettings::fromJson(const json& j) {
    CameraSettings settings;
    if(j.contains("uniqueCameraModel"))
        settings.uniqueCameraModel = j["uniqueCameraModel"].get<std::string>();
    return settings;
}

CameraSettingsMap loadCameraSettings(const std::string& jsonString) {
    json j = json::parse(jsonString);
    CameraSettingsMap settings;

    for(const auto& item : j.items()) {
        settings[item.key()] = CameraSettings::fromJson(item.value());
    }

    return settings;
}

} // namespace motioncam

