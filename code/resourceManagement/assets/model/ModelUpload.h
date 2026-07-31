#ifndef MODEL_UPLOAD_H
#define MODEL_UPLOAD_H

#include "ModelGpuAssets.h"
#include "resourceManagement/assets/upload/AssetGpuUploader.h"
#include "resourceManagement/scene/cpu/CpuScene.h"
#include <VPGLoader/Model.hpp>

namespace Play
{

// Uploads renderer resources directly from an immutable VPGLoader model.
// It owns the source while the asynchronous upload is in flight and is consumed only
// after AssetGpuUploader has waited for its submission to complete.
class ModelUploadJob final : public AssetGpuUploadJob
{
public:
    ModelUploadJob(ModelLoadRequestID requestID, vpgloader::ModelHandle source);

    bool build(AssetGpuUploadContext& context, std::string& message) override;

    ModelLoadRequestID getRequestID() const
    {
        return _requestID;
    }

    UploadedModel takeUploadedModel();

private:
    bool uploadTextures(AssetGpuUploadContext& context, std::string& message);
    bool uploadGeometry(AssetGpuUploadContext& context, std::string& message);
    bool uploadMetadata(AssetGpuUploadContext& context, std::string& message);

    ModelLoadRequestID                  _requestID;
    vpgloader::ModelHandle _source;
    UploadedModel                        _uploadedModel;
};

} // namespace Play

#endif // MODEL_UPLOAD_H
