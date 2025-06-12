#pragma once
#include <optional>
#include <array>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace motioncam {

struct CalibrationProfile {
    std::optional<std::array<float,9>> colorMatrix1;
    std::optional<std::array<float,9>> colorMatrix2;
    std::optional<std::array<float,9>> forwardMatrix1;
    std::optional<std::array<float,9>> forwardMatrix2;
    std::optional<std::array<unsigned short,4>> blackLevel;
    std::optional<float> whiteLevel;
};

std::map<std::string, CalibrationProfile> loadCalibrationProfiles(const std::string& path);

}
