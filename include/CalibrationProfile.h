#pragma once
#include <optional>
#include <array>
#include <string>
#include <map>

namespace motioncam {
struct CalibrationProfile {
    std::optional<std::array<float,9>> colorMatrix1;
    std::optional<std::array<float,9>> colorMatrix2;
    std::optional<std::array<float,9>> forwardMatrix1;
    std::optional<std::array<float,9>> forwardMatrix2;
    std::optional<std::array<float,9>> calibrationMatrix1;
    std::optional<std::array<float,9>> calibrationMatrix2;
    std::optional<std::string> colorIlluminant1;
    std::optional<std::string> colorIlluminant2;
};

std::map<std::string, CalibrationProfile> loadCalibrationProfiles(const std::string& file);
}
