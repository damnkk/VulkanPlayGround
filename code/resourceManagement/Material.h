#ifndef MATERIAL_H
#define MATERIAL_H

#include "PlayProgram.h"
#include "Resource.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

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

protected:
    std::string  _name;
    PlayProgram* _program = nullptr;
};

class FixedMaterial : public Material
{
public:
    ~FixedMaterial();

    static FixedMaterial* Create();

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
};

} // namespace Play

#endif // MATERIAL_H