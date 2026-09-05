#include "editor/InspectorWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "editor/PropertyWidgetFactory.h"
#include "editor/RuntimeEditor.h"

namespace Play::editor
{

InspectorWidget::InspectorWidget(RuntimeEditor& editor, QWidget* parent) : QScrollArea(parent), _editor(editor)
{
    setWidgetResizable(true);
    rebuild();
}

void InspectorWidget::setSnapshot(const EditorSnapshot& snapshot)
{
    _snapshot = snapshot;
    rebuild();
}

void InspectorWidget::selectNode(EditorNodeId nodeId)
{
    _selectedNode = nodeId;
    rebuild();
}

void InspectorWidget::rebuild()
{
    QWidget*     content    = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(content);

    const EditorNode* selectedNode = nullptr;
    for (const EditorNode& node : _snapshot.nodes)
    {
        if (node.id == _selectedNode)
        {
            selectedNode = &node;
            break;
        }
    }

    if (!selectedNode)
    {
        const char* message = _snapshot.revision == 0   ? "Waiting for an engine snapshot."
                              : _snapshot.nodes.empty() ? "Scene is empty. Use Add Root to create a node."
                                                        : "Select a node to inspect.";
        rootLayout->addWidget(new QLabel(message, content));
        rootLayout->addStretch();
        setWidget(content);
        return;
    }

    QLabel* title = new QLabel(QString::fromStdString(selectedNode->name), content);
    QFont   font  = title->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    title->setFont(font);
    rootLayout->addWidget(title);

    if (selectedNode->hasTransform)
    {
        QGroupBox*               transformGroup  = new QGroupBox("Transform", content);
        QGridLayout*             transformLayout = new QGridLayout(transformGroup);
        const char*              rows[]          = {"Translation", "Rotation", "Scale"};
        const char*              axes[]          = {"X", "Y", "Z"};
        QVector<QDoubleSpinBox*> editors;

        for (int axis = 0; axis < 3; ++axis)
        {
            transformLayout->addWidget(new QLabel(axes[axis], transformGroup), 0, axis + 1);
        }
        for (int row = 0; row < 3; ++row)
        {
            transformLayout->addWidget(new QLabel(rows[row], transformGroup), row + 1, 0);
            for (int axis = 0; axis < 3; ++axis)
            {
                QDoubleSpinBox* spinBox = new QDoubleSpinBox(transformGroup);
                spinBox->setDecimals(4);
                spinBox->setRange(-1000000.0, 1000000.0);
                spinBox->setSingleStep(row == 2 ? 0.01 : 0.1);
                const float* values = row == 0   ? selectedNode->transform.translation
                                      : row == 1 ? selectedNode->transform.rotation
                                                 : selectedNode->transform.scale;
                spinBox->setValue(values[axis]);
                transformLayout->addWidget(spinBox, row + 1, axis + 1);
                editors.push_back(spinBox);
            }
        }

        for (QDoubleSpinBox* spinBox : editors)
        {
            connect(spinBox, &QDoubleSpinBox::valueChanged, transformGroup,
                    [this, editors](double)
                    {
                        EditorCommand command;
                        command.type   = EditorCommandType::SetTransform;
                        command.nodeId = _selectedNode;
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

    for (const EditorComponent& component : selectedNode->components)
    {
        QGroupBox*   componentGroup  = new QGroupBox(QString::fromStdString(component.label.empty() ? component.typeName : component.label), content);
        QVBoxLayout* componentLayout = new QVBoxLayout(componentGroup);
        QFormLayout* properties      = new QFormLayout();
        for (const EditorProperty& property : component.properties)
        {
            QWidget* propertyWidget = PropertyWidgetFactory::create(property, _snapshot, selectedNode->id, component.id, _editor, componentGroup);
            properties->addRow(QString::fromStdString(property.label.empty() ? property.name : property.label), propertyWidget);
        }
        componentLayout->addLayout(properties);

        if (component.removable)
        {
            QPushButton* remove = new QPushButton("Remove Component", componentGroup);
            connect(remove, &QPushButton::clicked, componentGroup,
                    [this, componentId = component.id]()
                    {
                        EditorCommand command;
                        command.type        = EditorCommandType::RemoveComponent;
                        command.nodeId      = _selectedNode;
                        command.componentId = componentId;
                        _editor.submit(std::move(command));
                    });
            componentLayout->addWidget(remove);
        }
        rootLayout->addWidget(componentGroup);
    }

    if (!_snapshot.componentTypes.empty())
    {
        QWidget*     addRow = new QWidget(content);
        QHBoxLayout* layout = new QHBoxLayout(addRow);
        layout->setContentsMargins(0, 0, 0, 0);
        QComboBox* types = new QComboBox(addRow);
        for (const EditorComponentType& type : _snapshot.componentTypes)
        {
            types->addItem(QString::fromStdString(type.label), QString::fromStdString(type.typeName));
        }
        QPushButton* add = new QPushButton("Add Component", addRow);
        layout->addWidget(types);
        layout->addWidget(add);
        connect(add, &QPushButton::clicked, addRow,
                [this, types]()
                {
                    EditorCommand command;
                    command.type          = EditorCommandType::AddComponent;
                    command.nodeId        = _selectedNode;
                    command.componentType = types->currentData().toString().toStdString();
                    _editor.submit(std::move(command));
                });
        rootLayout->addWidget(addRow);
    }

    rootLayout->addStretch();
    setWidget(content);
}

} // namespace Play::editor
