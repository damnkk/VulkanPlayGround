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

    .panel-head,
    .section-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 8px;
    }

    .panel-head {
      margin-bottom: 10px;
    }

    .panel-head h2 {
      margin: 0;
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

    .scene-tree-list {
      display: grid;
      gap: 2px;
    }

    .scene-tree-line {
      min-width: 0;
      display: flex;
      align-items: center;
      gap: 4px;
    }

    .scene-tree-toggle {
      width: 22px;
      height: 26px;
      box-sizing: border-box;
      border: 0;
      border-radius: 4px;
      padding: 0;
      background: transparent;
      color: #8d99ac;
      cursor: pointer;
    }

    .scene-tree-toggle:disabled {
      color: transparent;
      cursor: default;
    }

    .scene-tree-node {
      min-width: 0;
      min-height: 28px;
      flex: 1 1 auto;
      box-sizing: border-box;
      display: flex;
      align-items: center;
      gap: 7px;
      border: 1px solid transparent;
      border-radius: 4px;
      padding: 4px 7px;
      background: transparent;
      color: #c8d2e2;
      text-align: left;
      cursor: pointer;
    }

    .scene-tree-node:hover {
      background: #202838;
      color: #ffffff;
    }

    .scene-tree-node.selected {
      border-color: #4b95ca;
      background: #1f3345;
      color: #ffffff;
    }

    .scene-node-type {
      min-width: 28px;
      border-radius: 3px;
      padding: 2px 4px;
      background: #263246;
      color: #9fb2cb;
      font-size: 10px;
      text-align: center;
    }

    .scene-node-name {
      min-width: 0;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .scene-tree-children {
      display: none;
      gap: 2px;
      margin-left: 18px;
    }

    .scene-tree-item.expanded > .scene-tree-children {
      display: grid;
    }

    .scene-node-inspector {
      display: none;
      border: 1px solid #2f394a;
      border-radius: 6px;
      padding: 10px;
      background: #1d2430;
    }

    .scene-node-inspector.active {
      display: block;
    }

    .scene-node-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
    }

    .scene-add-component {
      height: 28px;
      box-sizing: border-box;
      border: 1px solid #384456;
      border-radius: 4px;
      padding: 0;
      background: #1c2532;
      color: #d7dfeb;
      cursor: pointer;
    }

    .scene-add-component {
      width: 28px;
    }

    .scene-add-component:hover:not(:disabled) {
      border-color: #4b95ca;
      color: #ffffff;
    }

    .scene-add-component:disabled {
      opacity: 0.42;
      cursor: default;
    }

    .inspector-section {
      margin-top: 12px;
      padding-top: 10px;
      border-top: 1px solid #2f394a;
    }

    .section-title,
    .component-title {
      color: #f4f7fb;
      font-size: 12px;
      font-weight: 650;
    }

    .component-list {
      display: grid;
      gap: 8px;
      margin-top: 8px;
    }

    .component-card {
      border: 1px solid #334055;
      border-radius: 6px;
      padding: 8px;
      background: #17202c;
    }

    .component-info-row {
      min-width: 0;
      display: grid;
      grid-template-columns: minmax(82px, 0.8fr) minmax(0, 1.4fr);
      gap: 8px;
      margin-top: 6px;
      color: #b7c2d4;
      font-size: 12px;
    }

    .component-info-row span:last-child {
      min-width: 0;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
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

    .scene-context-menu,
    .scene-context-submenu {
      min-width: 148px;
      box-sizing: border-box;
      border: 1px solid #384456;
      border-radius: 5px;
      padding: 4px;
      background: #171f2c;
      box-shadow: 0 12px 32px rgb(0 0 0 / 0.35);
      color: #d7dfeb;
      font-size: 12px;
      z-index: 1000;
    }

    .scene-context-menu {
      position: fixed;
      display: none;
    }

    .scene-context-menu.active {
      display: block;
    }

    .scene-context-menu-item {
      position: relative;
      min-height: 28px;
      box-sizing: border-box;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      border-radius: 4px;
      padding: 0 8px;
      cursor: default;
      white-space: nowrap;
    }

    .scene-context-menu-item:hover {
      background: #263246;
      color: #ffffff;
    }

    .scene-context-submenu {
      position: absolute;
      top: -5px;
      left: 100%;
      display: none;
    }

    .scene-context-menu-item:hover > .scene-context-submenu {
      display: block;
    }

    .scene-context-choice {
      width: 100%;
      min-height: 28px;
      box-sizing: border-box;
      display: block;
      border: 0;
      border-radius: 4px;
      padding: 0 8px;
      background: transparent;
      color: inherit;
      text-align: left;
      cursor: pointer;
    }

    .scene-context-choice:hover {
      background: #263246;
      color: #ffffff;
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
  <div class="scene-context-menu" data-scene-context-menu hidden>
    <div class="scene-context-menu-item" data-scene-context-new-node>
      <span>New Node</span><span>&gt;</span>
      <div class="scene-context-submenu">
        <button class="scene-context-choice" type="button" data-scene-context-create="Node3D">3D Node</button>
        <button class="scene-context-choice" type="button" data-scene-context-create="Node2D">2D Node</button>
      </div>
    </div>
  </div>
  <script>
    const tabs = [...document.querySelectorAll("[data-render-mode]")];
    const pages = [...document.querySelectorAll("[data-render-mode-page]")];
    const sceneContextMenu = document.querySelector("[data-scene-context-menu]");
    const sceneContext = { editor: null, nodeKey: "" };

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

    async function commitSceneNodeTransformInput(input) {
      const editor = input.closest("[data-scene-manager-editor]");
      const panel = input.closest("[data-scene-node-panel]");
      if (!editor || !panel || !input.dataset.sceneNodeTransform || !window.setSceneNodeTransform) {
        return;
      }

      input.classList.remove("property-error");
      try {
        await window.setSceneNodeTransform(
          editor.dataset.renderModeId,
          panel.dataset.sceneNodePanel,
          input.dataset.sceneNodeTransform,
          getPropertyInputValue(input)
        );
        input.defaultValue = input.value;
      } catch (error) {
        input.classList.add("property-error");
      }
    }

    function commitInput(input) {
      if (input.dataset.sceneNodeTransform) {
        return commitSceneNodeTransformInput(input);
      }

      return commitPropertyInput(input);
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
        commitInput(input);
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
        commitInput(input);
      });

      input.addEventListener("keydown", (event) => {
        if (event.key === "Enter") {
          input.blur();
        } else if (event.key === "Escape") {
          input.value = input.defaultValue;
          input.blur();
        }
      });

      input.addEventListener("change", () => commitInput(input));

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
            commitInput(input);
          }
        };

        input.addEventListener("pointermove", move);
        input.addEventListener("pointerup", stop);
        input.addEventListener("pointercancel", stop);
      });
    }

)html";

    html += R"html(
    function setupEditorInput(input) {
      if (input.dataset.editorInputReady) {
        return;
      }

      input.dataset.editorInputReady = "true";
      if (input.dataset.editorDrag === "number") {
        setupDraggableNumberInput(input);
      } else {
        const eventName = input.type === "checkbox" ? "input" : "change";
        input.addEventListener(eventName, () => commitInput(input));
      }
    }

    function escapeHtml(value) {
      return String(value)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
    }

    function findDirectChildByClass(element, className) {
      if (!element) {
        return null;
      }

      for (const child of element.children) {
        if (child.classList.contains(className)) {
          return child;
        }
      }
      return null;
    }

    function findSceneTreeItem(editor, nodeKey) {
      for (const item of editor.querySelectorAll("[data-scene-node-item]")) {
        if (item.dataset.sceneNodeItem === nodeKey) {
          return item;
        }
      }
      return null;
    }

    function getSceneTreeToggle(item) {
      const line = findDirectChildByClass(item, "scene-tree-line");
      return line ? line.querySelector("[data-scene-tree-toggle]") : null;
    }

    function getSceneTreeChildren(item) {
      return findDirectChildByClass(item, "scene-tree-children");
    }

    function getSelectedSceneNode(editor) {
      const selected = editor.querySelector("[data-scene-node-select].selected");
      if (selected) {
        return selected.dataset.sceneNodeSelect;
      }

      const first = editor.querySelector("[data-scene-node-select]");
      return first ? first.dataset.sceneNodeSelect : "";
    }

    function hideSceneContextMenu() {
      if (!sceneContextMenu) {
        return;
      }

      sceneContextMenu.classList.remove("active");
      sceneContextMenu.hidden = true;
      sceneContext.editor = null;
      sceneContext.nodeKey = "";
    }

    function showSceneContextMenu(editor, nodeKey, event) {
      if (!sceneContextMenu) {
        return;
      }

      sceneContext.editor = editor;
      sceneContext.nodeKey = nodeKey;
      sceneContextMenu.hidden = false;
      sceneContextMenu.classList.add("active");

      const menuRect = sceneContextMenu.getBoundingClientRect();
      const left = Math.min(event.clientX, Math.max(0, window.innerWidth - menuRect.width - 6));
      const top = Math.min(event.clientY, Math.max(0, window.innerHeight - menuRect.height - 6));
      sceneContextMenu.style.left = `${Math.max(6, left)}px`;
      sceneContextMenu.style.top = `${Math.max(6, top)}px`;
    }

    function selectSceneNode(editor, nodeKey) {
      for (const row of editor.querySelectorAll("[data-scene-node-select]")) {
        row.classList.toggle("selected", row.dataset.sceneNodeSelect === nodeKey);
      }

      for (const panel of editor.querySelectorAll("[data-scene-node-panel]")) {
        panel.classList.toggle("active", panel.dataset.sceneNodePanel === nodeKey);
      }
    }

    function setSceneTreeItemExpanded(item, expanded) {
      item.classList.toggle("expanded", expanded);

      const toggle = getSceneTreeToggle(item);
      if (toggle && !toggle.disabled) {
        toggle.textContent = expanded ? "v" : ">";
      }
    }

    function setupSceneTreeItem(editor, item) {
      if (!item || item.dataset.sceneTreeItemReady) {
        return;
      }

      item.dataset.sceneTreeItemReady = "true";
      const line = findDirectChildByClass(item, "scene-tree-line");
      const toggle = line ? line.querySelector("[data-scene-tree-toggle]") : null;
      if (toggle) {
        toggle.addEventListener("click", (event) => {
          event.preventDefault();
          event.stopPropagation();

          const expanded = !item.classList.contains("expanded");
          setSceneTreeItemExpanded(item, expanded);
        });
      }

      const nodeButton = line ? line.querySelector("[data-scene-node-select]") : null;
      if (nodeButton) {
        nodeButton.addEventListener("click", (event) => {
          event.preventDefault();
          hideSceneContextMenu();
          selectSceneNode(editor, nodeButton.dataset.sceneNodeSelect);
        });
        nodeButton.addEventListener("contextmenu", (event) => {
          event.preventDefault();
          event.stopPropagation();
          selectSceneNode(editor, nodeButton.dataset.sceneNodeSelect);
          showSceneContextMenu(editor, nodeButton.dataset.sceneNodeSelect, event);
        });
      }
    }

    function setupSceneTree(editor) {
      for (const item of editor.querySelectorAll("[data-scene-node-item]")) {
        setupSceneTreeItem(editor, item);
      }
    }

    function setupSceneNodePanel(panel) {
      if (!panel || panel.dataset.sceneNodePanelReady) {
        return;
      }

      panel.dataset.sceneNodePanelReady = "true";
      for (const input of panel.querySelectorAll("[data-scene-node-transform]")) {
        setupEditorInput(input);
      }
      for (const button of panel.querySelectorAll("[data-scene-add-component]")) {
        setupSceneNodeComponentButton(button);
      }
    }

    function sceneNodeTransformInput(nodeKey, property, value, step) {
      const safeValue = escapeHtml(value);
      return `<input type="text" value="${safeValue}" inputmode="decimal" data-editor-drag="number" data-editor-step="${escapeHtml(step)}" data-scene-node-transform="${escapeHtml(property)}" data-scene-node="${escapeHtml(nodeKey)}" data-editor-default="${safeValue}" title="${escapeHtml(property)}">`;
    }

    function sceneNodeTransformRow(nodeKey, label, property, values, step) {
      return `<div class="property-row scene-transform-row"><label>${escapeHtml(label)}</label><div class="vector-input vector-size-3">`
        + sceneNodeTransformInput(nodeKey, `${property}.x`, values[0], step)
        + sceneNodeTransformInput(nodeKey, `${property}.y`, values[1], step)
        + sceneNodeTransformInput(nodeKey, `${property}.z`, values[2], step)
        + `</div></div>`;
    }

    function createSceneTreeItemHtml(nodeKey, nodeType) {
      const shortType = nodeType === "Node2D" ? "2D" : "3D";
      const name = nodeType === "Node2D" ? "Node2D" : "Node3D";
      return `<div class="scene-tree-item" data-scene-node-item="${escapeHtml(nodeKey)}"><div class="scene-tree-line"><button class="scene-tree-toggle" type="button" data-scene-tree-toggle disabled></button><button class="scene-tree-node" type="button" data-scene-node-select="${escapeHtml(nodeKey)}"><span class="scene-node-type">${shortType}</span><span class="scene-node-name">${name}</span></button></div><div class="scene-tree-children"></div></div>`;
    }

    function createSceneNodePanelHtml(nodeKey, nodeType) {
      const name = nodeType === "Node2D" ? "Node2D" : "Node3D";
      const label = nodeType === "Node2D" ? "2D Node" : "3D Node";
      const componentDisabled = nodeType === "Node3D" ? "" : " disabled";
      return `<article class="scene-node-inspector" data-scene-node-panel="${escapeHtml(nodeKey)}"><div class="object-head scene-node-head"><div><div class="object-title">${name}</div><div class="object-type">${label}</div></div></div><section class="inspector-section"><div class="section-title">Transform</div><div class="property-list">`
        + sceneNodeTransformRow(nodeKey, "Translation", "translation", ["0", "0", "0"], "0.01")
        + sceneNodeTransformRow(nodeKey, "Rotation", "rotation", ["0", "0", "0"], "0.1")
        + sceneNodeTransformRow(nodeKey, "Scale", "scale", ["1", "1", "1"], "0.01")
        + `</div></section><section class="inspector-section"><div class="section-head"><div class="section-title">Components</div><button class="scene-add-component" type="button" data-scene-add-component="ModelComponent" title="Add ModelComponent"${componentDisabled}>+</button></div><div class="component-list"><div class="empty">No components.</div></div></section></article>`;
    }

    async function createSceneNode(editor, parentKey, nodeType, sourceButton) {
      if (!editor || !parentKey || !nodeType || !window.createSceneNode) {
        return;
      }

      if (sourceButton) {
        sourceButton.disabled = true;
      }
      try {
        const nodeKey = await window.createSceneNode(editor.dataset.renderModeId, parentKey, nodeType);
        if (!nodeKey) {
          return;
        }

        const parentItem = findSceneTreeItem(editor, parentKey);
        const treeChildren = parentItem ? getSceneTreeChildren(parentItem) : editor.querySelector(".scene-tree-list");
        const inspectorList = editor.querySelector(".scene-node-inspector-list");
        if (!treeChildren || !inspectorList) {
          return;
        }

        treeChildren.insertAdjacentHTML("afterbegin", createSceneTreeItemHtml(nodeKey, nodeType));
        inspectorList.insertAdjacentHTML("beforeend", createSceneNodePanelHtml(nodeKey, nodeType));

        const newItem = findSceneTreeItem(editor, nodeKey);
        const newPanel = editor.querySelector(`[data-scene-node-panel="${nodeKey}"]`);
        setupSceneTreeItem(editor, newItem);
        setupSceneNodePanel(newPanel);

        if (parentItem) {
          const parentToggle = getSceneTreeToggle(parentItem);
          if (parentToggle) {
            parentToggle.disabled = false;
          }
          setSceneTreeItemExpanded(parentItem, true);
        }

        selectSceneNode(editor, nodeKey);
        if (newItem) {
          newItem.scrollIntoView({ block: "nearest" });
        }
      } finally {
        if (sourceButton) {
          sourceButton.disabled = false;
        }
      }
    }

)html";

    html += R"html(
    async function addSceneNodeComponent(button) {
      const editor = button.closest("[data-scene-manager-editor]");
      const panel = button.closest("[data-scene-node-panel]");
      if (!editor || !panel || !button.dataset.sceneAddComponent || !window.addSceneNodeComponent) {
        return;
      }

      button.disabled = true;
      try {
        await window.addSceneNodeComponent(editor.dataset.renderModeId, panel.dataset.sceneNodePanel, button.dataset.sceneAddComponent);
        const list = panel.querySelector(".component-list");
        if (list && button.dataset.sceneAddComponent === "ModelComponent") {
          list.innerHTML = '<article class="component-card" data-scene-component-card="ModelComponent"><div class="component-title">ModelComponent</div><div class="component-info-row"><span>Path</span><span>-</span></div><div class="component-info-row"><span>State</span><span>Empty</span></div><div class="component-info-row"><span>Renderables</span><span>All</span></div></article>';
        }
      } catch (error) {
        button.disabled = false;
      }
    }

    function setupSceneNodeComponentButton(button) {
      if (button.dataset.sceneAddComponentReady) {
        return;
      }

      button.dataset.sceneAddComponentReady = "true";
      button.addEventListener("click", (event) => {
        event.preventDefault();
        addSceneNodeComponent(button);
      });
    }

    for (const editor of document.querySelectorAll("[data-scene-manager-editor]")) {
      setupSceneTree(editor);
      for (const panel of editor.querySelectorAll("[data-scene-node-panel]")) {
        setupSceneNodePanel(panel);
      }
    }

    for (const button of document.querySelectorAll("[data-scene-add-component]")) {
      setupSceneNodeComponentButton(button);
    }

    for (const button of document.querySelectorAll("[data-scene-context-create]")) {
      button.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        const editor = sceneContext.editor;
        const parentKey = sceneContext.nodeKey;
        const nodeType = button.dataset.sceneContextCreate;
        hideSceneContextMenu();
        createSceneNode(editor, parentKey, nodeType, button);
      });
    }

    for (const input of document.querySelectorAll("[data-editor-property], [data-scene-node-transform]")) {
      setupEditorInput(input);
    }

    for (const button of document.querySelectorAll("[data-editor-reset-object]")) {
      button.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        resetObjectInput(button);
      });
    }

    document.addEventListener("pointerdown", (event) => {
      if (sceneContextMenu && !sceneContextMenu.hidden && !sceneContextMenu.contains(event.target)) {
        hideSceneContextMenu();
      }
    });

    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape") {
        hideSceneContextMenu();
      }
    });

    window.addEventListener("blur", hideSceneContextMenu);
    window.addEventListener("resize", hideSceneContextMenu);
  </script>
</body>
</html>
)html";

    return html;
}

} // namespace Play::editor
