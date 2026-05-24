#include "RasterGpuScene.h"

namespace Play
{

void RasterGpuScene::clear()
{
    _transforms.clear();
    _materials.clear();
    _textureInfos.clear();
    _submeshes.clear();
    _drawItems.clear();
    _bindlessTextures.clear();
    _textureInfos.push_back(shaderio::defaultGltfTextureInfo());
    _header              = {};
    _sourceSceneRevision = 0;
}

void RasterGpuScene::rebuild(const CpuScene& scene, AssetRegistry& assets)
{
    _transforms.clear();
    _materials.clear();
    _textureInfos.clear();
    _submeshes.clear();
    _drawItems.clear();
    _textureInfos.push_back(shaderio::defaultGltfTextureInfo());
    _header = {};

    const std::vector<CpuSceneNode>& nodes = scene.getNodes();
    for (uint32_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
    {
        const CpuSceneNode& node = nodes[nodeIndex];
        if (!node.alive || node.type != CpuSceneNodeType::eNode3D || !node.worldVisible)
        {
            continue;
        }

        const CpuModelComponent* modelComponent = nullptr;
        for (CpuSceneComponentID componentID : node.components)
        {
            modelComponent = scene.getComponent<CpuModelComponent>(componentID);
            if (modelComponent)
            {
                break;
            }
        }

        if (!modelComponent || !modelComponent->visible || !modelComponent->hasModel())
        {
            continue;
        }

        const ModelAsset* model = assets.getModel(modelComponent->model);
        if (!model)
        {
            continue;
        }

        uint32_t firstRenderable = 0;
        uint32_t renderableEnd   = static_cast<uint32_t>(model->renderables.size());
        if (!modelComponent->usesAllRenderables())
        {
            firstRenderable = modelComponent->firstRenderable;
            renderableEnd   = firstRenderable + modelComponent->renderableCount;
            if (renderableEnd > model->renderables.size())
            {
                renderableEnd = static_cast<uint32_t>(model->renderables.size());
            }
        }

        for (uint32_t renderableIndex = firstRenderable; renderableIndex < renderableEnd; ++renderableIndex)
        {
            const ModelRenderableTemplate& renderable = model->renderables[renderableIndex];
            if (renderable.submeshIndex >= model->submeshes.size())
            {
                continue;
            }

            const ModelSubmeshAsset& submesh = model->submeshes[renderable.submeshIndex];

            glm::mat4 renderableLocalToModel = renderable.localToModel;
            if (renderable.nodeIndex < model->nodes.size())
            {
                renderableLocalToModel = model->nodes[renderable.nodeIndex].modelTransform;
            }

            RasterGpuTransform transform;
            transform.objectToWorld     = node.worldTransform * renderableLocalToModel;
            transform.worldToObject     = glm::inverse(transform.objectToWorld);
            transform.prevObjectToWorld = transform.objectToWorld;

            const uint32_t transformID = static_cast<uint32_t>(_transforms.size());
            _transforms.push_back(transform);

            const uint32_t submeshID  = appendSubmesh(submesh);
            const uint32_t materialID = appendMaterial(submesh.material, assets);

            RasterGpuDrawItem drawItem;
            drawItem.transformID = transformID;
            drawItem.submeshID   = submeshID;
            drawItem.materialID  = materialID;
            drawItem.nodeID      = nodeIndex;
            _drawItems.push_back(drawItem);
        }
    }

    _header.drawCount        = static_cast<uint32_t>(_drawItems.size());
    _header.materialCount    = static_cast<uint32_t>(_materials.size());
    _header.textureInfoCount = static_cast<uint32_t>(_textureInfos.size());
    _header.submeshCount     = static_cast<uint32_t>(_submeshes.size());
    _header.transformCount   = static_cast<uint32_t>(_transforms.size());

    _sourceSceneRevision = scene.getRevision();
}

uint32_t RasterGpuScene::ensureBindlessTexture(TextureAssetID textureID, AssetRegistry& assets)
{
    TextureAsset* texture = assets.getTexture(textureID);
    if (!texture)
    {
        return INVALID_SCENE_ID;
    }

    if (!texture->texture && !texture->sourcePath.empty())
    {
        texture->texture = RefPtr<Texture>(
            new Texture(texture->sourcePath, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture->mipLevels, texture->isSrgb));
    }

    if (!texture->isResident())
    {
        return INVALID_SCENE_ID;
    }

    if (texture->bindlessIndex != INVALID_SCENE_ID && texture->bindlessIndex < _bindlessTextures.size())
    {
        return texture->bindlessIndex;
    }

    texture->bindlessIndex = static_cast<uint32_t>(_bindlessTextures.size());
    _bindlessTextures.push_back(texture->texture);
    return texture->bindlessIndex;
}

uint32_t RasterGpuScene::appendMaterial(MaterialAssetID materialID, AssetRegistry& assets)
{
    RasterGpuMaterial gpuMaterial = shaderio::defaultGltfMaterial();

    const MaterialAsset* material = assets.getMaterial(materialID);
    if (material)
    {
        gpuMaterial = material->parameters;
        gpuMaterial.pbrBaseColorTexture             = appendTextureInfo(material->textures.baseColor, assets);
        gpuMaterial.normalTexture                   = appendTextureInfo(material->textures.normal, assets);
        gpuMaterial.pbrMetallicRoughnessTexture     = appendTextureInfo(material->textures.metallicRoughness, assets);
        gpuMaterial.emissiveTexture                 = appendTextureInfo(material->textures.emissive, assets);
        gpuMaterial.transmissionTexture             = appendTextureInfo(material->textures.transmission, assets);
        gpuMaterial.thicknessTexture                = appendTextureInfo(material->textures.thickness, assets);
        gpuMaterial.clearcoatTexture                = appendTextureInfo(material->textures.clearcoat, assets);
        gpuMaterial.clearcoatRoughnessTexture       = appendTextureInfo(material->textures.clearcoatRoughness, assets);
        gpuMaterial.clearcoatNormalTexture          = appendTextureInfo(material->textures.clearcoatNormal, assets);
        gpuMaterial.specularTexture                 = appendTextureInfo(material->textures.specular, assets);
        gpuMaterial.specularColorTexture            = appendTextureInfo(material->textures.specularColor, assets);
        gpuMaterial.iridescenceTexture              = appendTextureInfo(material->textures.iridescence, assets);
        gpuMaterial.iridescenceThicknessTexture     = appendTextureInfo(material->textures.iridescenceThickness, assets);
        gpuMaterial.anisotropyTexture               = appendTextureInfo(material->textures.anisotropy, assets);
        gpuMaterial.sheenColorTexture               = appendTextureInfo(material->textures.sheenColor, assets);
        gpuMaterial.sheenRoughnessTexture           = appendTextureInfo(material->textures.sheenRoughness, assets);
        gpuMaterial.occlusionTexture                = appendTextureInfo(material->textures.occlusion, assets);
        gpuMaterial.pbrDiffuseTexture               = appendTextureInfo(material->textures.pbrDiffuse, assets);
        gpuMaterial.pbrSpecularGlossinessTexture    = appendTextureInfo(material->textures.pbrSpecularGlossiness, assets);
        gpuMaterial.diffuseTransmissionTexture      = appendTextureInfo(material->textures.diffuseTransmission, assets);
        gpuMaterial.diffuseTransmissionColorTexture = appendTextureInfo(material->textures.diffuseTransmissionColor, assets);
    }

    const uint32_t materialIndex = static_cast<uint32_t>(_materials.size());
    _materials.push_back(gpuMaterial);
    return materialIndex;
}

uint16_t RasterGpuScene::appendTextureInfo(const MaterialTextureSlot& slot, AssetRegistry& assets)
{
    if (!slot.hasTexture())
    {
        return 0;
    }

    const uint32_t bindlessIndex = ensureBindlessTexture(slot.texture, assets);
    if (bindlessIndex == INVALID_SCENE_ID || _textureInfos.size() >= 0xFFFF)
    {
        return 0;
    }

    shaderio::GltfTextureInfo textureInfo = shaderio::defaultGltfTextureInfo();
    textureInfo.index    = static_cast<int>(bindlessIndex);
    textureInfo.texCoord = slot.texCoord;
    textureInfo.uvTransform =
        shaderio::float3x2(slot.uvTransform[0][0], slot.uvTransform[1][0], slot.uvTransform[0][1], slot.uvTransform[1][1],
                           slot.uvTransform[0][2], slot.uvTransform[1][2]);

    const uint16_t textureInfoIndex = static_cast<uint16_t>(_textureInfos.size());
    _textureInfos.push_back(textureInfo);
    return textureInfoIndex;
}

uint32_t RasterGpuScene::appendSubmesh(const ModelSubmeshAsset& submesh)
{
    RasterGpuSubmesh gpuSubmesh;
    gpuSubmesh.firstIndex = submesh.firstIndex;
    gpuSubmesh.indexCount = submesh.indexCount;
    gpuSubmesh.vertexBase = submesh.vertexBase;
    gpuSubmesh.boundsMin  = submesh.boundsMin;
    gpuSubmesh.boundsMax  = submesh.boundsMax;

    const uint32_t submeshIndex = static_cast<uint32_t>(_submeshes.size());
    _submeshes.push_back(gpuSubmesh);
    return submeshIndex;
}

} // namespace Play
