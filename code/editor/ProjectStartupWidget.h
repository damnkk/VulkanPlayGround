#ifndef PLAY_CODE_EDITOR_PROJECTSTARTUPWIDGET_H
#define PLAY_CODE_EDITOR_PROJECTSTARTUPWIDGET_H

#include <QWidget>

class QLabel;

namespace Play::editor
{

class ProjectStartupWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectStartupWidget(QWidget* parent = nullptr);

    void showError(const QString& message);
    void showStarting();

signals:
    void projectSelected(const QString& projectPath);

private:
    QLabel*  _status  = nullptr;
    QWidget* _actions = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_PROJECTSTARTUPWIDGET_H
