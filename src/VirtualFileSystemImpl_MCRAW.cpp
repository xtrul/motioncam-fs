#include "VirtualFileSystemImpl_MCRAW.h"
#include "LRUCache.h"
#include "Measure.h"

#include <motioncam/Decoder.hpp>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

#include <BS_thread_pool.hpp>

#include <algorithm>
#include <sstream>

#define TINY_DNG_WRITER_IMPLEMENTATION 1

#include <tinydng/tiny_dng_writer.h>

namespace motioncam {

namespace {
    constexpr auto DRAFT_SCALE = 2;

    enum DngIlluminant {
        lsUnknown					=  0,
        lsDaylight					=  1,
        lsFluorescent				=  2,
        lsTungsten					=  3,
        lsFlash						=  4,
        lsFineWeather				=  9,
        lsCloudyWeather				= 10,
        lsShade						= 11,
        lsDaylightFluorescent		= 12,		// D  5700 - 7100K
        lsDayWhiteFluorescent		= 13,		// N  4600 - 5500K
        lsCoolWhiteFluorescent		= 14,		// W  3800 - 4500K
        lsWhiteFluorescent			= 15,		// WW 3250 - 3800K
        lsWarmWhiteFluorescent		= 16,		// L  2600 - 3250K
        lsStandardLightA			= 17,
        lsStandardLightB			= 18,
        lsStandardLightC			= 19,
        lsD55						= 20,
        lsD65						= 21,
        lsD75						= 22,
        lsD50						= 23,
        lsISOStudioTungsten			= 24,

        lsOther						= 255
    };

#ifdef _WIN32
    constexpr std::string_view DESKTOP_INI = R"([.ShellClassInfo]
ConfirmFileOp=0

[ViewState]
Mode=4
Vid={137E7700-3573-11CF-AE69-08002B2E1262}
FolderType=Generic

[{5984FFE0-28D4-11CF-AE66-08002B2E1262}]
Mode=4
LogicalViewMode=1
IconSize=16

[LocalizedFileNames]
)";

#endif

    std::string extractFilenameWithoutExtension(const std::string& fullPath) {
        boost::filesystem::path p(fullPath);
        return p.stem().string();
    }

    float calculateFrameRate(const std::vector<Timestamp>& frames) {
        // Need at least 2 frames to calculate frame rate
        if (frames.size() < 2) {
            return 0.0f;
        }

        // Use running average to prevent overflow
        double avgDuration = 0.0;
        int validFrames = 0;

        for (size_t i = 1; i < frames.size(); ++i) {
            double duration = static_cast<double>(frames[i] - frames[i-1]);

            if (duration > 0) {
                // Update running average
                // new_avg = old_avg + (new_value - old_avg) / (count + 1)
                avgDuration = avgDuration + (duration - avgDuration) / (validFrames + 1);
                validFrames++;
            }
        }

        if (validFrames == 0) {
            return 0.0f;
        }

        return static_cast<float>(1000000000.0 / avgDuration);
    }

    int64_t getFrameNumberFromTimestamp(Timestamp timestamp, Timestamp referenceTimestamp, float frameRate) {
        if (frameRate <= 0) {
            return -1; // Invalid frame rate
        }

        int64_t timeDifference = timestamp - referenceTimestamp;
        if (timeDifference < 0) {
            return -1;
        }

        // Calculate microseconds per frame
        double nanosecondsPerFrame = 1000000000.0 / frameRate;

        // Calculate expected frame number
        return static_cast<int64_t>(std::round(timeDifference / nanosecondsPerFrame));
    }

    std::string constructFrameFilename(
        const std::string& baseName, int frameNumber, int padding = 6, const std::string& extension = "")
    {
        std::ostringstream oss;

        // Add the base name
        oss << baseName;

        // Add the zero-padded frame number
        oss << std::setfill('0') << std::setw(padding) << frameNumber;

        // Add the extension if provided
        if (!extension.empty()) {
            // Check if extension already has a dot prefix
            if (extension[0] != '.') {
                oss << '.';
            }
            oss << extension;
        }

        return oss.str();
    }

    int getColorIlluminant(const std::string& value) {
        if(value == "standarda")
            return lsStandardLightA;
        else if(value == "standardb")
            return lsStandardLightB;
        else if(value == "standardc")
            return lsStandardLightC;
        else if(value == "d50")
            return lsD50;
        else if(value == "d55")
            return lsD55;
        else if(value == "d65")
            return lsD65;
        else if(value == "d75")
            return lsD75;
        else
            return lsUnknown;
    }

    int getScaleFromOptions(FileRenderOptions options) {
        if(options & RENDER_OPT_DRAFT)
            return DRAFT_SCALE;

        return 1;
    }

    void createProxyDngInPlace(
        std::vector<uint8_t>& data,
        uint32_t& inOutWidth,
        uint32_t& inOutHeight,
        uint32_t scale = 2)
    {
        if (scale < 2)
            return;

        // Ensure even scale for downscaling
        scale = (scale / 2) * 2;

        // Calculate new dimensions
        uint32_t newWidth = inOutWidth / scale;
        uint32_t newHeight = inOutHeight / scale;

        // Ensure even dimensions for Bayer pattern
        newWidth = (newWidth / 2) * 2;
        newHeight = (newHeight / 4) * 4;

        uint32_t originalWidth = inOutWidth;
        uint32_t dstOffset = 0;

        // Reinterpret the input data as uint16_t for reading
        uint16_t* srcData = reinterpret_cast<uint16_t*>(data.data());

        // Process the image by copying and packing 2x2 Bayer blocks
        for (auto y = 0; y < newHeight; y += 2) {
            for (auto x = 0; x < newWidth; x += 2) {
                // Get the source coordinates (scaled)
                uint32_t srcY = y * scale;
                uint32_t srcX = x * scale;

                // Copy the 2x2 Bayer block
                srcData[dstOffset]                 = srcData[srcY * originalWidth + srcX];
                srcData[dstOffset + 1]             = srcData[srcY * originalWidth + srcX + 1];
                srcData[dstOffset + newWidth]      = srcData[(srcY + 1) * originalWidth + srcX];
                srcData[dstOffset + newWidth + 1]  = srcData[(srcY + 1) * originalWidth + srcX + 1];

                dstOffset += 2;
            }

            dstOffset += newWidth;
        }

        data.resize(sizeof(uint16_t) * newWidth * newHeight);

        // Update dimensions
        inOutWidth = newWidth;
        inOutHeight = newHeight;
    }

    void encodeTo10Bit(
        std::vector<uint8_t>& data,
        uint32_t& width,
        uint32_t& height)
    {
        Measure m("encodeTo10Bit");

        uint16_t* srcPtr = reinterpret_cast<uint16_t*>(data.data());
        uint8_t* dstPtr = data.data();

        for(int y = 0; y < height; y++) {
            for(int x = 0; x < width; x+=4) {
                const uint16_t p0 = srcPtr[0];
                const uint16_t p1 = srcPtr[1];
                const uint16_t p2 = srcPtr[2];
                const uint16_t p3 = srcPtr[3];

                dstPtr[0] = p0 >> 2;
                dstPtr[1] = ((p0 & 0x03) << 6) | (p1 >> 4);
                dstPtr[2] = ((p1 & 0x0F) << 4) | (p2 >> 6);
                dstPtr[3] = ((p2 & 0x3F) << 2) | (p3 >> 8);
                dstPtr[4] = p3 & 0xFF;

                srcPtr += 4;
                dstPtr += 5;
            }
        }

        // Resize to fit new data
        auto newSize = dstPtr - data.data();

        data.resize(newSize);
    }

    std::shared_ptr<std::vector<char>> generateDng(
        std::vector<uint8_t>& data,
        const nlohmann::json& metadata,
        const nlohmann::json& containerMetadata,
        const int scale=1)
    {
        Measure m("generateDng");

        unsigned int width = metadata["width"];
        unsigned int height = metadata["height"];

        createProxyDngInPlace(data, width, height, scale);

        encodeTo10Bit(data, width, height);

        std::vector<float> asShotNeutral = metadata["asShotNeutral"];

        std::vector<uint16_t> blackLevel = containerMetadata["blackLevel"];
        double whiteLevel = containerMetadata["whiteLevel"];
        std::string sensorArrangement = containerMetadata["sensorArrangment"];
        std::vector<float> colorMatrix1 = containerMetadata["colorMatrix1"];
        std::vector<float> colorMatrix2 = containerMetadata["colorMatrix2"];
        std::vector<float> forwardMatrix1 = containerMetadata["forwardMatrix1"];
        std::vector<float> forwardMatrix2 = containerMetadata["forwardMatrix2"];

        std::string colorIlluminant1 = containerMetadata["colorIlluminant1"];
        std::string colorIlluminant2 = containerMetadata["colorIlluminant2"];

        // Create first frame
        tinydngwriter::DNGImage dng;

        dng.SetBigEndian(false);
        dng.SetDNGVersion(1, 4, 0, 0);
        dng.SetDNGBackwardVersion(1, 1, 0, 0);
        dng.SetImageData(reinterpret_cast<const unsigned char*>(data.data()), data.size());
        dng.SetImageWidth(width);
        dng.SetImageLength(height);
        dng.SetPlanarConfig(tinydngwriter::PLANARCONFIG_CONTIG);
        dng.SetPhotometric(tinydngwriter::PHOTOMETRIC_CFA);
        dng.SetRowsPerStrip(height);
        dng.SetSamplesPerPixel(1);
        dng.SetCFARepeatPatternDim(2, 2);

        dng.SetBlackLevelRepeatDim(2, 2);
        dng.SetBlackLevel(4, blackLevel.data());
        dng.SetWhiteLevel(whiteLevel);
        dng.SetCompression(tinydngwriter::COMPRESSION_NONE);

        std::vector<uint8_t> cfa;

        if(sensorArrangement == "rggb")
            cfa = { 0, 1, 1, 2 };
        else if(sensorArrangement == "bggr")
            cfa = { 2, 1, 1, 0 };
        else if(sensorArrangement == "grbg")
            cfa = { 1, 0, 2, 1 };
        else if(sensorArrangement == "gbrg")
            cfa = { 1, 2, 0, 1 };
        else
            throw std::runtime_error("Invalid sensor arrangement");

        dng.SetCFAPattern(4, cfa.data());

        // Rectangular
        dng.SetCFALayout(1);

        const uint16_t bps[1] = { 10 };
        dng.SetBitsPerSample(1, bps);

        dng.SetColorMatrix1(3, colorMatrix1.data());
        dng.SetColorMatrix2(3, colorMatrix2.data());

        dng.SetForwardMatrix1(3, forwardMatrix1.data());
        dng.SetForwardMatrix2(3, forwardMatrix2.data());

        dng.SetAsShotNeutral(3, asShotNeutral.data());

        dng.SetCalibrationIlluminant1(getColorIlluminant(colorIlluminant1));
        dng.SetCalibrationIlluminant2(getColorIlluminant(colorIlluminant2));

        dng.SetUniqueCameraModel("MotionCam");
        dng.SetSubfileType();

        const uint32_t activeArea[4] = { 0, 0, height, width };
        dng.SetActiveArea(&activeArea[0]);

        // Write DNG
        std::string err;

        tinydngwriter::DNGWriter writer(false);

        writer.AddImage(&dng);

        // Save to memory
        auto output = std::make_shared<std::vector<char>>();

        // Reserve enough to fit the data
        output->reserve(width*height*sizeof(uint16_t) + 512*1024);

        boost::iostreams::back_insert_device<std::vector<char>> sink(*output);
        boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>>> stream(sink);

        writer.WriteToFile(stream, &err);

        return output;
    }
}

VirtualFileSystemImpl_MCRAW::VirtualFileSystemImpl_MCRAW(
    FileRenderOptions options, LRUCache& cache, const std::string& file) :
        mCache(cache),
        mSrcPath(file),
        mBaseName(extractFilenameWithoutExtension(file)),
        mTypicalDngSize(0),
        mDecoder(std::make_unique<Decoder>(file)) {

    init(options);
}


void VirtualFileSystemImpl_MCRAW::init(FileRenderOptions options) {
    auto frames = mDecoder->getFrames();
    std::sort(frames.begin(), frames.end());

    if(frames.empty())
        return;

    spdlog::debug("VirtualFileSystemImpl_MCRAW::init(options={})", static_cast<unsigned int>(options));

    // Clear everything
    mCache.clear();
    mFiles.clear();

    // Calculate typical DNG size that we can use for all files
    std::vector<uint8_t> data;
    nlohmann::json metadata;

    mDecoder->loadFrame(frames[0], data, metadata);

    auto dngData = generateDng(data, metadata, mDecoder->getContainerMetadata(), getScaleFromOptions(options));
    mTypicalDngSize = dngData->size();

    // Generate file entries
    auto fps = calculateFrameRate(frames);
    int lastPts = 0;

    mFiles.reserve(frames.size()*2);

#ifdef _WIN32
    Entry desktopIni;

    desktopIni.type = FILE_ENTRY;
    desktopIni.size = DESKTOP_INI.size();
    desktopIni.name = "desktop.ini";

    mFiles.emplace_back(desktopIni);
#endif

    // Add video frames
    for(auto& x : frames) {
        int pts = getFrameNumberFromTimestamp(x, frames[0], fps);

        // Duplicate frames to account for dropped frames
        while(lastPts < pts) {
            Entry entry;

            entry.type = EntryType::FILE_ENTRY;
            entry.size = mTypicalDngSize;
            entry.name = constructFrameFilename(mBaseName, lastPts, 6, "dng");
            entry.userData = x;

            mFiles.emplace_back(entry);
            ++lastPts;
        }
    }

    // No point wasting the initial frame, add it to the cache
    mCache.put(mFiles[0], dngData);
}

const std::vector<Entry>& VirtualFileSystemImpl_MCRAW::listFiles(const std::string& filter) const {
    //TODO: Apply filter
    return mFiles;
}

std::optional<Entry> VirtualFileSystemImpl_MCRAW::findEntry(const std::string& filter) const {
    try {
        // Normalize the filter path by converting all backslashes to forward slashes
        std::string normalizedFilter = filter;
        boost::replace_all(normalizedFilter, "\\", "/");

        // Convert the filter string to a regex pattern
        // Escape dots in the literal parts of the pattern
        std::string regexPattern = boost::replace_all_copy(normalizedFilter, ".", "\\.");

        // Handle wildcards
        regexPattern = boost::replace_all_copy(regexPattern, "*", ".*");
        regexPattern = boost::replace_all_copy(regexPattern, "?", ".");

        boost::regex pattern(regexPattern, boost::regex::icase);

        // Iterate through all entries
        for (const auto& entry : mFiles) {
            // Check if the name matches the filter
            if (boost::regex_match(entry.name, pattern)) {
                return entry;
            }

            // Construct the full path from pathParts and name
            std::string fullPath;
            for (const auto& part : entry.pathParts) {
                fullPath += part + "/";
            }

            fullPath += entry.name;

            // Also check the full path against the pattern
            if (boost::regex_match(fullPath, pattern)) {
                return entry;
            }

            // Handle case where filter might be a partial path
            // For example, if filter is "dir/file.txt" or "dir\\file.txt"
            boost::filesystem::path filterPath(normalizedFilter);
            std::string filterFilename = filterPath.filename().string();

            // If the filename part matches and we should check path components
            if (entry.name == filterFilename || boost::regex_match(entry.name, boost::regex(filterFilename))) {
                // The filter might contain path information
                if (filterPath.has_parent_path()) {
                    std::string parentPath = filterPath.parent_path().string();
                    boost::replace_all(parentPath, "\\", "/");

                    // Reconstruct entry's parent path
                    std::string entryParentPath;
                    for (const auto& part : entry.pathParts) {
                        entryParentPath += part + "/";
                    }

                    // Remove trailing slash if present
                    if (!entryParentPath.empty() && entryParentPath.back() == '/') {
                        entryParentPath.pop_back();
                    }

                    // Check if the parent paths match
                    if (entryParentPath.find(parentPath) != std::string::npos ||
                        boost::ends_with(entryParentPath, parentPath)) {
                        return entry;
                    }
                }
            }
        }

        // No match found
        return std::nullopt;
    }
    catch (const boost::regex_error& e) {
        spdlog::error("findEntry(error={})", e.what());

        // Handle invalid regex pattern
        return std::nullopt;
    }
    catch (const std::exception& e) {
        spdlog::error("findEntry(error={})", e.what());

        // Handle other exceptions
        return std::nullopt;
    }
}

int VirtualFileSystemImpl_MCRAW::readFile(const Entry& entry, FileRenderOptions options, const size_t pos, const size_t len, void* dst) const {
#ifdef _WIN32
    if(entry.name == "desktop.ini") {
        if(len < DESKTOP_INI.size())
            return 0;

        std::memcpy(dst, DESKTOP_INI.data(), DESKTOP_INI.size());

        return DESKTOP_INI.size();
    }
#endif

    // Try to get from cache first
    auto dngData = mCache.get(entry);
    if(!dngData) {
        try {
            auto timestamp = std::get<Timestamp>(entry.userData);
            thread_local Decoder decoder(mSrcPath); // Use a decoder for each thread

            spdlog::debug("Reading frame {} with options {}", timestamp, static_cast<unsigned int>(options));

            std::vector<uint8_t> data;
            nlohmann::json metadata;

            decoder.loadFrame(timestamp, data, metadata);

            dngData = generateDng(data, metadata, mDecoder->getContainerMetadata(), getScaleFromOptions(options));

            mCache.put(entry, dngData);
        }
        catch(std::runtime_error& e) {
            spdlog::error("Failed to read frame (error: {})", e.what());
            return 0;
        }
    }

    if(!dngData)
        return 0;

    if (pos >= dngData->size())
        return 0;

    // Calculate actual length to copy (to avoid going out of bounds)
    const size_t actualLen = std::min(len, dngData->size() - pos);

    std::memcpy(dst, dngData->data() + pos, actualLen);

    return actualLen;
}

void VirtualFileSystemImpl_MCRAW::updateOptions(FileRenderOptions options) {
    init(options);
}

} // namespace motioncam

