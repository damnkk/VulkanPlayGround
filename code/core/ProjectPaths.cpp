#include "ProjectPaths.h"

#include <nvutils/file_operations.hpp>

namespace Play
{

std::filesystem::path getBaseFilePath()
{
#ifdef TARGET_EXE_TO_SOURCE_DIRECTORY
    const std::filesystem::path exeDirectory = nvutils::getExecutablePath().parent_path();
    const std::filesystem::path basePath     = exeDirectory / TARGET_EXE_TO_SOURCE_DIRECTORY;
    std::error_code             errorCode;
    const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(basePath, errorCode);
    return errorCode ? basePath.lexically_normal() : canonicalPath;
#else
    return std::filesystem::current_path();
#endif
}

std::string           ProjectInfo::_projectName;
std::filesystem::path ProjectInfo::_projectPath;

bool ProjectInfo::setProjectPath(const std::filesystem::path& projectPath)
{
    if (projectPath.empty())
    {
        clear();
        return false;
    }

    std::filesystem::path projectDirectory = projectPath;
    if (projectDirectory.extension() == ".project")
    {
        projectDirectory = projectDirectory.parent_path();
    }

    std::error_code             errorCode;
    const std::filesystem::path absolutePath = std::filesystem::absolute(projectDirectory, errorCode);
    if (!errorCode)
    {
        projectDirectory = absolutePath;
    }

    projectDirectory              = projectDirectory.lexically_normal();
    const std::string projectName = projectDirectory.filename().string();
    if (projectDirectory.empty() || projectName.empty())
    {
        clear();
        return false;
    }

    _projectPath = std::move(projectDirectory);
    _projectName = projectName;
    return true;
}

void ProjectInfo::clear()
{
    _projectName.clear();
    _projectPath.clear();
}

bool ProjectInfo::isOpen()
{
    return !_projectPath.empty();
}

const std::string& ProjectInfo::getProjectName()
{
    return _projectName;
}

const std::filesystem::path& ProjectInfo::getProjectPath()
{
    return _projectPath;
}

std::filesystem::path ProjectInfo::getAssetMapPath()
{
    return _projectPath.empty() ? std::filesystem::path{} : _projectPath / "assetmap.json";
}
} // namespace Play
