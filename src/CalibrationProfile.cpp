#include "CalibrationProfile.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace motioncam {

using json = nlohmann::json;

static std::array<float,9> getMatrix(const json& j, const char* key) {
    std::array<float,9> out{1,0,0,0,1,0,0,0,1};
    if(j.contains(key) && j[key].is_array()) {
        for(size_t i=0;i<9 && i<j[key].size();++i)
            out[i] = j[key][i].get<float>();
    }
    return out;
}

std::map<std::string, CalibrationProfile> loadCalibrationProfiles(const std::string& path) {
    std::map<std::string, CalibrationProfile> profiles;
    std::ifstream f(path);
    if(!f.is_open())
        return profiles;
    json j = json::parse(f, nullptr, false);
    if(j.is_discarded() || !j.is_object())
        return profiles;

    for(auto it = j.begin(); it != j.end(); ++it) {
        if(!it.value().is_object())
            continue;
        CalibrationProfile p;
        auto obj = it.value();
        p.uniqueCameraModel = obj.value("uniqueCameraModel", "");
        p.colorMatrix1 = getMatrix(obj, "colorMatrix1");
        p.colorMatrix2 = getMatrix(obj, "colorMatrix2");
        p.forwardMatrix1 = getMatrix(obj, "forwardMatrix1");
        p.forwardMatrix2 = getMatrix(obj, "forwardMatrix2");
        p.calibrationMatrix1 = getMatrix(obj, "calibrationMatrix1");
        p.calibrationMatrix2 = getMatrix(obj, "calibrationMatrix2");
        p.colorIlluminant1 = obj.value("colorIlluminant1", "d65");
        p.colorIlluminant2 = obj.value("colorIlluminant2", "d65");
        profiles[it.key()] = p;
    }

    return profiles;
}

} // namespace motioncam
