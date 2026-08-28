#include "ProjectPaths.h"

#include <nvutils/file_operations.hpp>

namespace Play
{

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
