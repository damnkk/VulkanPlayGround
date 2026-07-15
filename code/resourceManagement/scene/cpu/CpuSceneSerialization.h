#ifndef CPU_SCENE_SERIALIZATION_H
#define CPU_SCENE_SERIALIZATION_H

#include "resourceManagement/scene/cpu/CpuScene.h"

namespace Play
{

// The project asset table is the persistent registry for resources referenced by a
// scene. Asset paths identify loader inputs today and can later point at imported
// binary payloads without changing component references.
enum class ProjectAssetType : uint32_t
{
    eModel
};

struct ProjectAssetRecord
{
    std::string guid;
    std::string      sourcePath;
    ProjectAssetType type = ProjectAssetType::eModel;
};

class ProjectAssetTable
{
public:
    bool set(const ProjectAssetRecord& record);
    void clear();

    const ProjectAssetRecord* find(const std::string& guid) const;

    const std::vector<ProjectAssetRecord>& getRecords() const
    {
        return _records;
    }

private:
    std::vector<ProjectAssetRecord> _records;
};

// Creates a persistent, globally unique asset identity. Registration is owned by
// SceneManager, which writes the project asset table while holding its scene lock.
std::string generateProjectAssetGuid();

// A .project path identifies a directory package containing the scene and asset JSON documents.
bool createProjectArchive(const std::string& projectPath, std::string* errorMessage = nullptr);

bool saveProjectArchive(const CpuScene& scene, const ProjectAssetTable& assets, const std::string& projectPath, std::string* errorMessage = nullptr);

// Reads a project package, validates every model GUID against its asset table, and only then
// replaces the destination CPU scene and project asset table. No binary assets are loaded here.
bool loadProjectArchive(CpuScene& scene, ProjectAssetTable& assets, const std::string& projectPath, std::string* errorMessage = nullptr);

} // namespace Play

#endif // CPU_SCENE_SERIALIZATION_H
