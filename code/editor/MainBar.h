#ifndef PLAY_CODE_EDITOR_MAINBAR_H
#define PLAY_CODE_EDITOR_MAINBAR_H


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

#endif // PLAY_CODE_EDITOR_MAINBAR_H
