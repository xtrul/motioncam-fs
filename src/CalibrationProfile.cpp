#include "CalibrationProfile.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace motioncam {
namespace {
    template<typename T, size_t N>
    std::array<T,N> jsonToArray(const json& arr) {
        std::array<T,N> out{};
        for(size_t i=0;i<N && i<arr.size();++i)
            out[i]=arr[i].get<T>();
        return out;
    }
}

std::map<std::string, CalibrationProfile> loadCalibrationProfiles(const std::string& file) {
    std::map<std::string, CalibrationProfile> profiles;
    std::ifstream in(file);
    if(!in)
        return profiles;

    json j;
    in >> j;

    for(auto it = j.begin(); it != j.end(); ++it) {
        CalibrationProfile p;
        auto& obj = it.value();
        if(obj.contains("colorMatrix1")) p.colorMatrix1 = jsonToArray<float,9>(obj["colorMatrix1"]);
        if(obj.contains("colorMatrix2")) p.colorMatrix2 = jsonToArray<float,9>(obj["colorMatrix2"]);
        if(obj.contains("forwardMatrix1")) p.forwardMatrix1 = jsonToArray<float,9>(obj["forwardMatrix1"]);
        if(obj.contains("forwardMatrix2")) p.forwardMatrix2 = jsonToArray<float,9>(obj["forwardMatrix2"]);
        if(obj.contains("calibrationMatrix1")) p.calibrationMatrix1 = jsonToArray<float,9>(obj["calibrationMatrix1"]);
        if(obj.contains("calibrationMatrix2")) p.calibrationMatrix2 = jsonToArray<float,9>(obj["calibrationMatrix2"]);
        if(obj.contains("colorIlluminant1")) p.colorIlluminant1 = obj["colorIlluminant1"].get<std::string>();
        if(obj.contains("colorIlluminant2")) p.colorIlluminant2 = obj["colorIlluminant2"].get<std::string>();
        profiles[it.key()] = p;
    }

    return profiles;
}

} // namespace motioncam
