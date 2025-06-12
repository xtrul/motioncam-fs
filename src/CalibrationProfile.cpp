#include "CalibrationProfile.h"

#include <fstream>

namespace motioncam {

namespace {
    template<typename T, size_t N>
    std::array<T, N> jsonArrayToStdArray(const nlohmann::json& arr) {
        std::array<T, N> result{};
        for(size_t i = 0; i < N && i < arr.size(); ++i) {
            result[i] = arr[i].get<T>();
        }
        return result;
    }
}

std::map<std::string, CalibrationProfile> loadCalibrationProfiles(const std::string& path) {
    std::map<std::string, CalibrationProfile> profiles;
    std::ifstream ifs(path);
    if(!ifs)
        return profiles;

    nlohmann::json j;
    try {
        ifs >> j;
    } catch(...) {
        return profiles;
    }

    for(auto it = j.begin(); it != j.end(); ++it) {
        CalibrationProfile p;
        const auto& obj = it.value();
        if(obj.contains("colorMatrix1"))
            p.colorMatrix1 = jsonArrayToStdArray<float,9>(obj["colorMatrix1"]);
        if(obj.contains("colorMatrix2"))
            p.colorMatrix2 = jsonArrayToStdArray<float,9>(obj["colorMatrix2"]);
        if(obj.contains("forwardMatrix1"))
            p.forwardMatrix1 = jsonArrayToStdArray<float,9>(obj["forwardMatrix1"]);
        if(obj.contains("forwardMatrix2"))
            p.forwardMatrix2 = jsonArrayToStdArray<float,9>(obj["forwardMatrix2"]);
        if(obj.contains("blackLevel"))
            p.blackLevel = jsonArrayToStdArray<unsigned short,4>(obj["blackLevel"]);
        if(obj.contains("whiteLevel"))
            p.whiteLevel = obj["whiteLevel"].get<float>();
        profiles[it.key()] = p;
    }

    return profiles;
}

} // namespace motioncam
