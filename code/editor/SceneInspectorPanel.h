#ifndef PLAY_CODE_EDITOR_SCENEINSPECTORPANEL_H
#define PLAY_CODE_EDITOR_SCENEINSPECTORPANEL_H

#include <QGroupBox>
#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace Play::editor
{

struct EditorUiSceneComponent;
struct EditorUiSceneNode;
struct EditorUiTransform;

class TransformEditorWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit TransformEditorWidget(QWidget* parent = nullptr);

    void setTransform(const EditorUiTransform& transform);

signals:
    void transformEdited(const QString& path, double value);

private:
    QDoubleSpinBox* _editors[9] = {};
    bool            _syncing    = false;
};

class SceneComponentEditorWidget : public QGroupBox
{
    Q_OBJECT

public:
    explicit SceneComponentEditorWidget(QWidget* parent = nullptr);

    virtual void setComponent(const EditorUiSceneComponent& component);

signals:
    void loadModelRequested(const QString& path);

protected:
    QVBoxLayout* _bodyLayout  = nullptr;
    QFormLayout* _detailsForm = nullptr;
};

class ModelComponentEditorWidget final : public SceneComponentEditorWidget
{
    Q_OBJECT

public:
    explicit ModelComponentEditorWidget(QWidget* parent = nullptr);

private:
    QPushButton* _loadButton = nullptr;
};

class SceneComponentEditorFactory final
{
public:
    using Creator = SceneComponentEditorWidget* (*)(QWidget* parent);

    static SceneComponentEditorFactory& instance();

    void registerEditor(const QString& typeName, Creator creator);
    SceneComponentEditorWidget* createEditor(const QString& typeName, QWidget* parent) const;

private:
    SceneComponentEditorFactory();

    QHash<QString, Creator> _creators;
};

class SceneInspectorPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit SceneInspectorPanel(QWidget* parent = nullptr);

    void setUnavailable(const QString& renderModeId, const QString& message);
    void setEmpty(const QString& renderModeId, const QString& message);
    void setNode(const QString& renderModeId, const EditorUiSceneNode& node);

signals:
    void sceneNodeTransformRequested(const QString& renderModeId, const QString& nodeKey, const QString& path, double value);
    void sceneNodeComponentRequested(const QString& renderModeId, const QString& nodeKey, const QString& componentType);
    void sceneNodeModelLoadRequested(const QString& renderModeId, const QString& nodeKey, const QString& path);

private:
    void clearComponentEditors();
    void rebuildComponentEditors(const EditorUiSceneNode& node);
    void updateComponentEditors(const EditorUiSceneNode& node);

    QString _renderModeId;
    QString _nodeKey;
    QString _componentLayoutSignature;

    QLabel*                _emptyLabel      = nullptr;
    QWidget*               _detailsWidget   = nullptr;
    QLabel*                _nameLabel       = nullptr;
    QLabel*                _typeLabel       = nullptr;
    TransformEditorWidget* _transformWidget = nullptr;
    QPushButton*           _addModelButton  = nullptr;
    QLabel*                _componentsEmptyLabel = nullptr;
    QWidget*               _componentsHost       = nullptr;
    QVBoxLayout*           _componentsLayout     = nullptr;
    QList<SceneComponentEditorWidget*> _componentEditors;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_SCENEINSPECTORPANEL_H
