# Qt 层 review（2026-09-05）

评审标准：优先当前功能可用、代码清楚、职责简单。只讨论实际交互问题和已经存在的多余设计，不做商业项目式的稳定性加固。本次为静态检查，未编译或运行。

## 本次已处理

- 引擎没有消费 editor 命令，也没有发布场景快照。新增 `core/runtime/SceneEditorBridge.*`，接通根节点、子节点创建，子树删除，以及已有的节点重命名入口。
- 引擎 Entity ID 从 0 开始，与 UI 的无节点标记冲突。桥接层统一使用 `entityId + 1`。
- `Scene::removeEntity()` 在 vector erase 后使用原元素引用；现在先持有实体，再递归删除后代、解除父子关系、移出 Scene。
- SceneTree 重建期间屏蔽树信号，完成后统一恢复并通知最终选择；删除选中子节点时选中父节点，否则选中剩余根节点，场景为空时才清空选择，保证按钮和 Inspector 与树一致。
- 无选择时禁用 Add Child 和 Remove；添加子节点展开父节点；空场景与尚未收到快照使用不同提示。
- 创建节点不再弹出命名对话框，Scene 自动生成 `Entity 1`、`Entity 2` 等默认名，仍可在树中重命名。
- Scene 析构时清空 IDPool，修复创建节点后退出触发的 `not all IDs were destroyed` 断言。

## 建议整改，尚未改动

### 1. 接属性编辑之前，先改变 Inspector 的刷新方式

位置：`InspectorWidget.cpp` 的 `setSnapshot()`、`rebuild()` 和 Transform 的 `valueChanged` 连接；`QtRuntimeEditorWindow.cpp` 的 `refresh()`。

当前任何新 revision 都会重建整个 Inspector，最终 `setWidget(content)` 替换旧控件。按照现有协议，数值改动提交命令后，引擎会发布快照；因此接上 Transform 或属性编辑后，正常打字、连续调值就会触发控件替换，丢失焦点和编辑状态。改其他节点也会重建当前 Inspector。

建议：选择节点或组件结构改变时才重建控件；普通值变化就更新现有控件，更新时用 `QSignalBlocker` 防止回发。数值输入先按 `editingFinished` 提交即可，不必现在引入通用 diff 框架或复杂 Qt model。

### 2. 数值属性控件需要区分整数和浮点数

位置：`PropertyWidgetFactory.cpp` 的 `property.type.is_arithmetic()` 分支。

当前所有数字统一用四位小数的 `QDoubleSpinBox`，默认范围为 ±1e9。整数属性也允许输入小数，unsigned 属性也允许负数；提交时再转回原类型，UI 的输入语义与真实属性类型不一致。范围也是控件自行决定，而不是属性类型或元数据决定。

建议：先支持实际组件用到的整数和浮点类型；普通整数用整数控件和整数步长，浮点数用浮点控件，范围优先读取已有元数据。不必为尚未出现的所有数字类型做通用转换系统。

### 3. 删除未使用的项目启动通道，明确单次启动流程

位置：`RuntimeEditor.h` 的 `StartupProjectReceiver`，`RuntimeGuiHost.*` 的 `selectStartupProject()` / `takeStartupProject()`，`EngineLoop.cpp`。

当前 Qt 窗口没有项目选择入口，`takeStartupProject()` 也没有调用方；实际项目路径由 EngineLoop 直接决定。RuntimeEditor 因而同时保留了正在使用的命令/快照通道和未使用的 receiver 指针通道，增加了理解启动流程的成本。

建议：先删除这条未使用通道，保留目前直接启动的路径。真正做项目选择时，再按具体 UI 流程接入。Qt 窗口关闭已经表示退出程序，`RuntimeGuiHost` 的重新启动分支也可以在梳理生命周期时一并缩减；不需要为它补一套重启恢复逻辑。

### 4. 接保存功能前，修正 Scene 序列化修改 ID 的问题

位置：`resourceManagement/scene/cpu/Scene.h` 的 `serialize()`，这是 Qt 接入涉及的引擎侧问题。

`_idPool.createID(entity->_id)` 在读档和存档时都会执行。也就是说保存场景会改变活实体的 ID，使现有 UI 快照和待处理命令引用旧 ID。这是正常保存操作的问题。

建议：仅在读档时重建运行时 ID 和 Scene 归属；保存不得改变实体身份。当前尚未接项目保存 UI，本次没有扩展到存档流程。

## 保留的设计

`EditorProtocol` 的值快照、命令队列和独立 Widget 分工适合目前规模，可以保留。200 ms 轮询和 Scene 的线性查找目前也足够，不需要为了潜在性能问题改成事件总线、复杂缓存或多层抽象。节点层级约定为 Scene 持有全部实体，Entity 的父子引用表达树结构。

## 本次接入的手动验收

1. 按项目既有方式编译并启动；Scene 状态应出现，Inspector 提示空场景。
2. 连续点击 Add Root，确认直接出现 `Entity 1`、`Entity 2`，没有命名弹窗；在树中将它们重命名为 A 和 B。选中 A，Add Child 创建节点并重命名为 C，再选 C 创建 D，确认层级。
3. 删除 C，确认 C、D 同时消失，A、B 保留，自动选中 A，Inspector 显示 A，Add Child 和 Remove 仍可使用。也验证只有一个根节点和一个子节点时删除子节点的情况。
4. 重新给 A 添加子节点，再删除 A，确认 B 保留；删除 B 后仍能重新 Add Root。
5. 对新节点重命名，等待一次 UI 刷新后确认名字保留；可创建同名节点并分别删除，验证操作按 ID 定位。
6. 分别验证保留节点直接退出、删除全部节点后退出，两种情况均不应触发 IDPool 断言。

这是节点和 CPU Scene 的接入；Transform、组件、资源绑定、项目读写及渲染对象显示不属于本次完成范围。
