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
} // namespace

void RenderScene::fillDefaultMaterials(nvvkgltf::Scene& scene)
{
    this->_defaultMaterials.clear();
    this->_defaultMaterials.resize(scene.getModel().materials.size());
    this->_defaultMaterials.assign(scene.getModel().materials.size(), FixedMaterial::Create());
}

bool GaussianScene::load(const std::filesystem::path& filename)
{
    _vertices.clear();
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
                _vertices.clear();
                _meta = {};
                return false;
            }
        }

        if (isVertexElement)
        {
            std::array<uint32_t, 14> propIdxs{};
            if (!reader.find_properties(propIdxs.data(), static_cast<uint32_t>(propIdxs.size()), "x", "y", "z", "f_dc_0", "f_dc_1", "f_dc_2",
                                        "opacity", "scale_0", "scale_1", "scale_2", "rot_0", "rot_1", "rot_2", "rot_3"))
            {
                _vertices.clear();
                _meta = {};
                return false;
            }

            const uint32_t rowCount = reader.num_rows();
            if (rowCount == 0)
            {
                _vertices.clear();
                _meta = {};
                return false;
            }

            _vertices.resize(rowCount);
            if (!reader.extract_properties_with_stride(propIdxs.data(), static_cast<uint32_t>(propIdxs.size()), PLYPropertyType::Float,
                                                       _vertices.data(), static_cast<uint32_t>(sizeof(GaussianVertex))))
            {
                _vertices.clear();
                _meta = {};
                return false;
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
        _vertices.clear();
        _meta = {};
        return false;
    }

    _meta.splatCount = static_cast<uint32_t>(_vertices.size());

    const VkDeviceSize bufferSize = _vertices.size() * sizeof(GaussianVertex);
    _splatBuffer  = Buffer::Create("GaussianSplatBuffer", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, bufferSize,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto* manager = &PlayResourceManager::Instance();
    manager->appendBuffer(*_splatBuffer, 0, std::span(_vertices));

    _splatMetaBuffer = Buffer::Create("GaussianSplatMetaBuffer", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      sizeof(GaussianSceneMeta), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    manager->appendBuffer(*_splatMetaBuffer, 0, std::span(&_meta, 1));

    VkCommandBuffer cmd = manager->getTempCommandBuffer();
    manager->cmdUploadAppended(cmd);
    manager->submitAndWaitTempCmdBuffer(cmd);

    return !_vertices.empty();
}
} // namespace Play
