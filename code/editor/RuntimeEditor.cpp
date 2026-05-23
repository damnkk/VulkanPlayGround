#include "editor/RuntimeEditor.h"

namespace Play::editor
{

RuntimeEditor::RuntimeEditor() : _mainBar(_runtimeContext), _renderModeTabs(_runtimeContext, _editorRegistry)
{
    _renderModeTabs.addRenderMode("defer", "Defer", EditorRenderMode::Defer);
    _renderModeTabs.addRenderMode("gaussian", "Gaussian", EditorRenderMode::Gaussian);
    _renderModeTabs.addRenderMode("raytrace", "Ray Tracing", EditorRenderMode::Raytrace);
}

void RuntimeEditor::bindRuntime(Play::runtime::VulkanRuntime& runtime, Play::RenderSession& renderSession, const char* activeMode)
{
    _editorRegistry.clear();
    _runtimeContext.bind(runtime, renderSession, _editorRegistry);
    _renderModeTabs.bindRenderSession(renderSession, activeMode);
}

std::string RuntimeEditor::buildHtml() const
{
    std::string html = R"html(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      color-scheme: dark;
      font-family: "Segoe UI", sans-serif;
      background: #111318;
      color: #f4f7fb;
    }

    body {
      margin: 0;
      min-height: 100vh;
      background: #111318;
    }

    button,
    input {
      font: inherit;
    }

    .runtime-editor {
      min-height: 100vh;
      display: grid;
      grid-template-rows: auto 1fr;
    }

    .main-bar {
      min-height: 42px;
      display: flex;
      align-items: stretch;
      border-bottom: 1px solid #2b3342;
      background: #171b24;
    }

    .editor-title {
      min-width: 86px;
      display: flex;
      align-items: center;
      padding: 0 14px;
      border-right: 1px solid #2b3342;
      color: #f4f7fb;
      font-size: 13px;
      font-weight: 650;
    }

    .menu-strip,
    .tab-strip {
      display: flex;
      align-items: center;
      gap: 6px;
      padding: 0 10px;
    }

    .menu-item {
      border: 0;
      border-radius: 4px;
      padding: 7px 10px;
      background: transparent;
      color: #c8d2e2;
      cursor: pointer;
    }

    .menu-item:hover {
      background: #242c3a;
      color: #ffffff;
    }

    .render-mode-tabs {
      min-height: 0;
      display: grid;
      grid-template-rows: auto 1fr;
    }

    .tab-strip {
      min-height: 52px;
      border-bottom: 1px solid #2b3342;
      background: #151922;
    }

    .render-mode-tab {
      min-height: 34px;
      border: 1px solid #3c4658;
      border-radius: 6px;
      padding: 7px 12px;
      background: #202838;
      color: #c8d2e2;
      cursor: pointer;
    }

    .render-mode-tab.active {
      border-color: #4b95ca;
      background: #2a5f8f;
      color: #ffffff;
    }

    .render-mode-pages {
      min-height: 0;
    }

    .render-mode-page {
      min-height: 100%;
      display: none;
      grid-template-rows: minmax(220px, 1fr) auto;
    }

    .render-mode-page.active {
      display: grid;
    }

    .scene-manager-editor {
      min-height: 0;
      display: grid;
      grid-template-columns: minmax(220px, 320px) 1fr;
    }

    .panel {
      min-height: 0;
      padding: 12px;
      border-right: 1px solid #2b3342;
      overflow: auto;
    }

    .inspector {
      border-right: 0;
    }

    h2 {
      margin: 0 0 10px;
      color: #f4f7fb;
      font-size: 13px;
      font-weight: 650;
    }

    .empty {
      color: #8d99ac;
      font-size: 12px;
    }

    .scene-root {
      width: 100%;
      min-height: 42px;
      box-sizing: border-box;
      border: 1px solid #4b95ca;
      border-radius: 6px;
      padding: 9px 10px;
      background: #1f3345;
      color: #f4f7fb;
      text-align: left;
    }

    .object-view,
    .control-unit {
      border: 1px solid #2f394a;
      border-radius: 6px;
      background: #1d2430;
    }

    .object-view,
    .control-content {
      padding: 10px;
    }

    .object-title {
      color: #f4f7fb;
      font-size: 13px;
    }

    .object-type {
      margin-top: 4px;
      color: #8d99ac;
      font-size: 11px;
    }

    .control-panel {
      max-height: min(42vh, 430px);
      padding: 12px;
      border-top: 1px solid #2b3342;
      background: #171b24;
      overflow: auto;
    }

    .control-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
      gap: 10px;
    }

    .control-unit {
      overflow: hidden;
    }

    .control-unit summary {
      min-height: 42px;
      box-sizing: border-box;
      padding: 10px;
      color: #f4f7fb;
      cursor: pointer;
    }

    .property-list {
      display: grid;
      gap: 6px;
      margin-top: 10px;
    }

    .property-row {
      min-width: 0;
      display: grid;
      grid-template-columns: minmax(84px, 1fr) minmax(80px, 140px);
      gap: 8px;
      align-items: center;
      font-size: 12px;
    }

    .property-row label {
      min-width: 0;
      color: #b7c2d4;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .property-row input {
      width: 100%;
      box-sizing: border-box;
      border: 1px solid #384456;
      border-radius: 4px;
      padding: 5px 6px;
      background: #111822;
      color: #f4f7fb;
    }
  </style>
</head>
<body>
  <div class="runtime-editor">
)html";

    _mainBar.appendHtml(html);
    _renderModeTabs.appendHtml(html);

    html += R"html(
  </div>
  <script>
    const tabs = [...document.querySelectorAll("[data-render-mode]")];
    const pages = [...document.querySelectorAll("[data-render-mode-page]")];

    function showRenderMode(renderMode) {
      for (const tab of tabs) {
        tab.classList.toggle("active", tab.dataset.renderMode === renderMode);
      }

      for (const page of pages) {
        page.classList.toggle("active", page.dataset.renderModePage === renderMode);
      }
    }

    for (const tab of tabs) {
      tab.onclick = () => showRenderMode(tab.dataset.renderMode);
    }
  </script>
</body>
</html>
)html";

    return html;
}

} // namespace Play::editor
