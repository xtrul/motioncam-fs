#pragma once

#include <array>
#include <string>
#include <map>

namespace motioncam {

struct CalibrationProfile {
    std::string uniqueCameraModel;
    std::array<float,9> colorMatrix1{1,0,0,0,1,0,0,0,1};
    std::array<float,9> colorMatrix2{1,0,0,0,1,0,0,0,1};
    std::array<float,9> forwardMatrix1{1,0,0,0,1,0,0,0,1};
    std::array<float,9> forwardMatrix2{1,0,0,0,1,0,0,0,1};
    std::array<float,9> calibrationMatrix1{1,0,0,0,1,0,0,0,1};
    std::array<float,9> calibrationMatrix2{1,0,0,0,1,0,0,0,1};
    std::string colorIlluminant1{"d65"};
    std::string colorIlluminant2{"d65"};
};

std::map<std::string, CalibrationProfile> loadCalibrationProfiles(const std::string& path);

} // namespace motioncam
