#include "CameraSettings.h"
#include <fstream>
#include <sstream>

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

CameraSettingsMap loadCameraSettingsFromFile(const std::string& filename) {
    std::ifstream f(filename);
    if(!f.is_open()) {
        throw std::runtime_error("Failed to open camera settings file: " + filename);
    }
    std::stringstream buffer;
    buffer << f.rdbuf();
    return loadCameraSettings(buffer.str());
}

} // namespace motioncam

