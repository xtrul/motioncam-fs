#include "CalibrationProfile.h"
#include <fstream>
#include <QFile>
#include <QByteArray>

namespace motioncam {

namespace {
    template<typename T, size_t N>
    std::optional<std::array<T,N>> parseArray(const nlohmann::json& j) {
        if(!j.is_array() || j.size() < N)
            return std::nullopt;
        std::array<T,N> arr{};
        for(size_t i=0;i<N && i<j.size();++i)
            arr[i] = j[i].get<T>();
        return arr;
    }
}

std::map<std::string, CalibrationProfile> loadCalibrationProfiles(const std::string& path) {
    std::map<std::string, CalibrationProfile> profiles;
    std::ifstream ifs(path);
    nlohmann::json j;
    if(ifs.is_open()) {
        j = nlohmann::json::parse(ifs, nullptr, false);
    } else {
        QFile file(QString::fromStdString(path));
        if(file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            j = nlohmann::json::parse(data.constData(), nullptr, false);
        } else {
            return profiles;
        }
    }
    if(!j.is_object())
        return profiles;

    for(auto it=j.begin(); it!=j.end(); ++it) {
        CalibrationProfile p;
        const auto& obj = it.value();
        if(obj.contains("colorMatrix1"))
            p.colorMatrix1 = parseArray<float,9>(obj["colorMatrix1"]);
        if(obj.contains("colorMatrix2"))
            p.colorMatrix2 = parseArray<float,9>(obj["colorMatrix2"]);
        if(obj.contains("forwardMatrix1"))
            p.forwardMatrix1 = parseArray<float,9>(obj["forwardMatrix1"]);
        if(obj.contains("forwardMatrix2"))
            p.forwardMatrix2 = parseArray<float,9>(obj["forwardMatrix2"]);
        if(obj.contains("blackLevel"))
            p.blackLevel = parseArray<unsigned short,4>(obj["blackLevel"]);
        if(obj.contains("whiteLevel"))
            p.whiteLevel = obj["whiteLevel"].get<float>();
        profiles[it.key()] = p;
    }

    return profiles;
}

} // namespace motioncam

