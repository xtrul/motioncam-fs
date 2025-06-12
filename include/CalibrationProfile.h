#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <array>
#include <string>
#include <unordered_map>

namespace motioncam {

struct CalibrationProfile {
    std::optional<std::array<float, 9>> colorMatrix1;
    std::optional<std::array<float, 9>> colorMatrix2;
    std::optional<std::array<float, 9>> forwardMatrix1;
    std::optional<std::array<float, 9>> forwardMatrix2;
    std::optional<std::array<float, 9>> calibrationMatrix1;
    std::optional<std::array<float, 9>> calibrationMatrix2;
    std::optional<std::string> colorIlluminant1;
    std::optional<std::string> colorIlluminant2;
    std::optional<std::string> uniqueCameraModel;

    static CalibrationProfile fromJson(const nlohmann::json& j);
};

using CalibrationProfileMap = std::unordered_map<std::string, CalibrationProfile>;

CalibrationProfileMap loadCalibrationProfiles(const std::string& jsonString);

} // namespace motioncam

