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
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
      padding: 10px;
      color: #f4f7fb;
      cursor: pointer;
    }

    .control-reset {
      width: 26px;
      height: 26px;
      box-sizing: border-box;
      border: 1px solid #384456;
      border-radius: 4px;
      padding: 0;
      flex: 0 0 auto;
      background: #1c2532;
      color: #b7c2d4;
      cursor: pointer;
    }

    .control-reset:hover {
      border-color: #4b95ca;
      color: #ffffff;
    }

    .property-list {
      display: grid;
      gap: 6px;
      margin-top: 10px;
    }

    .property-row {
      min-width: 0;
      display: grid;
      grid-template-columns: minmax(84px, 1fr) minmax(96px, 168px);
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

    .property-row input.property-error {
      border-color: #d35b5b;
    }

    .property-row input[data-editor-drag="number"] {
      cursor: ew-resize;
    }

    .property-row input[data-editor-drag="number"].editing {
      cursor: text;
    }

    .property-row input.dragging {
      border-color: #4b95ca;
      background: #152334;
    }

    body.dragging-number {
      cursor: ew-resize;
      user-select: none;
    }

    .vector-input {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 4px;
    }

    .vector-input.vector-size-2 {
      grid-template-columns: repeat(2, minmax(0, 1fr));
    }

    .vector-input.vector-size-3 {
      grid-template-columns: repeat(3, minmax(0, 1fr));
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

    function getPropertyInputValue(input) {
      return input.type === "checkbox" ? (input.checked ? "true" : "false") : input.value;
    }

    async function commitPropertyInput(input) {
      const objectView = input.closest("[data-editor-object-id]");
      if (!objectView || !input.dataset.editorProperty || !window.setEditorProperty) {
        return;
      }

      input.classList.remove("property-error");
      try {
        await window.setEditorProperty(
          objectView.dataset.editorObjectId,
          input.dataset.editorProperty,
          getPropertyInputValue(input)
        );
        if (input.type === "checkbox") {
          input.defaultChecked = input.checked;
        } else {
          input.defaultValue = input.value;
        }
      } catch (error) {
        input.classList.add("property-error");
      }
    }

    function applyInputDefault(input) {
      if (input.type === "checkbox") {
        input.checked = input.dataset.editorDefault === "true";
        input.defaultChecked = input.checked;
      } else {
        input.value = input.dataset.editorDefault ?? "";
        input.defaultValue = input.value;
      }

      input.classList.remove("property-error");
    }

    async function resetObjectInput(button) {
      const objectView = button.closest("[data-editor-object-id]");
      if (!objectView || !window.resetEditorObject) {
        return;
      }

      button.disabled = true;
      try {
        await window.resetEditorObject(objectView.dataset.editorObjectId);
        for (const input of objectView.querySelectorAll("[data-editor-property]")) {
          applyInputDefault(input);
        }
      } finally {
        button.disabled = false;
      }
    }

    function scheduleCommitPropertyInput(input) {
      if (input.dataset.commitQueued) {
        return;
      }

      input.dataset.commitQueued = "true";
      requestAnimationFrame(() => {
        delete input.dataset.commitQueued;
        commitPropertyInput(input);
      });
    }

    function getDatasetNumber(input, name) {
      const value = Number(input.dataset[name]);
      return Number.isFinite(value) ? value : null;
    }

    function clampEditorNumber(input, value) {
      const min = getDatasetNumber(input, "editorMin");
      const max = getDatasetNumber(input, "editorMax");
      if (min !== null && value < min) {
        return min;
      }

      if (max !== null && value > max) {
        return max;
      }

      return value;
    }

    function getEditorStep(input, event) {
      let step = getDatasetNumber(input, "editorStep") ?? 0.01;
      if (event.shiftKey) {
        step *= 10.0;
      }

      if (event.altKey || event.ctrlKey) {
        step *= 0.1;
      }

      return step;
    }

    function getStepPrecision(step) {
      const text = String(step);
      const exponent = text.match(/e-(\d+)/i);
      if (exponent) {
        return Number(exponent[1]);
      }

      const dot = text.indexOf(".");
      return dot < 0 ? 0 : text.length - dot - 1;
    }

    function formatEditorNumber(value, step) {
      const precision = Math.min(8, Math.max(0, getStepPrecision(step) + 1));
      return Number(value.toFixed(precision)).toString();
    }

    function setEditingNumberInput(input, editing) {
      input.classList.toggle("editing", editing);
      if (editing) {
        input.focus();
        input.select();
      }
    }

    function setupDraggableNumberInput(input) {
      input.addEventListener("dblclick", (event) => {
        event.preventDefault();
        setEditingNumberInput(input, true);
      });

      input.addEventListener("focus", () => {
        if (!document.body.classList.contains("dragging-number")) {
          input.classList.add("editing");
        }
      });

      input.addEventListener("blur", () => {
        input.classList.remove("editing");
        commitPropertyInput(input);
      });

      input.addEventListener("keydown", (event) => {
        if (event.key === "Enter") {
          input.blur();
        } else if (event.key === "Escape") {
          input.value = input.defaultValue;
          input.blur();
        }
      });

      input.addEventListener("change", () => commitPropertyInput(input));

      input.addEventListener("pointerdown", (event) => {
        if (event.button !== 0 || input.classList.contains("editing")) {
          return;
        }

        event.preventDefault();
        const startValue = Number(input.value);
        const drag = {
          moved: false,
          startValue: Number.isFinite(startValue) ? startValue : 0.0,
          startX: event.clientX
        };

        input.setPointerCapture(event.pointerId);
        input.classList.add("dragging");
        document.body.classList.add("dragging-number");

        const move = (moveEvent) => {
          const dx = moveEvent.clientX - drag.startX;
          if (!drag.moved && Math.abs(dx) < 2) {
            return;
          }

          drag.moved = true;
          const step = getEditorStep(input, moveEvent);
          const value = clampEditorNumber(input, drag.startValue + dx * step);
          input.value = formatEditorNumber(value, step);
          scheduleCommitPropertyInput(input);
          moveEvent.preventDefault();
        };

        const stop = (stopEvent) => {
          if (input.hasPointerCapture(stopEvent.pointerId)) {
            input.releasePointerCapture(stopEvent.pointerId);
          }

          input.classList.remove("dragging");
          document.body.classList.remove("dragging-number");
          input.removeEventListener("pointermove", move);
          input.removeEventListener("pointerup", stop);
          input.removeEventListener("pointercancel", stop);
          if (drag.moved) {
            commitPropertyInput(input);
          }
        };

        input.addEventListener("pointermove", move);
        input.addEventListener("pointerup", stop);
        input.addEventListener("pointercancel", stop);
      });
    }

    for (const input of document.querySelectorAll("[data-editor-property]")) {
      if (input.dataset.editorDrag === "number") {
        setupDraggableNumberInput(input);
      } else {
        const eventName = input.type === "checkbox" ? "input" : "change";
        input.addEventListener(eventName, () => commitPropertyInput(input));
      }
    }

    for (const button of document.querySelectorAll("[data-editor-reset-object]")) {
      button.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        resetObjectInput(button);
      });
    }
  </script>
</body>
</html>
)html";

    return html;
}

} // namespace Play::editor
