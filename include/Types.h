#pragma once

#include <vector>
#include <string>
#include <variant>

#include <boost/filesystem.hpp>

namespace motioncam {

enum EntryType : int {
    FILE_ENTRY = 0,
    DIRECTORY_ENTRY = 1,
    INVALID_ENTRY = -1
};

struct Entry {
    EntryType type;
    std::vector<std::string> pathParts;
    std::string name;
    size_t size;
    std::variant<int64_t> userData;

    // Custom hash function for Entry
    struct Hash {
        size_t operator()(const Entry& entry) const {
            size_t hash = std::hash<int>{}(static_cast<int>(entry.type));

            // Hash the path parts
            for (const auto& part : entry.pathParts) {
                hash ^= std::hash<std::string>{}(part) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }

            // Hash the name
            hash ^= std::hash<std::string>{}(entry.name) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

            return hash;
        }
    };

    // Custom equality operator for Entry
    bool operator==(const Entry& other) const {
        return type == other.type &&
               pathParts == other.pathParts &&
               name == other.name;
    }

    std::string getFullPath() {
        namespace fs = boost::filesystem;

        fs::path result;

        // Add each path part
        for (const auto& part : pathParts) {
            result /= part;
        }

        // Add the filename
        result /= name;

        return result.string();
    }
};


enum FileRenderOptions : unsigned int {
    RENDER_OPT_NONE                         = 0,
    RENDER_OPT_DRAFT                        = 1 << 0,
    RENDER_OPT_APPLY_VIGNETTE_CORRECTION    = 1 << 1
};

} // namespace
