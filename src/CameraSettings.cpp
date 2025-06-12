#include "CameraSettings.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace motioncam {

std::map<std::string, CameraSettings> loadCameraSettings(const std::string& file) {
    std::map<std::string, CameraSettings> settings;
    std::ifstream in(file);
    if(!in)
        return settings;

    json j;
    in >> j;

    for(auto it = j.begin(); it != j.end(); ++it) {
        CameraSettings s;
        auto& obj = it.value();
        if(obj.contains("uniqueCameraModel"))
            s.uniqueCameraModel = obj["uniqueCameraModel"].get<std::string>();
        settings[it.key()] = s;
    }

    return settings;
}

} // namespace motioncam
