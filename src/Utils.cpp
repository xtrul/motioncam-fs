#include "Utils.h"
#include "Measure.h"

#include "CameraFrameMetadata.h"
#include "CameraMetadata.h"

#include <algorithm>
#include <cmath> // For std::pow, std::round, std::floor
#include <vector> // Redundant if already included via other headers, but good for clarity
#include <string> // Redundant if already included via other headers
#include <array>  // Redundant if already included via other headers

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

#define TINY_DNG_WRITER_IMPLEMENTATION 1
// #define TINY_DNG_WRITER_DEBUG // Optional: for debugging tiny_dng_writer itself

#include <tinydng/tiny_dng_writer.h>
#include <spdlog/spdlog.h> // For logging, if not already included via Measure.h

namespace motioncam {
namespace utils {

namespace {
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

    inline uint8_t ToTimecodeByte(int value)
    {
        return (((value / 10) << 4) | (value % 10));
    }

    unsigned short bitsNeeded(unsigned short value) {
        if (value == 0)
            return 1; // Technically 0 needs 0, but for buffer allocation, 1 is safer min. DNG expects >0.

        unsigned short bits = 0;

        while (value > 0) {
            value >>= 1;
            bits++;
        }

        return bits;
    }

    int getColorIlluminant(const std::string& value) {
        std::string lowerValue = value;
        std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);

        if(lowerValue == "standarda")
            return lsStandardLightA;
        else if(lowerValue == "standardb")
            return lsStandardLightB;
        else if(lowerValue == "standardc")
            return lsStandardLightC;
        else if(lowerValue == "d50")
            return lsD50;
        else if(lowerValue == "d55")
            return lsD55;
        else if(lowerValue == "d65")
            return lsD65;
        else if(lowerValue == "d75")
            return lsD75;
        else
            return lsUnknown;
    }

    void normalizeShadingMap(std::vector<std::vector<float>>& shadingMap) {
        if (shadingMap.empty()) { // Check if outer vector is empty
            return;
        }
        bool isEmpty = true;
        for(const auto& channel : shadingMap) {
            if (!channel.empty()) {
                isEmpty = false;
                break;
            }
        }
        if (isEmpty) { // Check if all inner vectors are empty
            return;
        }

        float maxValue = 0.0f;
        bool firstValSet = false;
        for (const auto& channel : shadingMap) {
            for (float value : channel) {
                if (!firstValSet) {
                    maxValue = value;
                    firstValSet = true;
                } else {
                    maxValue = std::max(maxValue, value);
                }
            }
        }
        
        if (!firstValSet) return;

        if (maxValue <= 1e-6f) { 
            return;
        }

        for (auto& channel : shadingMap) { 
            for (float& value : channel) { 
                value /= maxValue;
            }
        }
    }

    inline float getShadingMapValue(
        float x, float y, int channel, const std::vector<std::vector<float>>& lensShadingMap, int lensShadingMapWidth, int lensShadingMapHeight)
    {
        if (lensShadingMap.empty() || channel < 0 || static_cast<size_t>(channel) >= lensShadingMap.size() || lensShadingMap[channel].empty() || lensShadingMapWidth <= 1 || lensShadingMapHeight <= 1) {
            return 1.0f; 
        }

        x = std::max(0.0f, std::min(1.0f, x));
        y = std::max(0.0f, std::min(1.0f, y));

        const float mapX = x * (lensShadingMapWidth - 1);
        const float mapY = y * (lensShadingMapHeight - 1);

        const int x0 = static_cast<int>(std::floor(mapX));
        const int y0 = static_cast<int>(std::floor(mapY));
        
        const int x1 = std::min(x0 + 1, lensShadingMapWidth - 1);
        const int y1 = std::min(y0 + 1, lensShadingMapHeight - 1);

        const float wx = mapX - x0;
        const float wy = mapY - y0;

        size_t idx00 = static_cast<size_t>(y0 * lensShadingMapWidth + x0);
        size_t idx01 = static_cast<size_t>(y0 * lensShadingMapWidth + x1);
        size_t idx10 = static_cast<size_t>(y1 * lensShadingMapWidth + x0);
        size_t idx11 = static_cast<size_t>(y1 * lensShadingMapWidth + x1);

        const auto& currentChannelMap = lensShadingMap[channel];
        if (idx00 >= currentChannelMap.size() || idx01 >= currentChannelMap.size() ||
            idx10 >= currentChannelMap.size() || idx11 >= currentChannelMap.size()) {
             if (idx00 < currentChannelMap.size()) return currentChannelMap[idx00];
             return 1.0f;
        }

        const float val00 = currentChannelMap[idx00];
        const float val01 = currentChannelMap[idx01];
        const float val10 = currentChannelMap[idx10];
        const float val11 = currentChannelMap[idx11];

        const float valTop = val00 * (1.0f - wx) + val01 * wx;
        const float valBottom = val10 * (1.0f - wx) + val11 * wx;

        return valTop * (1.0f - wy) + valBottom * wy;
    }
} // anonymous namespace

void encodeTo10Bit(
    std::vector<uint8_t>& data,
    uint32_t& width, 
    uint32_t& height)
{
    Measure m("encodeTo10Bit");

    if (data.empty() || width == 0 || height == 0 || (width % 4 != 0) ) {
        if ( (width % 4 != 0) ) { 
             spdlog::warn("encodeTo10Bit: Width ({}) must be a multiple of 4 for 10-bit packing. Skipping.", width);
             return; 
        }
        spdlog::warn("encodeTo10Bit: Invalid input (empty data or zero dimensions). Skipping.");
        return;
    }
    if (data.size() < static_cast<size_t>(width) * height * sizeof(uint16_t)) {
        spdlog::warn("encodeTo10Bit: Data size ({}) too small for dimensions ({}x{}x2). Skipping.", data.size(), width, height);
        return; 
    }

    std::vector<uint8_t> packed_data;
    packed_data.resize((static_cast<size_t>(width) * height / 4) * 5);

    uint16_t* srcPtr = reinterpret_cast<uint16_t*>(data.data());
    uint8_t* dstPtr = packed_data.data();

    for(uint32_t y = 0; y < height; y++) {
        for(uint32_t x = 0; x < width; x+=4) {
            const uint16_t p0 = srcPtr[0]; 
            const uint16_t p1 = srcPtr[1]; 
            const uint16_t p2 = srcPtr[2]; 
            const uint16_t p3 = srcPtr[3]; 

            dstPtr[0] = (p0 >> 2) & 0xFF;                       
            dstPtr[1] = ((p0 & 0x03) << 6) | ((p1 >> 4) & 0x3F); 
            dstPtr[2] = ((p1 & 0x0F) << 4) | ((p2 >> 6) & 0x0F); 
            dstPtr[3] = ((p2 & 0x3F) << 2) | ((p3 >> 8) & 0x03); 
            dstPtr[4] = p3 & 0xFF;                              

            srcPtr += 4;
            dstPtr += 5;
        }
    }
    data = std::move(packed_data); 
}

void encodeTo12Bit(
    std::vector<uint8_t>& data,
    uint32_t& width, 
    uint32_t& height)
{
    Measure m("encodeTo12Bit");
    if (data.empty() || width == 0 || height == 0 || (width % 2 != 0)) {
        if ( (width % 2 != 0) ) {
            spdlog::warn("encodeTo12Bit: Width ({}) must be a multiple of 2 for 12-bit packing. Skipping.", width);
            return; 
        }
        spdlog::warn("encodeTo12Bit: Invalid input (empty data or zero dimensions). Skipping.");
        return;
    }
    if (data.size() < static_cast<size_t>(width) * height * sizeof(uint16_t)) {
        spdlog::warn("encodeTo12Bit: Data size ({}) too small for dimensions ({}x{}x2). Skipping.", data.size(), width, height);
        return; 
    }

    std::vector<uint8_t> packed_data;
    packed_data.resize((static_cast<size_t>(width) * height / 2) * 3);

    uint16_t* srcPtr = reinterpret_cast<uint16_t*>(data.data());
    uint8_t* dstPtr = packed_data.data();

    for(uint32_t y = 0; y < height; y++) {
        for(uint32_t x = 0; x < width; x+=2) {
            const uint16_t p0 = srcPtr[0]; 
            const uint16_t p1 = srcPtr[1]; 

            dstPtr[0] = (p0 >> 4) & 0xFF;                       
            dstPtr[1] = ((p0 & 0x0F) << 4) | ((p1 >> 8) & 0x0F); 
            dstPtr[2] = p1 & 0xFF;                              

            srcPtr += 2;
            dstPtr += 3;
        }
    }
    data = std::move(packed_data); 
}

void encodeTo14Bit(
    std::vector<uint8_t>& data,
    uint32_t& width, 
    uint32_t& height)
{
    Measure m("encodeTo14Bit");

    if (data.empty() || width == 0 || height == 0 || (width % 4 != 0)) {
        if ( (width % 4 != 0) ) {
            spdlog::warn("encodeTo14Bit: Width ({}) must be a multiple of 4 for 14-bit packing. Skipping.", width);
            return; 
        }
        spdlog::warn("encodeTo14Bit: Invalid input (empty data or zero dimensions). Skipping.");
        return;
    }
    if (data.size() < static_cast<size_t>(width) * height * sizeof(uint16_t)) {
        spdlog::warn("encodeTo14Bit: Data size ({}) too small for dimensions ({}x{}x2). Skipping.", data.size(), width, height);
        return; 
    }

    std::vector<uint8_t> packed_data;
    packed_data.resize((static_cast<size_t>(width) * height / 4) * 7);

    uint16_t* srcPtr = reinterpret_cast<uint16_t*>(data.data());
    uint8_t* dstPtr = packed_data.data();

    for(uint32_t y = 0; y < height; y++) {
        for(uint32_t x = 0; x < width; x+=4) {
            const uint16_t p0 = srcPtr[0]; 
            const uint16_t p1 = srcPtr[1]; 
            const uint16_t p2 = srcPtr[2]; 
            const uint16_t p3 = srcPtr[3]; 

            dstPtr[0] = (p0 >> 6) & 0xFF;                               
            dstPtr[1] = ((p0 & 0x3F) << 2) | ((p1 >> 12) & 0x03);       
            dstPtr[2] = (p1 >> 4) & 0xFF;                               
            dstPtr[3] = ((p1 & 0x0F) << 4) | ((p2 >> 10) & 0x0F);       
            dstPtr[4] = (p2 >> 2) & 0xFF;                               
            dstPtr[5] = ((p2 & 0x03) << 6) | ((p3 >> 8) & 0x3F);       
            dstPtr[6] = p3 & 0xFF;                                      

            srcPtr += 4;
            dstPtr += 7;
        }
    }
    data = std::move(packed_data); 
}

std::tuple<std::vector<uint8_t>, std::array<unsigned short, 4>, unsigned short> preprocessData(
    std::vector<uint8_t>& data, 
    uint32_t& inOutWidth,
    uint32_t& inOutHeight,
    const CameraFrameMetadata& metadata,
    const CameraConfiguration& cameraConfiguration,
    const std::array<uint8_t, 4>& cfa, 
    uint32_t scale, 
    bool applyShadingMap=true,
    bool normaliseShadingMapOption=false) 
{
    Measure m("preprocessData");

    if (data.empty() || inOutWidth == 0 || inOutHeight == 0) {
        spdlog::error("preprocessData: Input data is empty or dimensions are zero.");
        return {{}, cameraConfiguration.blackLevel, static_cast<unsigned short>(cameraConfiguration.whiteLevel)};
    }
    if (data.size() < static_cast<size_t>(inOutWidth) * inOutHeight * sizeof(uint16_t)) {
        spdlog::error("preprocessData: Input data size does not match dimensions.");
        return {{}, cameraConfiguration.blackLevel, static_cast<unsigned short>(cameraConfiguration.whiteLevel)};
    }

    uint32_t input_original_width = inOutWidth; 

    scale = std::max(1u, scale);
    if (scale > 1) {
        scale = (scale / 2) * 2;
        if (scale == 0) scale = 2; 
    }

    uint32_t newWidth = inOutWidth / scale;
    uint32_t newHeight = inOutHeight / scale;

    newWidth = (newWidth / 4) * 4;
    newHeight = (newHeight / 2) * 2; 

    if (newWidth == 0 || newHeight == 0) {
        spdlog::error("preprocessData: Calculated new dimensions are zero after scaling/alignment.");
        return {{}, cameraConfiguration.blackLevel, static_cast<unsigned short>(cameraConfiguration.whiteLevel)};
    }

    const auto& srcBlackLevelConfig = cameraConfiguration.blackLevel;
    const float srcWhiteLevelConfig = cameraConfiguration.whiteLevel;

    // Default to source levels initially
    std::array<unsigned short, 4> dstBlackLevel = srcBlackLevelConfig;
    float dstWhiteLevel = srcWhiteLevelConfig;
    int original_data_bits = bitsNeeded(static_cast<unsigned short>(srcWhiteLevelConfig));

    // Local copy of lens shading map for potential modification
    auto processedLensShadingMap = metadata.lensShadingMap; // Make a copy

    // Determine if we need to increase processing precision
    bool increasePrecision = false;
    int target_processing_bits = original_data_bits; // Default to original

    // <<<<< MODIFIED LOGIC HERE >>>>>
    // Increase precision to 14-bit if "Scale RAW" (normaliseShadingMapOption) is active,
    // OR if applying vignette without "Scale RAW" but original data is < 14-bit.
    if (normaliseShadingMapOption) { // "Scale RAW" is checked
        increasePrecision = true;
        target_processing_bits = 14;
        spdlog::debug("preprocessData: 'Scale RAW' active, targeting 14-bit processing.");
    } else if (applyShadingMap) { // "Scale RAW" is NOT checked, but "Vignette Correction" is
        spdlog::debug("preprocessData: Only 'Vignette' active (no 'Scale RAW'). Original bits: {}", original_data_bits);
        if (original_data_bits < 14) { // If original is < 14 bit (e.g. 10 or 12)
            target_processing_bits = std::min(16, original_data_bits + 2); // e.g. 10->12, 12->14
            if (target_processing_bits > original_data_bits) {
                increasePrecision = true;
                spdlog::debug("preprocessData: Vignette only, original bits < 14. Target bits: {}", target_processing_bits);
            }
        } else { // Original is >= 14-bit
            target_processing_bits = std::min(16, original_data_bits + 2); // Add headroom if possible
             if (target_processing_bits > original_data_bits) {
                increasePrecision = true;
                spdlog::debug("preprocessData: Vignette only, original bits >= 14. Target bits: {}", target_processing_bits);
            }
        }
    }
    // <<<<< END OF MODIFIED LOGIC FOR PRECISION TARGET >>>>>


    if (increasePrecision) {
        int useBits = target_processing_bits; // Use the determined target
        useBits = std::min(16, useBits);      // Cap at 16

        // Only update if useBits is actually greater than original_data_bits
        // OR if normaliseShadingMapOption is true (because it implies a rescaling that needs new levels for the target bit depth)
        if (useBits > original_data_bits || (normaliseShadingMapOption && useBits >= original_data_bits) ) {
            dstWhiteLevel = std::pow(2.0f, useBits) - 1;
            spdlog::debug("preprocessData: Increased precision. useBits: {}, new dstWhiteLevel: {}", useBits, dstWhiteLevel);

            if (useBits > original_data_bits) {
                int black_level_shift_amount = useBits - original_data_bits;
                spdlog::debug("preprocessData: Shifting black levels by {} bits.", black_level_shift_amount);
                for(size_t i=0; i < dstBlackLevel.size(); ++i) { 
                    unsigned long long shifted_v = static_cast<unsigned long long>(srcBlackLevelConfig[i]) << black_level_shift_amount;
                    unsigned short max_black_heuristic = static_cast<unsigned short>(dstWhiteLevel / 4.0f); // Cap at 25% of white
                    dstBlackLevel[i] = static_cast<unsigned short>(std::min(shifted_v, static_cast<unsigned long long>(max_black_heuristic)));
                }
            } else {
                 // If useBits == original_data_bits, but normaliseShadingMapOption was true,
                 // we are processing at the original bit depth but with a normalized map.
                 // dstBlackLevel remains srcBlackLevelConfig, dstWhiteLevel is set to original_data_bits range.
                 // This case should be fine as dstWhiteLevel would be pow(2,original_data_bits)-1.
                 // No black level shift needed *due to bit depth change*.
                 dstBlackLevel = srcBlackLevelConfig; // Ensure it's based on original if no bit depth change
                 // dstWhiteLevel would be correctly set if useBits == original_data_bits (e.g. 14-bit original, 14-bit target)
                 // This actually is implicitly handled by dstWhiteLevel = pow(2.0f, useBits) -1
            }
        } else {
            // No precision increase was triggered, or target_processing_bits was not > original_data_bits
            // Keep original dstBlackLevel and dstWhiteLevel
             spdlog::debug("preprocessData: No precision increase. Using original levels. Original bits: {}, Target bits: {}", original_data_bits, target_processing_bits);
        }
    } else {
        // No precision increase requested by logic, ensure levels are original
        dstBlackLevel = srcBlackLevelConfig;
        dstWhiteLevel = srcWhiteLevelConfig;
        spdlog::debug("preprocessData: No precision increase triggered. Using original levels.");
    }


    // The actual normalization of the shading map gains
    if(applyShadingMap && normaliseShadingMapOption && !processedLensShadingMap.empty() && 
       (processedLensShadingMap.size() > 0 && !processedLensShadingMap[0].empty()) ) { // Check inner too
        normalizeShadingMap(processedLensShadingMap);
        spdlog::debug("preprocessData: Lens shading map normalized.");
    }

    // Linear gain calculation should now use the potentially updated dstBlackLevel and dstWhiteLevel
    const std::array<float, 4> linearGain = {
        (srcWhiteLevelConfig > srcBlackLevelConfig[0] && (srcWhiteLevelConfig - srcBlackLevelConfig[0]) > 1e-5f) ? (dstWhiteLevel - dstBlackLevel[0]) / (srcWhiteLevelConfig - srcBlackLevelConfig[0]) : 1.0f,
        (srcWhiteLevelConfig > srcBlackLevelConfig[1] && (srcWhiteLevelConfig - srcBlackLevelConfig[1]) > 1e-5f) ? (dstWhiteLevel - dstBlackLevel[1]) / (srcWhiteLevelConfig - srcBlackLevelConfig[1]) : 1.0f,
        (srcWhiteLevelConfig > srcBlackLevelConfig[2] && (srcWhiteLevelConfig - srcBlackLevelConfig[2]) > 1e-5f) ? (dstWhiteLevel - dstBlackLevel[2]) / (srcWhiteLevelConfig - srcBlackLevelConfig[2]) : 1.0f,
        (srcWhiteLevelConfig > srcBlackLevelConfig[3] && (srcWhiteLevelConfig - srcBlackLevelConfig[3]) > 1e-5f) ? (dstWhiteLevel - dstBlackLevel[3]) / (srcWhiteLevelConfig - srcBlackLevelConfig[3]) : 1.0f
    };


    const int fullWidth = metadata.originalWidth > 0 ? metadata.originalWidth : inOutWidth;
    const int fullHeight = metadata.originalHeight > 0 ? metadata.originalHeight : inOutHeight;
    const int cropOffsetX = (fullWidth - inOutWidth) / 2;  
    const int cropOffsetY = (fullHeight - inOutHeight) / 2;
    const float shadingMapScaleX = (fullWidth > 0) ? 1.0f / static_cast<float>(fullWidth) : 0.0f;
    const float shadingMapScaleY = (fullHeight > 0) ? 1.0f / static_cast<float>(fullHeight) : 0.0f;

    std::vector<uint8_t> dst_byte_buffer;
    dst_byte_buffer.resize(static_cast<size_t>(newWidth) * newHeight * sizeof(uint16_t));
    uint16_t* dstData = reinterpret_cast<uint16_t*>(dst_byte_buffer.data());
    uint16_t* srcData = reinterpret_cast<uint16_t*>(data.data()); 

    size_t dstOffset = 0;

    for (uint32_t y_dst = 0; y_dst < newHeight; y_dst += 2) { 
        for (uint32_t x_dst = 0; x_dst < newWidth; x_dst += 2) {
            uint32_t srcY_tl = y_dst * scale;
            uint32_t srcX_tl = x_dst * scale;

            if (srcY_tl + 1 >= inOutHeight || srcX_tl + 1 >= input_original_width) {
                 // This can happen if inOutHeight/inOutWidth was not multiple of scale*2 or scale*4
                 // Fill with black or skip. For now, skip to avoid out of bounds.
                 // This might leave parts of the dst image unwritten if newWidth/newHeight calculation
                 // based on inOutWidth/Height/scale leads to src coords > original inOutWidth/Height.
                 // The alignment of newWidth/newHeight should prevent this for the *destination* loop,
                 // but source access needs care.
                 // Let's ensure srcX_tl and srcY_tl are valid for a 2x2 block.
                 if (srcY_tl >= inOutHeight -1 || srcX_tl >= input_original_width -1) continue;
            }


            uint16_t s_raw[4] = {
                srcData[srcY_tl * input_original_width + srcX_tl],         
                srcData[srcY_tl * input_original_width + (srcX_tl + 1)],   
                srcData[(srcY_tl + 1) * input_original_width + srcX_tl],   
                srcData[(srcY_tl + 1) * input_original_width + (srcX_tl + 1)] 
            };

            float processed_pixels_float[4];

            for (int i = 0; i < 4; ++i) { 
                int bayer_dx = (i % 2); 
                int bayer_dy = (i / 2); 
                int cfa_channel_index = cfa[i]; 

                float shading_gain = 1.0f;
                if (applyShadingMap && !processedLensShadingMap.empty() && 
                    (processedLensShadingMap.size() > 0 && !processedLensShadingMap[0].empty()) && // Check inner too
                    metadata.lensShadingMapWidth > 0 && metadata.lensShadingMapHeight > 0) {
                    
                    float norm_x_full_sensor = (static_cast<float>(srcX_tl + bayer_dx) + cropOffsetX) * shadingMapScaleX;
                    float norm_y_full_sensor = (static_cast<float>(srcY_tl + bayer_dy) + cropOffsetY) * shadingMapScaleY;
                    
                    int lsc_channel_for_pixel = i; 
                    if (static_cast<size_t>(lsc_channel_for_pixel) < processedLensShadingMap.size()){
                         shading_gain = getShadingMapValue(norm_x_full_sensor, norm_y_full_sensor, lsc_channel_for_pixel,
                                                      processedLensShadingMap, metadata.lensShadingMapWidth, metadata.lensShadingMapHeight);
                    } else {
                        if (static_cast<size_t>(cfa_channel_index) < processedLensShadingMap.size()) {
                             shading_gain = getShadingMapValue(norm_x_full_sensor, norm_y_full_sensor, cfa_channel_index,
                                                      processedLensShadingMap, metadata.lensShadingMapWidth, metadata.lensShadingMapHeight);
                        }
                    }
                }

                float val = static_cast<float>(s_raw[i]) - srcBlackLevelConfig[i]; 
                val *= linearGain[i]; 
                val *= shading_gain;  
                val += dstBlackLevel[i]; 

                processed_pixels_float[i] = val;
            }

            dstData[dstOffset]                 = static_cast<unsigned short>(std::clamp(std::round(processed_pixels_float[0]), 0.0f, dstWhiteLevel));
            dstData[dstOffset + 1]             = static_cast<unsigned short>(std::clamp(std::round(processed_pixels_float[1]), 0.0f, dstWhiteLevel));
            dstData[dstOffset + newWidth]      = static_cast<unsigned short>(std::clamp(std::round(processed_pixels_float[2]), 0.0f, dstWhiteLevel));
            dstData[dstOffset + newWidth + 1]  = static_cast<unsigned short>(std::clamp(std::round(processed_pixels_float[3]), 0.0f, dstWhiteLevel));

            dstOffset += 2; 
        }
        dstOffset += newWidth; 
    }

    inOutWidth = newWidth;
    inOutHeight = newHeight;

    spdlog::debug("preprocessData: Final dstBlackLevels: [{},{},{},{}], dstWhiteLevel: {}",
        dstBlackLevel[0], dstBlackLevel[1], dstBlackLevel[2], dstBlackLevel[3], dstWhiteLevel);

    return std::make_tuple(std::move(dst_byte_buffer), dstBlackLevel, static_cast<unsigned short>(dstWhiteLevel));
}


std::shared_ptr<std::vector<char>> generateDng(
    std::vector<uint8_t>& data, 
    const CameraFrameMetadata& metadata,
    const CameraConfiguration& cameraConfiguration,
    float recordingFps,
    int frameNumber,
    FileRenderOptions options,
    int draftScale) 
{
    Measure m("generateDng");

    unsigned int width = metadata.width;
    unsigned int height = metadata.height;

    std::array<uint8_t, 4> cfa_for_processing_and_dng_pattern;

    if(cameraConfiguration.sensorArrangement == "rggb") {
        cfa_for_processing_and_dng_pattern = { 0, 1, 1, 2 }; 
    } else if(cameraConfiguration.sensorArrangement == "bggr") {
        cfa_for_processing_and_dng_pattern = { 2, 1, 1, 0 }; 
    } else if(cameraConfiguration.sensorArrangement == "grbg") {
        cfa_for_processing_and_dng_pattern = { 1, 0, 2, 1 }; 
    } else if(cameraConfiguration.sensorArrangement == "gbrg") {
        cfa_for_processing_and_dng_pattern = { 1, 2, 0, 1 }; 
    } else {
        spdlog::error("Invalid sensor arrangement: {}", cameraConfiguration.sensorArrangement);
        throw std::runtime_error("Invalid sensor arrangement");
    }

    bool applyShadingMapFlag = (options & RENDER_OPT_APPLY_VIGNETTE_CORRECTION) != 0;
    bool normalizeShadingMapFlag = (options & RENDER_OPT_NORMALIZE_SHADING_MAP) != 0;
    int current_scale_factor = (options & RENDER_OPT_DRAFT) ? draftScale : 1;

    std::vector<uint8_t> frame_pixel_data_copy = data; 

    auto [processedPixelDataBytes, dstBlackLevel, dstWhiteLevel] = utils::preprocessData(
        frame_pixel_data_copy, 
        width, height,         
        metadata,
        cameraConfiguration,
        cfa_for_processing_and_dng_pattern, 
        current_scale_factor,
        applyShadingMapFlag, normalizeShadingMapFlag);

    if (processedPixelDataBytes.empty() || width == 0 || height == 0) {
        spdlog::error("Preprocessing failed or resulted in empty image.");
        return nullptr;
    }

    spdlog::debug("DNG Gen: Original Dims {}x{}, Processed Dims {}x{}", metadata.width, metadata.height, width, height);
    spdlog::debug("DNG Gen: Black levels after preprocess: {},{},{},{}. White level: {}",
                  dstBlackLevel[0], dstBlackLevel[1], dstBlackLevel[2], dstBlackLevel[3], dstWhiteLevel);

    auto bits_for_encoding = bitsNeeded(dstWhiteLevel);
    unsigned short final_dng_bits_per_sample = 16; 
    spdlog::debug("DNG Gen: Bits needed for whitelevel {}: {}", dstWhiteLevel, bits_for_encoding);


    if(bits_for_encoding <= 10 && width % 4 == 0) { 
        utils::encodeTo10Bit(processedPixelDataBytes, width, height);
        final_dng_bits_per_sample = 10;
    }
    else if(bits_for_encoding <= 12 && width % 2 == 0) { 
        utils::encodeTo12Bit(processedPixelDataBytes, width, height);
        final_dng_bits_per_sample = 12;
    }
    else if(bits_for_encoding <= 14 && width % 4 == 0) { 
        utils::encodeTo14Bit(processedPixelDataBytes, width, height);
        final_dng_bits_per_sample = 14;
    }
    else { 
        final_dng_bits_per_sample = std::max((unsigned short)1, bits_for_encoding); 
        final_dng_bits_per_sample = std::min((unsigned short)16, final_dng_bits_per_sample); 
        // If no packing happened, data in processedPixelDataBytes is still effectively uint16_t values.
        // The DNG writer will expect data in the format corresponding to BitsPerSample.
        // If final_dng_bits_per_sample is e.g. 14, but data is uint16_t, tinyDNG should handle it.
        spdlog::debug("DNG Gen: No packing or packing skipped due to width constraints. Effective bits: {}", final_dng_bits_per_sample);
    }
    spdlog::debug("DNG Gen: Final DNG BitsPerSample: {}", final_dng_bits_per_sample);


    tinydngwriter::DNGImage dng;
    dng.SetBigEndian(false); 
    dng.SetDNGVersion(1, 4, 0, 0); 
    dng.SetDNGBackwardVersion(1, 1, 0, 0);

    dng.SetImageData(reinterpret_cast<const unsigned char*>(processedPixelDataBytes.data()), processedPixelDataBytes.size());
    dng.SetImageWidth(width);
    dng.SetImageLength(height);
    dng.SetPlanarConfig(tinydngwriter::PLANARCONFIG_CONTIG); 
    dng.SetPhotometric(tinydngwriter::PHOTOMETRIC_CFA);      
    dng.SetRowsPerStrip(height); 
    dng.SetSamplesPerPixel(1);   

    dng.SetCFARepeatPatternDim(2, 2);
    dng.SetCFAPattern(4, cfa_for_processing_and_dng_pattern.data());
    dng.SetCFALayout(1); 


    dng.SetBlackLevelRepeatDim(2, 2); 
    dng.SetBlackLevel(4, dstBlackLevel.data());
    dng.SetWhiteLevel(dstWhiteLevel);

    dng.SetCompression(tinydngwriter::COMPRESSION_NONE);

    dng.SetIso(static_cast<float>(metadata.iso)); 
    dng.SetExposureTime(metadata.exposureTime / 1e9); 


    if (recordingFps > 0) {
        float time_secs = static_cast<float>(frameNumber) / recordingFps;
        int hours = static_cast<int>(std::floor(time_secs / 3600));
        int minutes = (static_cast<int>(std::floor(time_secs / 60))) % 60;
        int seconds = static_cast<int>(std::floor(time_secs)) % 60;
        
        int frames_tc = (recordingFps > 1) ? (frameNumber % static_cast<int>(std::round(recordingFps))) : 0;
        // A more precise way for frames_tc if frameNumber is truly sequential and starts from 0 for the clip
        // frames_tc = static_cast<int>(std::round(fmod(time_secs * recordingFps, recordingFps)));
        // However, DNG spec timecode is more about HH:MM:SS:FF within the recording.
        // The frameNumber % round(recordingFps) is a common way to get the FF part.

        std::array<uint8_t, 8> timeCodeBytes = {0}; 
        timeCodeBytes[0] = ToTimecodeByte(frames_tc); 
        timeCodeBytes[1] = ToTimecodeByte(seconds);   
        timeCodeBytes[2] = ToTimecodeByte(minutes);   
        timeCodeBytes[3] = ToTimecodeByte(hours);     
        dng.SetTimeCode(timeCodeBytes.data());
        dng.SetFrameRate(recordingFps);
    }

    const unsigned short dng_bps[1] = { final_dng_bits_per_sample };
    dng.SetBitsPerSample(1, dng_bps);

    dng.SetColorMatrix1(3, cameraConfiguration.colorMatrix1.data());
    dng.SetColorMatrix2(3, cameraConfiguration.colorMatrix2.data());
    dng.SetForwardMatrix1(3, cameraConfiguration.forwardMatrix1.data());
    dng.SetForwardMatrix2(3, cameraConfiguration.forwardMatrix2.data());
    dng.SetAsShotNeutral(3, metadata.asShotNeutral.data());
    dng.SetCalibrationIlluminant1(getColorIlluminant(cameraConfiguration.colorIlluminant1));
    dng.SetCalibrationIlluminant2(getColorIlluminant(cameraConfiguration.colorIlluminant2));

    const auto software = "MotionCam Tools";
    dng.SetSoftware(software);
    dng.SetUniqueCameraModel(cameraConfiguration.extraData.postProcessSettings.metadata.buildModel.empty() ?
                             "Unknown Camera" : cameraConfiguration.extraData.postProcessSettings.metadata.buildModel);

    dng.SetSubfileType(0);

    const uint32_t activeArea[4] = { 0, 0, height, width }; 
    dng.SetActiveArea(&activeArea[0]);

    std::string err_msg;
    tinydngwriter::DNGWriter writer(false); 
    writer.AddImage(&dng);

    auto output_dng_data = std::make_shared<std::vector<char>>();
    size_t estimated_size = static_cast<size_t>(width) * height * (final_dng_bits_per_sample / 8.0) + 1024 * 100; 
    output_dng_data->reserve(estimated_size);

    boost::iostreams::back_insert_device<std::vector<char>> sink(*output_dng_data);
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::vector<char>>> out_stream(sink);

    if (!writer.WriteToFile(out_stream, &err_msg)) {
        spdlog::error("Failed to write DNG to memory: {}", err_msg);
        return nullptr;
    }
    out_stream.flush(); 

    return output_dng_data;
}


int gcd(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return std::abs(a); 
}

std::pair<int, int> toFraction(float frameRate, int base) {
    if (base <= 0) base = 1000; 
    if (frameRate <= 1e-6f) { 
        return std::make_pair(0, 1);
    }

    double num_double = std::round(frameRate * base);
    int numerator = static_cast<int>(num_double);
    int denominator = base;

    if (numerator == 0 && frameRate > 0) { 
        numerator = 1; 
        denominator = static_cast<int>(std::round(1.0 / frameRate));
        if (denominator == 0) denominator = 100000; 
    }

    if (numerator != 0) { 
        int common_divisor = gcd(numerator, denominator);
        if (common_divisor != 0) { 
            numerator /= common_divisor;
            denominator /= common_divisor;
        }
    } else if (denominator == 0) { 
        return std::make_pair(0,1); 
    }

    return std::make_pair(numerator, denominator);
}

} // namespace utils
} // namespace motioncam