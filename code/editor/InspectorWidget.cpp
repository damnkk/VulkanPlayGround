#include "editor/InspectorWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QVBoxLayout>

#include "editor/PropertyWidgetFactory.h"
#include "editor/RuntimeEditor.h"

namespace Play::editor
{

InspectorWidget::InspectorWidget(RuntimeEditor& editor, const EditorSnapshot& snapshot, QWidget* parent)
    : QScrollArea(parent), _editor(editor), _snapshot(snapshot)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rebuild();
}

const EditorNode* InspectorWidget::selectedNode() const
{
    for (const auto& node : _snapshot.nodes)
    {
        if (node.id == _selectedNode) return &node;
    }
    return nullptr;
}

void InspectorWidget::refresh()
{
    const auto* node = selectedNode();
    if (!node || !_title || node->hasTransform != (_transformEditors[0] != nullptr) || node->components.size() != _componentIds.size())
    {
        rebuild();
        return;
    }

    // RTTR property/action definitions are fixed for each component type.
    // Only changes to the attached components require new widgets.
    for (size_t i = 0; i < _componentIds.size(); ++i)
    {
        if (node->components[i].id != _componentIds[i])
        {
            rebuild();
            return;
        }
    }
    updateValues(*node);
}

void InspectorWidget::refreshPendingValues()
{
    if (_pendingValues) updateValues(*selectedNode());
}

void InspectorWidget::updateValues(const EditorNode& node)
{
    _pendingValues = false;
    _title->setText(QString::fromStdString(node.name));
    bool editingTransform = false;
    for (auto* spin : _transformEditors)
    {
        editingTransform = editingTransform || (spin && spin->hasFocus());
    }
    // SetTransform still submits all nine values together, so keep this edit intact.
    if (editingTransform)
    {
        _pendingValues = true;
    }
    else if (node.hasTransform)
    {
        for (int row = 0; row < 3; ++row)
        {
            const float* values = row == 0 ? node.transform.translation : row == 1 ? node.transform.rotation : node.transform.scale;
            for (int axis = 0; axis < 3; ++axis)
            {
                auto*                spin = _transformEditors[row * 3 + axis];
                const QSignalBlocker blocker(spin);
                spin->setValue(values[axis]);
            }
        }
    }
    for (size_t i = 0; i < node.components.size(); ++i)
    {
        for (size_t p = 0; p < _propertyWidgets[i].size(); ++p)
        {
            if (!PropertyWidgetFactory::update(_propertyWidgets[i][p], node.components[i].properties[p], _snapshot))
            {
                _pendingValues = true;
            }
        }
    }
}

void InspectorWidget::selectNode(EditorNodeId nodeId)
{
    if (_selectedNode == nodeId) return;
    _selectedNode = nodeId;
    rebuild();
}

void InspectorWidget::rebuild()
{
    const int scrollPosition = verticalScrollBar()->value();
    _title                   = nullptr;
    for (auto& spin : _transformEditors) spin = nullptr;
    _componentIds.clear();
    _pendingValues = false;
    _propertyWidgets.clear();
    QWidget*     content    = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(content);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(8);

    const EditorNode* selectedNode = this->selectedNode();

    if (!selectedNode)
    {
        const char* message = _snapshot.revision == 0   ? "Waiting for an engine snapshot."
                              : _snapshot.nodes.empty() ? "No scene is loaded."
                                                        : "Select a node to inspect.";
        rootLayout->addWidget(new QLabel(message, content));
        rootLayout->addStretch();
        setWidget(content);
        return;
    }

    QLabel* title = new QLabel(QString::fromStdString(selectedNode->name), content);
    _title        = title;
    title->setWordWrap(true);
    QFont font = title->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    title->setFont(font);
    rootLayout->addWidget(title);

    if (selectedNode->hasTransform)
    {
        QGroupBox*   transformGroup  = new QGroupBox("Transform", content);
        QGridLayout* transformLayout = new QGridLayout(transformGroup);
        transformLayout->setContentsMargins(8, 8, 8, 8);
        transformLayout->setHorizontalSpacing(5);
        transformLayout->setVerticalSpacing(5);
        const char*              rows[] = {"Position", "Rotation", "Scale"};
        const char*              axes[] = {"X", "Y", "Z"};
        QVector<QDoubleSpinBox*> editors;

        for (int axis = 0; axis < 3; ++axis)
        {
            auto*       label    = new QLabel(axes[axis], transformGroup);
            const char* colors[] = {"#dd7474", "#83bd80", "#7cabe0"};
            label->setStyleSheet(QString("color: %1; font-weight: 600;").arg(colors[axis]));
            label->setAlignment(Qt::AlignCenter);
            transformLayout->addWidget(label, 0, axis + 1);
            transformLayout->setColumnStretch(axis + 1, 1);
        }
        for (int row = 0; row < 3; ++row)
        {
            transformLayout->addWidget(new QLabel(rows[row], transformGroup), row + 1, 0);
            for (int axis = 0; axis < 3; ++axis)
            {
                QDoubleSpinBox* spinBox = new QDoubleSpinBox(transformGroup);
                spinBox->setDecimals(3);
                spinBox->setKeyboardTracking(false);
                spinBox->setMinimumWidth(0);
                spinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
                spinBox->setFixedHeight(26);
                spinBox->setToolTip(QString("%1 %2%3").arg(rows[row]).arg(axes[axis]).arg(row == 1 ? " (degrees)" : ""));
                spinBox->setRange(-1000000.0, 1000000.0);
                spinBox->setSingleStep(row == 2 ? 0.01 : row == 1 ? 1.0 : 0.1);
                if (row == 1) spinBox->setSuffix(QString::fromUtf8("\xC2\xB0"));
                const float* values = row == 0   ? selectedNode->transform.translation
                                      : row == 1 ? selectedNode->transform.rotation
                                                 : selectedNode->transform.scale;
                spinBox->setValue(values[axis]);
                transformLayout->addWidget(spinBox, row + 1, axis + 1);
                editors.push_back(spinBox);
                _transformEditors[row * 3 + axis] = spinBox;
            }
        }

        for (QDoubleSpinBox* spinBox : editors)
        {
            connect(spinBox, &QDoubleSpinBox::valueChanged, transformGroup,
                    [this, editors, nodeId = selectedNode->id](double)
                    {
                        EditorCommand command;
                        command.type   = EditorCommandType::SetTransform;
                        command.nodeId = nodeId;
                        for (int axis = 0; axis < 3; ++axis)
                        {
                            command.transform.translation[axis] = static_cast<float>(editors[axis]->value());
                            command.transform.rotation[axis]    = static_cast<float>(editors[axis + 3]->value());
                            command.transform.scale[axis]       = static_cast<float>(editors[axis + 6]->value());
                        }
                        _editor.submit(std::move(command));
                    });
        }
        rootLayout->addWidget(transformGroup);
    }

    _propertyWidgets.resize(selectedNode->components.size());
    for (size_t componentIndex = 0; componentIndex < selectedNode->components.size(); ++componentIndex)
    {
        const auto& component = selectedNode->components[componentIndex];
        _componentIds.push_back(component.id);
        QGroupBox*   componentGroup  = new QGroupBox(QString::fromStdString(component.label), content);
        QVBoxLayout* componentLayout = new QVBoxLayout(componentGroup);
        componentLayout->setContentsMargins(8, 8, 8, 8);
        componentLayout->setSpacing(5);
        QFormLayout* properties = new QFormLayout();
        properties->setContentsMargins(0, 0, 0, 0);
        properties->setHorizontalSpacing(8);
        properties->setVerticalSpacing(5);
        properties->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        for (const EditorProperty& property : component.properties)
        {
            QWidget* propertyWidget = PropertyWidgetFactory::create(property, _snapshot, selectedNode->id, component.id, _editor, componentGroup);
            properties->addRow(QString::fromStdString(property.label), propertyWidget);
            _propertyWidgets[componentIndex].push_back(propertyWidget);
        }
        componentLayout->addLayout(properties);

        for (const auto& action : component.actions)
        {
            auto* button = new QPushButton(QString::fromStdString(action.label), componentGroup);
            button->setFixedHeight(26);
            connect(button, &QPushButton::clicked, componentGroup,
                    [this, nodeId = selectedNode->id, componentId = component.id, name = action.name]()
                    {
                        EditorCommand command;
                        command.type        = EditorCommandType::InvokeAction;
                        command.nodeId      = nodeId;
                        command.componentId = componentId;
                        command.property    = name;
                        _editor.submit(std::move(command));
                    });
            componentLayout->addWidget(button);
        }

        if (component.removable)
        {
            QPushButton* remove = new QPushButton("Remove", componentGroup);
            remove->setFixedHeight(24);
            connect(remove, &QPushButton::clicked, componentGroup,
                    [this, componentId = component.id, nodeId = selectedNode->id]()
                    {
                        EditorCommand command;
                        command.type        = EditorCommandType::RemoveComponent;
                        command.nodeId      = nodeId;
                        command.componentId = componentId;
                        _editor.submit(std::move(command));
                    });
            componentLayout->addWidget(remove, 0, Qt::AlignRight);
        }
        rootLayout->addWidget(componentGroup);
    }

    if (!_snapshot.componentTypes.empty())
    {
        QWidget*     addRow = new QWidget(content);
        QHBoxLayout* layout = new QHBoxLayout(addRow);
        layout->setContentsMargins(0, 0, 0, 0);
        QComboBox* types = new QComboBox(addRow);
        types->setMinimumWidth(0);
        types->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        types->setToolTip("Component type");
        types->setFixedHeight(28);
        for (const EditorComponentType& type : _snapshot.componentTypes)
        {
            bool attached = false;
            for (const auto& component : selectedNode->components)
            {
                attached = attached || component.typeName == type.typeName;
            }
            if (attached) continue;
            types->addItem(QString::fromStdString(type.label), QString::fromStdString(type.typeName));
        }
        QPushButton* add = new QPushButton("+ Add", addRow);
        add->setFixedHeight(28);
        add->setEnabled(types->count() != 0);
        if (types->count() == 0) types->addItem("All components added");
        types->setEnabled(add->isEnabled());
        layout->setSpacing(6);
        layout->addWidget(types, 1);
        layout->addWidget(add);
        connect(add, &QPushButton::clicked, addRow,
                [this, types, nodeId = selectedNode->id]()
                {
                    EditorCommand command;
                    command.type          = EditorCommandType::AddComponent;
                    command.nodeId        = nodeId;
                    command.componentType = types->currentData().toString().toStdString();
                    _editor.submit(std::move(command));
                });
        rootLayout->addWidget(addRow);
    }

    rootLayout->addStretch();
    setWidget(content);
    verticalScrollBar()->setValue(scrollPosition);
}

} // namespace Play::editor
