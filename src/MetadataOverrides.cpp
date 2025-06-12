#include "MetadataOverrides.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <boost/filesystem.hpp>

namespace motioncam {

using json = nlohmann::json;
namespace fs = boost::filesystem;

static std::map<std::string, CameraProfile> gCameraProfiles;
static std::map<std::string, MatrixProfile> gMatrixProfiles;
static std::string gSelectedCamera = "(No Override)";
static std::string gSelectedMatrix = "(No Override)";


void loadOverrideProfiles(const std::string& directory) {
    gCameraProfiles.clear();
    gMatrixProfiles.clear();

    fs::path dir(directory);

    fs::path cameraPath = dir / "camera-name.json";
    if(fs::exists(cameraPath)) {
        try {
            std::ifstream f(cameraPath.string());
            json j = json::parse(f);
            for(auto it=j.begin(); it!=j.end(); ++it) {
                if(it.value().contains("uniqueCameraModel") && it.value()["uniqueCameraModel"].is_string()) {
                    gCameraProfiles[it.key()] = { it.value()["uniqueCameraModel"].get<std::string>() };
                }
            }
        } catch(const std::exception& e) {
            spdlog::error("Failed to parse {}: {}", cameraPath.string(), e.what());
        }
    } else {
        spdlog::warn("camera-name.json missing in {}", directory);
    }

    fs::path matrixPath = dir / "matrix-calibration.json";
    if(fs::exists(matrixPath)) {
        try {
            std::ifstream f(matrixPath.string());
            json j = json::parse(f);
            for(auto it=j.begin(); it!=j.end(); ++it) {
                MatrixProfile p; p.data = it.value();
                gMatrixProfiles[it.key()] = std::move(p);
            }
        } catch(const std::exception& e) {
            spdlog::error("Failed to parse {}: {}", matrixPath.string(), e.what());
        }
    } else {
        spdlog::warn("matrix-calibration.json missing in {}", directory);
    }
}

const std::map<std::string, CameraProfile>& getCameraProfiles() { return gCameraProfiles; }
const std::map<std::string, MatrixProfile>& getMatrixProfiles() { return gMatrixProfiles; }

void setSelectedCameraProfile(const std::string& key) { gSelectedCamera = key; }
void setSelectedMatrixProfile(const std::string& key) { gSelectedMatrix = key; }

const std::string& getSelectedCameraProfile() { return gSelectedCamera; }
const std::string& getSelectedMatrixProfile() { return gSelectedMatrix; }

} // namespace motioncam

