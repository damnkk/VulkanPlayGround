#include "editor/SceneInspectorPanel.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "editor/EditorUiModel.h"

namespace Play::editor
{

namespace
{
QString toQString(const std::string& text)
{
    return QString::fromStdString(text);
}

bool widgetContainsFocus(const QWidget* widget)
{
    const QWidget* focusWidget = QApplication::focusWidget();
    return widget && focusWidget && (widget == focusWidget || widget->isAncestorOf(focusWidget));
}

QLabel* makeMutedLabel(const QString& text)
{
    QLabel* label = new QLabel(text);
    label->setWordWrap(true);
    label->setProperty("muted", true);
    return label;
}

QFrame* makeSeparator()
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

int transformEditorIndex(int groupIndex, int componentIndex)
{
    return groupIndex * 3 + componentIndex;
}

QString makeComponentLayoutSignature(const EditorUiSceneNode& node)
{
    QString signature = toQString(node.key);
    signature += "\n";
    for (const EditorUiSceneComponent& component : node.components)
    {
        signature += toQString(component.typeName);
        signature += "\n";
    }
    return signature;
}

SceneComponentEditorWidget* createComponentEditor(const EditorUiSceneComponent& component, QWidget* parent)
{
    return SceneComponentEditorFactory::instance().createEditor(toQString(component.typeName), parent);
}
} // namespace

TransformEditorWidget::TransformEditorWidget(QWidget* parent) : QWidget(parent)
{
    QFormLayout* form = new QFormLayout(this);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    static const char* transformLabels[] = {"Translation", "Rotation", "Scale"};
    static const char* transformNames[]  = {"translation", "rotation", "scale"};
    static const char* components[]      = {"x", "y", "z"};
    static const double transformSteps[] = {0.01, 0.1, 0.01};

    for (int groupIndex = 0; groupIndex < 3; ++groupIndex)
    {
        QWidget*     row       = new QWidget(this);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        for (int componentIndex = 0; componentIndex < 3; ++componentIndex)
        {
            QDoubleSpinBox* spin = new QDoubleSpinBox(row);
            spin->setDecimals(3);
            spin->setRange(-1000000000.0, 1000000000.0);
            spin->setSingleStep(transformSteps[groupIndex]);
            spin->setKeyboardTracking(false);
            rowLayout->addWidget(spin);

            _editors[transformEditorIndex(groupIndex, componentIndex)] = spin;
            const QString path = QString(transformNames[groupIndex]) + "." + components[componentIndex];
            QObject::connect(spin, &QDoubleSpinBox::valueChanged, this,
                             [this, path](double value)
                             {
                                 if (!_syncing)
                                 {
                                     emit transformEdited(path, value);
                                 }
                             });
        }

        form->addRow(transformLabels[groupIndex], row);
    }
}

void TransformEditorWidget::setTransform(const EditorUiTransform& transform)
{
    const float* transformValues[] = {transform.translation, transform.rotation, transform.scale};
    _syncing = true;
    for (int groupIndex = 0; groupIndex < 3; ++groupIndex)
    {
        for (int componentIndex = 0; componentIndex < 3; ++componentIndex)
        {
            QDoubleSpinBox* spin = _editors[transformEditorIndex(groupIndex, componentIndex)];
            if (!spin || widgetContainsFocus(spin))
            {
                continue;
            }

            const QSignalBlocker blocker(spin);
            spin->setValue(transformValues[groupIndex][componentIndex]);
        }
    }
    _syncing = false;
}

SceneComponentEditorWidget::SceneComponentEditorWidget(QWidget* parent) : QGroupBox(parent)
{
    _bodyLayout = new QVBoxLayout(this);
    _detailsForm = new QFormLayout();
    _detailsForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    _detailsForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    _bodyLayout->addLayout(_detailsForm);
}

void SceneComponentEditorWidget::setComponent(const EditorUiSceneComponent& component)
{
    setTitle(toQString(component.typeName));
    while (_detailsForm->rowCount() > 0)
    {
        _detailsForm->removeRow(0);
    }

    if (component.details.empty())
    {
        _detailsForm->addRow(makeMutedLabel("No details."));
        return;
    }

    for (const EditorUiKeyValue& detail : component.details)
    {
        QLabel* value = new QLabel(toQString(detail.value));
        value->setWordWrap(true);
        _detailsForm->addRow(toQString(detail.label), value);
    }
}

ModelComponentEditorWidget::ModelComponentEditorWidget(QWidget* parent) : SceneComponentEditorWidget(parent)
{
    _loadButton = new QPushButton("Load Model", this);
    _bodyLayout->insertWidget(0, _loadButton);

    QObject::connect(_loadButton, &QPushButton::clicked, this,
                     [this]()
                     {
                         const QString path = QFileDialog::getOpenFileName(
                             this, "Load Model", QString(),
                             "Model Files (*.obj *.gltf *.glb *.fbx *.ply *.spz);;All Files (*.*)");
                         if (!path.isEmpty())
                         {
                             emit loadModelRequested(path);
                         }
                     });
}

SceneComponentEditorFactory::SceneComponentEditorFactory()
{
    registerEditor("ModelComponent",
                   [](QWidget* parent) -> SceneComponentEditorWidget*
                   {
                       return new ModelComponentEditorWidget(parent);
                   });
}

SceneComponentEditorFactory& SceneComponentEditorFactory::instance()
{
    static SceneComponentEditorFactory factory;
    return factory;
}

void SceneComponentEditorFactory::registerEditor(const QString& typeName, Creator creator)
{
    if (!typeName.isEmpty() && creator)
    {
        _creators.insert(typeName, creator);
    }
}

SceneComponentEditorWidget* SceneComponentEditorFactory::createEditor(const QString& typeName, QWidget* parent) const
{
    Creator creator = _creators.value(typeName, nullptr);
    return creator ? creator(parent) : new SceneComponentEditorWidget(parent);
}

SceneInspectorPanel::SceneInspectorPanel(QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* title = new QLabel("Node Inspector", this);
    title->setProperty("sectionTitle", true);
    rootLayout->addWidget(title);

    _emptyLabel = makeMutedLabel("");
    _emptyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    rootLayout->addWidget(_emptyLabel);

    _detailsWidget = new QWidget(this);
    QVBoxLayout* detailsLayout = new QVBoxLayout(_detailsWidget);
    detailsLayout->setContentsMargins(0, 0, 0, 0);

    _nameLabel = new QLabel(_detailsWidget);
    _nameLabel->setProperty("objectTitle", true);
    detailsLayout->addWidget(_nameLabel);

    _typeLabel = makeMutedLabel("");
    detailsLayout->addWidget(_typeLabel);
    detailsLayout->addWidget(makeSeparator());

    QGroupBox* transformGroup = new QGroupBox("Transform", _detailsWidget);
    QVBoxLayout* transformLayout = new QVBoxLayout(transformGroup);
    _transformWidget = new TransformEditorWidget(transformGroup);
    transformLayout->addWidget(_transformWidget);
    detailsLayout->addWidget(transformGroup);

    QGroupBox* componentsGroup = new QGroupBox("Components", _detailsWidget);
    QVBoxLayout* componentsBody = new QVBoxLayout(componentsGroup);
    _addModelButton = new QPushButton("Add ModelComponent", componentsGroup);
    componentsBody->addWidget(_addModelButton);
    _componentsEmptyLabel = makeMutedLabel("No components.");
    componentsBody->addWidget(_componentsEmptyLabel);
    _componentsHost = new QWidget(componentsGroup);
    _componentsLayout = new QVBoxLayout(_componentsHost);
    _componentsLayout->setContentsMargins(0, 0, 0, 0);
    componentsBody->addWidget(_componentsHost);
    detailsLayout->addWidget(componentsGroup);
    detailsLayout->addStretch();

    rootLayout->addWidget(_detailsWidget);
    rootLayout->addStretch();

    QObject::connect(_transformWidget, &TransformEditorWidget::transformEdited, this,
                     [this](const QString& path, double value)
                     {
                         if (!_nodeKey.isEmpty())
                         {
                             emit sceneNodeTransformRequested(_renderModeId, _nodeKey, path, value);
                         }
                     });

    QObject::connect(_addModelButton, &QPushButton::clicked, this,
                     [this]()
                     {
                         if (!_nodeKey.isEmpty())
                         {
                             emit sceneNodeComponentRequested(_renderModeId, _nodeKey, "ModelComponent");
                         }
                     });

    setEmpty(QString(), "No scene node selected.");
}

void SceneInspectorPanel::setUnavailable(const QString& renderModeId, const QString& message)
{
    _renderModeId = renderModeId;
    _nodeKey.clear();
    _componentLayoutSignature.clear();
    clearComponentEditors();
    _emptyLabel->setText(message);
    _emptyLabel->show();
    _detailsWidget->hide();
}

void SceneInspectorPanel::setEmpty(const QString& renderModeId, const QString& message)
{
    _renderModeId = renderModeId;
    _nodeKey.clear();
    _componentLayoutSignature.clear();
    clearComponentEditors();
    _emptyLabel->setText(message);
    _emptyLabel->show();
    _detailsWidget->hide();
}

void SceneInspectorPanel::setNode(const QString& renderModeId, const EditorUiSceneNode& node)
{
    _renderModeId = renderModeId;
    _nodeKey      = toQString(node.key);
    _emptyLabel->hide();
    _detailsWidget->show();

    _nameLabel->setText(toQString(node.name));
    _typeLabel->setText(toQString(node.typeName + "  " + node.key));
    _transformWidget->setTransform(node.transform);
    _addModelButton->setEnabled(node.canAddModelComponent);

    const QString componentLayoutSignature = makeComponentLayoutSignature(node);
    if (componentLayoutSignature != _componentLayoutSignature)
    {
        _componentLayoutSignature = componentLayoutSignature;
        rebuildComponentEditors(node);
    }
    updateComponentEditors(node);
}

void SceneInspectorPanel::clearComponentEditors()
{
    for (SceneComponentEditorWidget* editor : _componentEditors)
    {
        _componentsLayout->removeWidget(editor);
        delete editor;
    }
    _componentEditors.clear();
    if (_componentsEmptyLabel)
    {
        _componentsEmptyLabel->show();
    }
}

void SceneInspectorPanel::rebuildComponentEditors(const EditorUiSceneNode& node)
{
    clearComponentEditors();
    _componentsEmptyLabel->setVisible(node.components.empty());

    for (const EditorUiSceneComponent& component : node.components)
    {
        SceneComponentEditorWidget* editor = createComponentEditor(component, _componentsHost);
        QObject::connect(editor, &SceneComponentEditorWidget::loadModelRequested, this,
                         [this](const QString& path)
                         {
                             if (!_nodeKey.isEmpty())
                             {
                                 emit sceneNodeModelLoadRequested(_renderModeId, _nodeKey, path);
                             }
                         });
        _componentsLayout->addWidget(editor);
        _componentEditors.push_back(editor);
    }
}

void SceneInspectorPanel::updateComponentEditors(const EditorUiSceneNode& node)
{
    _componentsEmptyLabel->setVisible(node.components.empty());
    for (int componentIndex = 0; componentIndex < _componentEditors.size() && componentIndex < static_cast<int>(node.components.size());
         ++componentIndex)
    {
        _componentEditors[componentIndex]->setComponent(node.components[componentIndex]);
    }
}

} // namespace Play::editor
