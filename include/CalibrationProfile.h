#pragma once

#include <array>
#include <string>
#include <map>

#include <nlohmann/json.hpp>

namespace motioncam {

struct CalibrationProfile {
    std::string uniqueCameraModel;
    std::array<float, 9> colorMatrix1{};
    std::array<float, 9> colorMatrix2{};
    std::array<float, 9> forwardMatrix1{};
    std::array<float, 9> forwardMatrix2{};
    std::array<float, 9> calibrationMatrix1{};
    std::array<float, 9> calibrationMatrix2{};
    std::string colorIlluminant1;
    std::string colorIlluminant2;
    float baselineExposure{0.f};
};

std::map<std::string, CalibrationProfile> loadCalibrationProfiles(const std::string& path);

} // namespace motioncam

