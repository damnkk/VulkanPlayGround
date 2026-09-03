#include "Asset.h"
#include "core/ProjectPaths.h"

namespace Play
{

void Asset::onLoadAsset()
{
    ProjectInfo::ensureFilePathInProject(_filePath, getAssetResourceDirectory(), _uid);
}

} // namespace Play
