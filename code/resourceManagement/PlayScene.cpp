#include "PlayScene.h"
#include "Material.h"
#include <array>
#include <nvvkgltf/scene.hpp>
#include <string>
#include <nvutils/parallel_work.hpp>

namespace Play
{

void GaussianScene::convertCoordinates(spz::CoordinateSystem from, spz::CoordinateSystem to)
{
    spz::CoordinateConverter c         = coordinateConverter(from, to);
    const auto               numPoints = _positions.size();
    for (size_t i = 0; i < _positions.size(); ++i)
    {
        _positions[i].x *= c.flipP[0];
        _positions[i].y *= c.flipP[1];
        _positions[i].z *= c.flipP[2];
    }
    for (size_t i = 0; i < _rotations.size(); i += 4)
    {
        // Don't modify the scalar component (index 0)
        _rotations[i + 1] *= c.flipQ[0];
        _rotations[i + 2] *= c.flipQ[1];
        _rotations[i + 3] *= c.flipQ[2];
    }

    const size_t numCoeffs         = _shRestCoefficients.size() / 3;
    const size_t numCoeffsPerPoint = numCoeffs / numPoints;
    size_t       idx               = 0;
    for (size_t i = 0; i < numPoints; ++i)
    {
        // Process R, G, and B coefficients for each point
        for (size_t j = 0; j < numCoeffsPerPoint; ++j)
        {
            const auto flip = c.flipSh[j];
            _shRestCoefficients[idx + j] *= flip;                         // R
            _shRestCoefficients[idx + numCoeffsPerPoint + j] *= flip;     // G
            _shRestCoefficients[idx + numCoeffsPerPoint * 2 + j] *= flip; // B
        }
        idx += 3 * numCoeffsPerPoint;
    }
}

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

bool collectIndexedProperties(const miniply::PLYElement* elem, const char* prefix, std::vector<uint32_t>& outIdx)
{
    outIdx.clear();
    if (elem == nullptr || prefix == nullptr) return false;

    const std::string                          prefixString(prefix);
    std::vector<std::pair<uint32_t, uint32_t>> numberedProps;
    numberedProps.reserve(elem->properties.size());

    for (uint32_t propIndex = 0; propIndex < static_cast<uint32_t>(elem->properties.size()); propIndex++)
    {
        const std::string& propName = elem->properties[propIndex].name;
        if (propName.rfind(prefixString, 0) != 0) continue;

        const char* numberBegin = propName.c_str() + prefixString.size();
        const char* numberEnd   = propName.c_str() + propName.size();
        if (numberBegin == numberEnd) continue;

        uint32_t indexedSuffix = 0;
        const auto [ptr, ec]   = std::from_chars(numberBegin, numberEnd, indexedSuffix);
        if (ec != std::errc() || ptr != numberEnd) continue;

        numberedProps.emplace_back(indexedSuffix, propIndex);
    }

    if (numberedProps.empty()) return false;

    std::sort(numberedProps.begin(), numberedProps.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

    outIdx.reserve(numberedProps.size());
    for (const auto& [_, propIndex] : numberedProps)
    {
        outIdx.push_back(propIndex);
    }

    return true;
}

bool extractFloatComponents(miniply::PLYReader& reader, const std::vector<uint32_t>& propIdxs, std::vector<float>& outComps)
{
    if (propIdxs.empty()) return false;

    const uint32_t rowCount = reader.num_rows();
    outComps.resize(static_cast<size_t>(rowCount) * propIdxs.size());
    return reader.extract_properties(propIdxs.data(), static_cast<uint32_t>(propIdxs.size()), PLYPropertyType::Float, outComps.data());
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
        _covariances.clear();
        _rotations.clear();
        _shRestCoefficients.clear();
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
            _colors.resize(rowCount, float4(1.0f));

            std::vector<float3> scales(rowCount, float3(1.0f));
            _rotations.assign(static_cast<size_t>(rowCount) * 4, 0.0f);
            for (size_t i = 0; i < rowCount; i++)
            {
                _rotations[i * 4 + 3] = 1.0f;
            }

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
                for (size_t i = 0; i < _colors.size(); i++)
                {
                    const size_t base = i * 3;
                    _colors[i].x      = components[base + 0];
                    _colors[i].y      = components[base + 1];
                    _colors[i].z      = components[base + 2];
                }
            }

            std::vector<uint32_t> shRestPropIdxs;
            if (collectIndexedProperties(elem, "f_rest_", shRestPropIdxs))
            {
                if (!extractFloatComponents(reader, shRestPropIdxs, _shRestCoefficients))
                {
                    resetSceneData();
                    return false;
                }
            }

            const std::array<std::array<const char*, 1>, 2> opacityPatterns = {
                std::array<const char*, 1>{"opacity"},
                std::array<const char*, 1>{"alpha"},
            };
            if (extractFloatComponents(reader, elem, opacityPatterns, components))
            {
                for (size_t i = 0; i < _colors.size(); i++)
                {
                    _colors[i].w = components[i];
                }
            }

            nvutils::parallel_batches<512>(rowCount,
                                           [&](uint32_t i)
                                           {
                                               const float SH_C0 = 0.28209479177387814f;
                                               _colors[i].x      = glm::clamp(0.5f + SH_C0 * _colors[i].x, 0.0f, 1.0f);
                                               _colors[i].y      = glm::clamp(0.5f + SH_C0 * _colors[i].y, 0.0f, 1.0f);
                                               _colors[i].z      = glm::clamp(0.5f + SH_C0 * _colors[i].z, 0.0f, 1.0f);
                                               _colors[i].w      = glm::clamp(1.0f / (1.0f + std::exp(-_colors[i].w)), 0.0f, 1.0f);
                                           });

            const std::array<std::array<const char*, 3>, 3> scalePatterns = {
                std::array<const char*, 3>{"scale_0", "scale_1", "scale_2"},
                std::array<const char*, 3>{"scale_x", "scale_y", "scale_z"},
                std::array<const char*, 3>{"sx", "sy", "sz"},
            };
            if (extractFloatComponents(reader, elem, scalePatterns, components))
            {
                unpackFloat3(components, scales);
            }

            const std::array<std::array<const char*, 4>, 3> rotationPatterns = {
                std::array<const char*, 4>{"rot_0", "rot_1", "rot_2", "rot_3"},
                std::array<const char*, 4>{"rotation_0", "rotation_1", "rotation_2", "rotation_3"},
                std::array<const char*, 4>{"qx", "qy", "qz", "qw"},
            };
            if (extractFloatComponents(reader, elem, rotationPatterns, components))
            {
                _rotations = components;
            }

            convertCoordinates(spz::CoordinateSystem::RDF, spz::CoordinateSystem::RUB);

            _covariances.resize(rowCount * 6);
            nvutils::parallel_batches<512>(rowCount,
                                           [&](uint32_t i)
                                           {
                                               glm::vec3      scale{std::exp(scales[i].x), std::exp(scales[i].y), std::exp(scales[i].z)};
                                               const uint32_t rotationBase = i * 4;
                                               glm::quat      rotation{_rotations[rotationBase + 0], _rotations[rotationBase + 1],
                                                                  _rotations[rotationBase + 2], _rotations[rotationBase + 3]};
                                               rotation = glm::normalize(rotation);

                                               const glm::mat3 scaleMatrix           = glm::mat3(glm::scale(scale));
                                               const glm::mat3 rotationMatrix        = glm::mat3_cast(rotation);
                                               const glm::mat3 covarianceMatrix      = rotationMatrix * scaleMatrix;
                                               glm::mat3       transformedCovariance = covarianceMatrix * glm::transpose(covarianceMatrix);

                                               const uint32_t stride6    = i * 6;
                                               _covariances[stride6 + 0] = glm::value_ptr(transformedCovariance)[0];
                                               _covariances[stride6 + 1] = glm::value_ptr(transformedCovariance)[3];
                                               _covariances[stride6 + 2] = glm::value_ptr(transformedCovariance)[6];
                                               _covariances[stride6 + 3] = glm::value_ptr(transformedCovariance)[4];
                                               _covariances[stride6 + 4] = glm::value_ptr(transformedCovariance)[7];
                                               _covariances[stride6 + 5] = glm::value_ptr(transformedCovariance)[8];
                                           });

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
    _positionBuffer = RefPtr<Buffer>(new Buffer("GaussianSplatPositionBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                positionBufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    manager->appendBuffer(*_positionBuffer, 0, std::span(_positions));

    const VkDeviceSize colorBufferSize = _colors.size() * sizeof(float4);
    _colorBuffer = RefPtr<Buffer>(new Buffer("GaussianSplatColorBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                             colorBufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    manager->appendBuffer(*_colorBuffer, 0, std::span(_colors));

    const VkDeviceSize covarianceBufferSize = _covariances.size() * sizeof(float);
    _covarianceBuffer =
        RefPtr<Buffer>(new Buffer("GaussianSplatCovarianceBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  covarianceBufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    manager->appendBuffer(*_covarianceBuffer, 0, std::span(_covariances));

    const VkDeviceSize shRestBufferSize = std::max<VkDeviceSize>(sizeof(float), _shRestCoefficients.size() * sizeof(float));
    _shRestBuffer = RefPtr<Buffer>(new Buffer("GaussianSplatShRestBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                              shRestBufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    manager->appendBuffer(*_shRestBuffer, 0, std::span(_shRestCoefficients));

    _splatMetaBuffer = RefPtr<Buffer>(new Buffer("GaussianSplatMetaBuffer", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                 sizeof(GaussianSceneMeta), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    manager->appendBuffer(*_splatMetaBuffer, 0, std::span(&_meta, 1));

    VkCommandBuffer cmd = manager->getTempCommandBuffer();
    manager->cmdUploadAppended(cmd);
    manager->submitAndWaitTempCmdBuffer(cmd);
    _sceneUniformBuffer =
        RefPtr<Buffer>(new Buffer("GaussianSplatSceneUniformBuffer", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  sizeof(GaussianSceneUniform), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

    GaussianSceneUniform sceneUniform{};

    sceneUniform.colorBufferDeviceAddress      = _colorBuffer->address;
    sceneUniform.covarianceBufferDeviceAddress = _covarianceBuffer->address;
    sceneUniform.positionBufferDeviceAddress   = _positionBuffer->address;
    sceneUniform.shBufferDeviceAddress         = _shRestBuffer->address;
    sceneUniform.metaDataAddress               = _splatMetaBuffer->address;
    sceneUniform.colorStride                   = uint32_t(sizeof(float4));
    sceneUniform.positionStride                = uint32_t(sizeof(float3));
    sceneUniform.covarianceStride              = uint32_t(sizeof(float3) * 2);
    sceneUniform.shStride                      = uint32_t(_shRestCoefficients.size() / this->getVertexCount());
    memcpy(_sceneUniformBuffer->mapping, &sceneUniform, sizeof(GaussianSceneUniform));
    // PlayResourceManager::Instance().flushBuffer(*_sceneUniformBuffer, 0, VK_WHOLE_SIZE);

    return !_positions.empty();
}
} // namespace Play
