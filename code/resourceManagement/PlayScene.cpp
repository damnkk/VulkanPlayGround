#include "PlayScene.h"
#include "Material.h"
#include <array>
#include <nvvkgltf/scene.hpp>
#include <string>

namespace Play
{
namespace
{
using miniply::kInvalidIndex;
using miniply::PLYPropertyType;

bool extractFirstProperties(miniply::PLYReader& reader, uint32_t count, PLYPropertyType destType, void* dst)
{
    if (reader.num_rows() != 1 || reader.element() == nullptr || reader.element()->properties.size() < count) return false;

    std::vector<uint32_t> propIdxs(count);
    for (uint32_t i = 0; i < count; i++) propIdxs[i] = i;

    return reader.extract_properties(propIdxs.data(), count, destType, dst);
}

bool extractIndexedProperties(miniply::PLYReader& reader, const miniply::PLYElement* elem, const char* prefix, uint32_t count,
                              PLYPropertyType destType, void* dst)
{
    if (reader.num_rows() != 1 || elem == nullptr) return false;

    std::vector<uint32_t> propIdxs(count);
    for (uint32_t i = 0; i < count; i++)
    {
        std::string name = std::string(prefix) + "_" + std::to_string(i);
        propIdxs[i]      = elem->find_property(name.c_str());
        if (propIdxs[i] == kInvalidIndex) return false;
    }

    return reader.extract_properties(propIdxs.data(), count, destType, dst);
}

bool extractNamedPairU32(miniply::PLYReader& reader, const miniply::PLYElement* elem, uint32_t outPair[2])
{
    if (reader.num_rows() != 1 || elem == nullptr) return false;

    const uint32_t widthIdx  = elem->find_property("width");
    const uint32_t heightIdx = elem->find_property("height");
    if (widthIdx == kInvalidIndex || heightIdx == kInvalidIndex) return false;

    return reader.extract_properties(&widthIdx, 1, PLYPropertyType::UInt, &outPair[0]) &&
           reader.extract_properties(&heightIdx, 1, PLYPropertyType::UInt, &outPair[1]);
}

bool extractNamedPairF32(miniply::PLYReader& reader, const miniply::PLYElement* elem, const char* a, const char* b, float outPair[2])
{
    if (reader.num_rows() != 1 || elem == nullptr) return false;

    const uint32_t idxA = elem->find_property(a);
    const uint32_t idxB = elem->find_property(b);
    if (idxA == kInvalidIndex || idxB == kInvalidIndex) return false;

    return reader.extract_properties(&idxA, 1, PLYPropertyType::Float, &outPair[0]) &&
           reader.extract_properties(&idxB, 1, PLYPropertyType::Float, &outPair[1]);
}

bool isMetaCandidate(const miniply::PLYElement* elem)
{
    if (elem == nullptr) return false;

    if (elem->name == "extrinsic" || elem->name == "intrinsic" || elem->name == "image_size" || elem->name == "frame" || elem->name == "disparity" ||
        elem->name == "color_space" || elem->name == "colorspace" || elem->name == "version")
        return true;

    return elem->find_property("extrinsic_0") != kInvalidIndex || elem->find_property("intrinsic_0") != kInvalidIndex ||
           elem->find_property("image_size_0") != kInvalidIndex || elem->find_property("frame_0") != kInvalidIndex ||
           elem->find_property("disparity_0") != kInvalidIndex || elem->find_property("color_space") != kInvalidIndex ||
           elem->find_property("colorspace") != kInvalidIndex || elem->find_property("version_0") != kInvalidIndex ||
           elem->find_property("width") != kInvalidIndex || elem->find_property("height") != kInvalidIndex ||
           elem->find_property("min_disparity") != kInvalidIndex || elem->find_property("max_disparity") != kInvalidIndex;
}

void parseMetaFromCurrentElement(miniply::PLYReader& reader, const miniply::PLYElement* elem, GaussianSceneMeta& meta)
{
    if (elem == nullptr) return;

    if (elem->name == "extrinsic")
    {
        extractIndexedProperties(reader, elem, "extrinsic", 16, PLYPropertyType::Float, meta.extrinsic) ||
            extractFirstProperties(reader, 16, PLYPropertyType::Float, meta.extrinsic);
        return;
    }
    if (elem->name == "intrinsic")
    {
        extractIndexedProperties(reader, elem, "intrinsic", 9, PLYPropertyType::Float, meta.intrinsic) ||
            extractFirstProperties(reader, 9, PLYPropertyType::Float, meta.intrinsic);
        return;
    }
    if (elem->name == "image_size")
    {
        extractIndexedProperties(reader, elem, "image_size", 2, PLYPropertyType::UInt, meta.imageSize) ||
            extractNamedPairU32(reader, elem, meta.imageSize) || extractFirstProperties(reader, 2, PLYPropertyType::UInt, meta.imageSize);
        return;
    }
    if (elem->name == "frame")
    {
        extractIndexedProperties(reader, elem, "frame", 2, PLYPropertyType::Int, meta.frame) ||
            extractFirstProperties(reader, 2, PLYPropertyType::Int, meta.frame);
        return;
    }
    if (elem->name == "disparity")
    {
        extractIndexedProperties(reader, elem, "disparity", 2, PLYPropertyType::Float, meta.disparity) ||
            extractNamedPairF32(reader, elem, "min_disparity", "max_disparity", meta.disparity) ||
            extractFirstProperties(reader, 2, PLYPropertyType::Float, meta.disparity);
        return;
    }
    if (elem->name == "color_space" || elem->name == "colorspace")
    {
        const uint32_t colorIdx = elem->find_property("color_space");
        const uint32_t idx      = (colorIdx != kInvalidIndex) ? colorIdx : elem->find_property("colorspace");
        if (idx != kInvalidIndex && reader.num_rows() == 1)
            reader.extract_properties(&idx, 1, PLYPropertyType::UChar, &meta.colorSpace);
        else
            extractFirstProperties(reader, 1, PLYPropertyType::UChar, &meta.colorSpace);
        return;
    }
    if (elem->name == "version")
    {
        extractIndexedProperties(reader, elem, "version", 3, PLYPropertyType::UChar, meta.version) ||
            extractFirstProperties(reader, 3, PLYPropertyType::UChar, meta.version);
        return;
    }

    // Combined metadata element fallback (all values stored in one row).
    extractIndexedProperties(reader, elem, "extrinsic", 16, PLYPropertyType::Float, meta.extrinsic);
    extractIndexedProperties(reader, elem, "intrinsic", 9, PLYPropertyType::Float, meta.intrinsic);
    extractIndexedProperties(reader, elem, "image_size", 2, PLYPropertyType::UInt, meta.imageSize);
    extractIndexedProperties(reader, elem, "frame", 2, PLYPropertyType::Int, meta.frame);
    extractIndexedProperties(reader, elem, "disparity", 2, PLYPropertyType::Float, meta.disparity);
    extractIndexedProperties(reader, elem, "version", 3, PLYPropertyType::UChar, meta.version);

    const uint32_t colorIdx = elem->find_property("color_space");
    const uint32_t csIdx    = (colorIdx != kInvalidIndex) ? colorIdx : elem->find_property("colorspace");
    if (csIdx != kInvalidIndex && reader.num_rows() == 1) reader.extract_properties(&csIdx, 1, PLYPropertyType::UChar, &meta.colorSpace);

    extractNamedPairU32(reader, elem, meta.imageSize);
    extractNamedPairF32(reader, elem, "min_disparity", "max_disparity", meta.disparity);
}

template <size_t N, size_t K>
bool findPropertyPattern(const miniply::PLYElement* elem, const std::array<std::array<const char*, N>, K>& patterns, std::array<uint32_t, N>& outIdx)
{
    if (elem == nullptr) return false;

    for (const auto& pattern : patterns)
    {
        bool matched = true;
        for (size_t i = 0; i < N; i++)
        {
            outIdx[i] = elem->find_property(pattern[i]);
            if (outIdx[i] == kInvalidIndex)
            {
                matched = false;
                break;
            }
        }

        if (matched) return true;
    }

    return false;
}

template <size_t N, size_t K>
bool extractFloatComponents(miniply::PLYReader& reader, const miniply::PLYElement* elem, const std::array<std::array<const char*, N>, K>& patterns,
                            std::vector<float>& outComps)
{
    std::array<uint32_t, N> propIdxs{};
    if (!findPropertyPattern(elem, patterns, propIdxs)) return false;

    const uint32_t rowCount = reader.num_rows();
    outComps.resize(static_cast<size_t>(rowCount) * N);
    return reader.extract_properties(propIdxs.data(), static_cast<uint32_t>(N), PLYPropertyType::Float, outComps.data());
}

void unpackFloat3(const std::vector<float>& src, std::vector<float3>& dst)
{
    const size_t rowCount = dst.size();
    for (size_t i = 0; i < rowCount; i++)
    {
        const size_t base = i * 3;
        dst[i]            = float3(src[base + 0], src[base + 1], src[base + 2]);
    }
}

void unpackFloat4(const std::vector<float>& src, std::vector<float4>& dst)
{
    const size_t rowCount = dst.size();
    for (size_t i = 0; i < rowCount; i++)
    {
        const size_t base = i * 4;
        dst[i]            = float4(src[base + 0], src[base + 1], src[base + 2], src[base + 3]);
    }
}
} // namespace

void RenderScene::fillDefaultMaterials(nvvkgltf::Scene& scene)
{
    this->_defaultMaterials.clear();
    this->_defaultMaterials.resize(scene.getModel().materials.size());
    this->_defaultMaterials.assign(scene.getModel().materials.size(), FixedMaterial::Create());
}

bool GaussianScene::load(const std::filesystem::path& filename)
{
    auto resetSceneData = [this]()
    {
        _positions.clear();
        _colors.clear();
        _opacities.clear();
        _scales.clear();
        _rotations.clear();
        _meta = {};
    };

    resetSceneData();
    _meta = {};

    const std::string  filenameString = filename.string();
    miniply::PLYReader reader(filenameString.c_str());
    if (!reader.valid()) return false;

    bool gotVertices = false;

    while (reader.has_element())
    {
        const miniply::PLYElement* elem = reader.element();
        if (elem == nullptr)
        {
            break;
        }

        const bool isVertexElement = reader.element_is(miniply::kPLYVertexElement);
        const bool metaCandidate   = isMetaCandidate(elem);
        if (isVertexElement || metaCandidate)
        {
            if (!reader.load_element())
            {
                resetSceneData();
                return false;
            }
        }

        if (isVertexElement)
        {
            const uint32_t rowCount = reader.num_rows();
            if (rowCount == 0)
            {
                resetSceneData();
                return false;
            }

            _positions.resize(rowCount);
            _colors.resize(rowCount, float3(1.0f));
            _opacities.resize(rowCount, 1.0f);
            _scales.resize(rowCount, float3(1.0f));
            _rotations.resize(rowCount, float4(0.0f, 0.0f, 0.0f, 1.0f));

            std::vector<float> components;

            const std::array<std::array<const char*, 3>, 3> positionPatterns = {
                std::array<const char*, 3>{"x", "y", "z"},
                std::array<const char*, 3>{"pos_x", "pos_y", "pos_z"},
                std::array<const char*, 3>{"position_0", "position_1", "position_2"},
            };
            if (!extractFloatComponents(reader, elem, positionPatterns, components))
            {
                resetSceneData();
                return false;
            }
            unpackFloat3(components, _positions);

            const std::array<std::array<const char*, 3>, 4> colorPatterns = {
                std::array<const char*, 3>{"f_dc_0", "f_dc_1", "f_dc_2"},
                std::array<const char*, 3>{"red", "green", "blue"},
                std::array<const char*, 3>{"r", "g", "b"},
                std::array<const char*, 3>{"color_0", "color_1", "color_2"},
            };
            if (extractFloatComponents(reader, elem, colorPatterns, components))
            {
                unpackFloat3(components, _colors);
            }

            const std::array<std::array<const char*, 1>, 2> opacityPatterns = {
                std::array<const char*, 1>{"opacity"},
                std::array<const char*, 1>{"alpha"},
            };
            if (extractFloatComponents(reader, elem, opacityPatterns, components))
            {
                for (size_t i = 0; i < _opacities.size(); i++)
                {
                    _opacities[i] = components[i];
                }
            }

            const std::array<std::array<const char*, 3>, 3> scalePatterns = {
                std::array<const char*, 3>{"scale_0", "scale_1", "scale_2"},
                std::array<const char*, 3>{"scale_x", "scale_y", "scale_z"},
                std::array<const char*, 3>{"sx", "sy", "sz"},
            };
            if (extractFloatComponents(reader, elem, scalePatterns, components))
            {
                unpackFloat3(components, _scales);
            }

            const std::array<std::array<const char*, 4>, 3> rotationPatterns = {
                std::array<const char*, 4>{"rot_0", "rot_1", "rot_2", "rot_3"},
                std::array<const char*, 4>{"rotation_0", "rotation_1", "rotation_2", "rotation_3"},
                std::array<const char*, 4>{"qx", "qy", "qz", "qw"},
            };
            if (extractFloatComponents(reader, elem, rotationPatterns, components))
            {
                unpackFloat4(components, _rotations);
            }

            gotVertices = true;
        }
        else if (metaCandidate)
        {
            parseMetaFromCurrentElement(reader, elem, _meta);
        }

        reader.next_element();
    }

    if (!gotVertices)
    {
        resetSceneData();
        return false;
    }

    _meta.splatCount = static_cast<uint32_t>(_positions.size());

    auto* manager = &PlayResourceManager::Instance();

    const VkDeviceSize positionBufferSize = _positions.size() * sizeof(float3);
    _positionBuffer = Buffer::Create("GaussianSplatPositionBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     positionBufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    manager->appendBuffer(*_positionBuffer, 0, std::span(_positions));

    const VkDeviceSize colorBufferSize = _colors.size() * sizeof(float3);
    _colorBuffer = Buffer::Create("GaussianSplatColorBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, colorBufferSize,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    manager->appendBuffer(*_colorBuffer, 0, std::span(_colors));

    const VkDeviceSize opacityBufferSize = _opacities.size() * sizeof(float);
    _opacityBuffer = Buffer::Create("GaussianSplatOpacityBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    opacityBufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    manager->appendBuffer(*_opacityBuffer, 0, std::span(_opacities));

    const VkDeviceSize scaleBufferSize = _scales.size() * sizeof(float3);
    _scaleBuffer = Buffer::Create("GaussianSplatScaleBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, scaleBufferSize,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    manager->appendBuffer(*_scaleBuffer, 0, std::span(_scales));

    const VkDeviceSize rotationBufferSize = _rotations.size() * sizeof(float4);
    _rotationBuffer = Buffer::Create("GaussianSplatRotationBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     rotationBufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    manager->appendBuffer(*_rotationBuffer, 0, std::span(_rotations));

    _splatMetaBuffer = Buffer::Create("GaussianSplatMetaBuffer", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      sizeof(GaussianSceneMeta), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    manager->appendBuffer(*_splatMetaBuffer, 0, std::span(&_meta, 1));

    VkCommandBuffer cmd = manager->getTempCommandBuffer();
    manager->cmdUploadAppended(cmd);
    manager->submitAndWaitTempCmdBuffer(cmd);

    return !_positions.empty();
}
} // namespace Play
