#include "CalibrationProfile.h"

using json = nlohmann::json;

namespace motioncam {

namespace {
    template<typename T, std::size_t N>
    std::array<T, N> parseArray(const json& arr) {
        std::array<T, N> result{};
        for(std::size_t i = 0; i < N && i < arr.size(); ++i)
            result[i] = arr[i].get<T>();
        return result;
    }
}

CalibrationProfile CalibrationProfile::fromJson(const json& j) {
    CalibrationProfile profile;

    if(j.contains("colorMatrix1"))
        profile.colorMatrix1 = parseArray<float,9>(j["colorMatrix1"]);
    if(j.contains("colorMatrix2"))
        profile.colorMatrix2 = parseArray<float,9>(j["colorMatrix2"]);
    if(j.contains("forwardMatrix1"))
        profile.forwardMatrix1 = parseArray<float,9>(j["forwardMatrix1"]);
    if(j.contains("forwardMatrix2"))
        profile.forwardMatrix2 = parseArray<float,9>(j["forwardMatrix2"]);
    if(j.contains("calibrationMatrix1"))
        profile.calibrationMatrix1 = parseArray<float,9>(j["calibrationMatrix1"]);
    if(j.contains("calibrationMatrix2"))
        profile.calibrationMatrix2 = parseArray<float,9>(j["calibrationMatrix2"]);

    if(j.contains("colorIlluminant1"))
        profile.colorIlluminant1 = j["colorIlluminant1"].get<std::string>();
    if(j.contains("colorIlluminant2"))
        profile.colorIlluminant2 = j["colorIlluminant2"].get<std::string>();
    if(j.contains("uniqueCameraModel"))
        profile.uniqueCameraModel = j["uniqueCameraModel"].get<std::string>();

    return profile;
}

CalibrationProfileMap loadCalibrationProfiles(const std::string& jsonString) {
    json j = json::parse(jsonString);
    CalibrationProfileMap profiles;

    for(const auto& item : j.items()) {
        profiles[item.key()] = CalibrationProfile::fromJson(item.value());
    }

    return profiles;
}

} // namespace motioncam

