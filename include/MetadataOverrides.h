#ifndef METADATAOVERRIDES_H
#define METADATAOVERRIDES_H

#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include <array>

namespace motioncam {

struct CameraProfile {
    std::string uniqueCameraModel;
};

struct MatrixProfile {
    nlohmann::json data;
};

template<typename T, size_t N>
std::array<T, N> jsonArrayToStdArray(const nlohmann::json& arr) {
    std::array<T, N> result{};
    for(size_t i=0; i < N && i < arr.size(); ++i) {
        result[i] = arr[i].get<T>();
    }
    return result;
}

void loadOverrideProfiles(const std::string& directory);
const std::map<std::string, CameraProfile>& getCameraProfiles();
const std::map<std::string, MatrixProfile>& getMatrixProfiles();

void setSelectedCameraProfile(const std::string& key);
void setSelectedMatrixProfile(const std::string& key);
const std::string& getSelectedCameraProfile();
const std::string& getSelectedMatrixProfile();

}

#endif // METADATAOVERRIDES_H
