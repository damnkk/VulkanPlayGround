#pragma once

#include "editor/EditorHtml.h"

namespace Play::editor
{

class EditorRuntimeContext;

class MainBar
{
public:
    explicit MainBar(EditorRuntimeContext& context);

    bool requestNewProject();
    bool requestOpenProject();
    bool requestSaveProject();
    void appendHtml(std::string& html) const;

private:
    EditorRuntimeContext& _context;
};

} // namespace Play::editor
