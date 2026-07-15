#ifndef MODEL_UPLOAD_H
#define MODEL_UPLOAD_H

#include "ModelGpuAssets.h"
#include "resourceManagement/assets/upload/AssetGpuUploader.h"

namespace Play
{

// Converts the CPU-only LoadedModel product into the GPU-only UploadedModel product.
// It owns the source while the asynchronous upload is in flight and is consumed only
// after AssetGpuUploader has waited for its submission to complete.
class ModelUploadJob final : public AssetGpuUploadJob
{
public:
    ModelUploadJob(ModelLoadRequestID requestID, std::shared_ptr<const LoadedModel> source);

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
    std::shared_ptr<const LoadedModel>   _source;
    UploadedModel                        _uploadedModel;
};

} // namespace Play

#endif // MODEL_UPLOAD_H
