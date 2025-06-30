#include "CalibrationProfile.h"

#include <fstream>
#include <stdexcept>

namespace motioncam {

static std::array<float,9> toArray(const nlohmann::json& j) {
    std::array<float,9> a{};
    for(size_t i=0;i<9 && i<j.size();++i)
        a[i] = j[i].get<float>();
    return a;
}

std::map<std::string, CalibrationProfile> loadCalibrationProfiles(const std::string& path) {
    std::ifstream f(path);
    if(!f.is_open())
        throw std::runtime_error("Failed to open calibration file");

    nlohmann::json j;
    f >> j;

    std::map<std::string, CalibrationProfile> profiles;
    for(auto it=j.begin(); it!=j.end(); ++it) {
        CalibrationProfile p;
        const auto& obj = it.value();
        p.uniqueCameraModel = obj.value("uniqueCameraModel", "");
        if(obj.contains("colorMatrix1")) p.colorMatrix1 = toArray(obj["colorMatrix1"]);
        if(obj.contains("colorMatrix2")) p.colorMatrix2 = toArray(obj["colorMatrix2"]);
        if(obj.contains("forwardMatrix1")) p.forwardMatrix1 = toArray(obj["forwardMatrix1"]);
        if(obj.contains("forwardMatrix2")) p.forwardMatrix2 = toArray(obj["forwardMatrix2"]);
        if(obj.contains("calibrationMatrix1")) p.calibrationMatrix1 = toArray(obj["calibrationMatrix1"]);
        if(obj.contains("calibrationMatrix2")) p.calibrationMatrix2 = toArray(obj["calibrationMatrix2"]);
        p.colorIlluminant1 = obj.value("colorIlluminant1", "");
        p.colorIlluminant2 = obj.value("colorIlluminant2", "");
        p.baselineExposure = obj.value("baselineExposure", 0.f);
        profiles[it.key()] = p;
    }

    return profiles;
}

} // namespace motioncam

