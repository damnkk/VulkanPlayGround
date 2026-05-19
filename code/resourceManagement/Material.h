#ifndef MATERIAL_H
#define MATERIAL_H

#include "PlayProgram.h"
#include "Resource.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <rttr/rttr_enable.h>

namespace Play
{

enum class MaterialType : uint32_t
{
    eFixed,
    eCustom,
    eCount
};

class Material
{
public:
    Material(std::string name = "Default Material") : _name(name) {};
    virtual ~Material() = default;
    PlayProgram* getProgram()
    {
        return _program;
    }

    RTTR_ENABLE()

protected:
    std::string  _name;
    PlayProgram* _program = nullptr;
};

class FixedMaterial : public Material
{
public:
    ~FixedMaterial();

    static FixedMaterial* Create();

    RTTR_ENABLE(Material)

private:
    FixedMaterial();
};

class CustomMaterial : public Material
{
public:
    CustomMaterial(const std::string& name = "Custom Material");
    void setProgram(PlayProgram* program)
    {
        _program = program;
    }
    ~CustomMaterial();

    RTTR_ENABLE(Material)
};

} // namespace Play

#endif // MATERIAL_H
