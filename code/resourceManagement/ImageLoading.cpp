#include "ImageLoading.h"

#include "nvutils/file_operations.hpp"
#include "nvutils/logger.hpp"
#include <OpenImageIO/imageio.h>

namespace Play::ImageLoading
{
namespace
{
bool HasFloatingPointChannel(const OIIO::ImageSpec& spec)
{
    for (int channel = 0; channel < spec.nchannels; ++channel)
    {
        if (spec.channelformat(channel).is_floating_point())
        {
            return true;
        }
    }
    return spec.format.is_floating_point();
}

bool HasLargeIntegerChannel(const OIIO::ImageSpec& spec)
{
    for (int channel = 0; channel < spec.nchannels; ++channel)
    {
        const OIIO::TypeDesc channelFormat = spec.channelformat(channel).scalartype();
        if (!channelFormat.is_floating_point() && channelFormat.basesize() > sizeof(uint16_t))
        {
            return true;
        }
    }
    return !spec.format.is_floating_point() && spec.format.scalartype().basesize() > sizeof(uint16_t);
}

bool HasSixteenBitChannel(const OIIO::ImageSpec& spec)
{
    for (int channel = 0; channel < spec.nchannels; ++channel)
    {
        const OIIO::TypeDesc channelFormat = spec.channelformat(channel).scalartype();
        if (channelFormat.basetype == OIIO::TypeDesc::UINT16 || channelFormat.basetype == OIIO::TypeDesc::INT16)
        {
            return true;
        }
    }
    const OIIO::TypeDesc baseFormat = spec.format.scalartype();
    return baseFormat.basetype == OIIO::TypeDesc::UINT16 || baseFormat.basetype == OIIO::TypeDesc::INT16;
}

template <typename PixelT>
void ExpandChannels(const PixelT* sourcePixels, int sourceComponents, PixelT* targetPixels, int targetComponents, size_t pixelCount,
                    PixelT alphaValue)
{
    for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex)
    {
        const PixelT* source = sourcePixels + pixelIndex * sourceComponents;
        PixelT*       target = targetPixels + pixelIndex * targetComponents;

        if (targetComponents == 1)
        {
            target[0] = source[0];
            continue;
        }

        if (sourceComponents == 1)
        {
            target[0] = source[0];
            target[1] = source[0];
            target[2] = source[0];
            target[3] = alphaValue;
        }
        else if (sourceComponents == 2)
        {
            target[0] = source[0];
            target[1] = source[0];
            target[2] = source[0];
            target[3] = source[1];
        }
        else
        {
            target[0] = source[0];
            target[1] = source[1];
            target[2] = source[2];
            target[3] = sourceComponents > 3 ? source[3] : alphaValue;
        }
    }
}

bool ReadImageData(OIIO::ImageInput& imageInput, const OIIO::ImageSpec& spec, OIIO::TypeDesc dataType, int requiredComponents,
                   std::vector<uint8_t>& pixels)
{
    const int    readComponents = spec.nchannels < requiredComponents ? spec.nchannels : requiredComponents;
    const size_t pixelCount     = static_cast<size_t>(spec.width) * static_cast<size_t>(spec.height);
    const size_t componentSize  = dataType.size();

    std::vector<uint8_t> sourcePixels(pixelCount * static_cast<size_t>(readComponents) * componentSize);
    if (!imageInput.read_image(0, 0, 0, readComponents, dataType, sourcePixels.data()))
    {
        return false;
    }

    if (readComponents == requiredComponents)
    {
        pixels.swap(sourcePixels);
        return true;
    }

    pixels.resize(pixelCount * static_cast<size_t>(requiredComponents) * componentSize);
    switch (dataType.basetype)
    {
        case OIIO::TypeDesc::FLOAT:
            ExpandChannels(reinterpret_cast<const float*>(sourcePixels.data()), readComponents, reinterpret_cast<float*>(pixels.data()),
                           requiredComponents, pixelCount, 1.0f);
            break;
        case OIIO::TypeDesc::UINT16:
            ExpandChannels(reinterpret_cast<const uint16_t*>(sourcePixels.data()), readComponents, reinterpret_cast<uint16_t*>(pixels.data()),
                           requiredComponents, pixelCount, static_cast<uint16_t>(65535));
            break;
        default:
            ExpandChannels(sourcePixels.data(), readComponents, pixels.data(), requiredComponents, pixelCount, static_cast<uint8_t>(255));
            break;
    }

    return true;
}
} // namespace

bool LoadTextureImage(const std::filesystem::path& imagePath, bool isSrgb, LoadedImage& loadedImage)
{
    loadedImage = {};

    const std::string imagePathUtf8 = nvutils::utf8FromPath(imagePath);
    auto              imageInput    = OIIO::ImageInput::open(imagePathUtf8);
    if (!imageInput)
    {
        const std::string error = OIIO::geterror();
        LOGW("Failed to open image with OpenImageIO: %s%s%s\n", imagePathUtf8.c_str(), error.empty() ? "" : ": ", error.c_str());
        return false;
    }

    const OIIO::ImageSpec& spec = imageInput->spec();
    if (spec.width <= 0 || spec.height <= 0 || spec.nchannels <= 0)
    {
        LOGW("Invalid image dimensions or channel count: %s\n", imagePathUtf8.c_str());
        return false;
    }

    const bool isFloatImage = HasFloatingPointChannel(spec) || HasLargeIntegerChannel(spec);
    const bool is16BitImage = !isFloatImage && HasSixteenBitChannel(spec);

    OIIO::TypeDesc dataType;
    int            requiredComponents = spec.nchannels == 1 ? 1 : 4;
    if (isFloatImage)
    {
        dataType           = OIIO::TypeDesc::FLOAT;
        requiredComponents = 4;
        loadedImage.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    else if (is16BitImage)
    {
        dataType           = OIIO::TypeDesc::UINT16;
        loadedImage.format = requiredComponents == 1 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16G16B16A16_UNORM;
    }
    else
    {
        dataType           = OIIO::TypeDesc::UINT8;
        loadedImage.format = requiredComponents == 1 ? VK_FORMAT_R8_UNORM : isSrgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    }

    if (!ReadImageData(*imageInput, spec, dataType, requiredComponents, loadedImage.pixels))
    {
        const std::string error = imageInput->geterror();
        LOGW("Failed to read image with OpenImageIO: %s%s%s\n", imagePathUtf8.c_str(), error.empty() ? "" : ": ", error.c_str());
        return false;
    }

    loadedImage.extent = {static_cast<uint32_t>(spec.width), static_cast<uint32_t>(spec.height)};
    return true;
}
} // namespace Play::ImageLoading
