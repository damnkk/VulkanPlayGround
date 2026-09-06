#include "editor/ProjectStartupWidget.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMatrix4x4>
#include <QPainter>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

namespace Play::editor
{
namespace
{

// A lightweight, animated parametric surface. Only the launcher's visible
// artwork repaints; it needs no Vulkan context or external assets.
class StartupArtwork final : public QWidget
{
public:
    explicit StartupArtwork(QWidget* parent) : QWidget(parent)
    {
        setMinimumSize(330, 260);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        _timer = new QTimer(this);
        _timer->setInterval(40);
        connect(_timer, &QTimer::timeout, this,
                [this]()
                {
                    _angle += 0.18f;
                    update();
                });
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        _timer->start();
    }

    void hideEvent(QHideEvent* event) override
    {
        _timer->stop();
        QWidget::hideEvent(event);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPointF center(width() * 0.49, height() * 0.49);
        const qreal   scale = qMin(width() * 0.32, height() * 0.43);

        QRadialGradient glow(center, scale * 1.6);
        glow.setColorAt(0.0, QColor(92, 148, 76, 40));
        glow.setColorAt(0.5, QColor(41, 104, 95, 22));
        glow.setColorAt(1.0, QColor(10, 14, 20, 0));
        painter.fillRect(rect(), glow);

        painter.setPen(QColor(125, 151, 154, 24));
        for (int x = 16; x < width(); x += 24)
        {
            for (int y = 16; y < height(); y += 24)
            {
                painter.drawPoint(x, y);
            }
        }

        painter.setPen(QPen(QColor(133, 164, 160, 35), 1));
        painter.drawEllipse(center, scale * 1.25, scale * 1.25);
        painter.drawLine(QPointF(center.x() - scale * 1.5, center.y()), QPointF(center.x() + scale * 1.5, center.y()));
        painter.drawLine(QPointF(center.x(), center.y() - scale * 1.4), QPointF(center.x(), center.y() + scale * 1.4));

        QMatrix4x4 rotation;
        rotation.rotate(-24.0f, 1.0f, 0.0f, 0.0f);
        rotation.rotate(_angle, 0.0f, 1.0f, 0.0f);
        rotation.rotate(24.0f, 0.0f, 0.0f, 1.0f);

        const auto surface = [&rotation](qreal u, qreal v)
        {
            const qreal radius = 0.77 + 0.15 * qCos(3.0 * u + 2.0 * v);
            return rotation.map(QVector3D(static_cast<float>((radius + 0.3 * qCos(v)) * qCos(u)), static_cast<float>(0.3 * qSin(v)),
                                          static_cast<float>((radius + 0.3 * qCos(v)) * qSin(u))));
        };
        const auto project = [center, scale](const QVector3D& point)
        {
            const qreal perspective = 3.7 / (3.7 - point.z());
            return center + QPointF(point.x() * scale * perspective, point.y() * scale * perspective);
        };

        // Draw back-facing segments first, then the brighter foreground mesh.
        for (int layer = 0; layer < 2; ++layer)
        {
            for (int ring = 0; ring < 36; ++ring)
            {
                const qreal u = ring * 2.0 * M_PI / 36.0;
                for (int segment = 0; segment < 64; ++segment)
                {
                    const qreal     v     = segment * 2.0 * M_PI / 64.0;
                    const QVector3D a     = surface(u, v);
                    const QVector3D b     = surface(u, v + 2.0 * M_PI / 64.0);
                    const bool      front = (a.z() + b.z()) > 0.0f;
                    if (front != (layer == 1))
                    {
                        continue;
                    }
                    painter.setPen(QPen(front ? QColor(191, 235, 137, 155) : QColor(74, 135, 125, 48), 0.85));
                    painter.drawLine(project(a), project(b));
                    if (segment % 4 == 0)
                    {
                        painter.setPen(QPen(front ? QColor(154, 203, 154, 95) : QColor(74, 135, 125, 30), 0.7));
                        painter.drawLine(project(a), project(surface(u + 2.0 * M_PI / 36.0, v)));
                    }
                }
            }
        }

        painter.setPen(QPen(QColor("#c7f58a"), 2));
        painter.drawLine(QPointF(12, 26), QPointF(12, 12));
        painter.drawLine(QPointF(12, 12), QPointF(26, 12));
        painter.setPen(QPen(QColor("#50605e"), 1));
        painter.drawLine(QPointF(width() - 26, height() - 12), QPointF(width() - 12, height() - 12));
        painter.drawLine(QPointF(width() - 12, height() - 12), QPointF(width() - 12, height() - 26));
    }

private:
    QTimer* _timer = nullptr;
    float   _angle = 18.0f;
};

QLabel* addLabel(QVBoxLayout* layout, const QString& text, const char* name)
{
    auto* label = new QLabel(text);
    label->setObjectName(name);
    label->setWordWrap(true);
    layout->addWidget(label);
    return label;
}

} // namespace

ProjectStartupWidget::ProjectStartupWidget(QWidget* parent) : QWidget(parent)
{
    setObjectName("ProjectStartup");
    setAttribute(Qt::WA_StyledBackground);
    setMinimumSize(900, 740);
    setStyleSheet(R"(
        QWidget#ProjectStartup { background: #0b1016; }
        QLabel { color: #e9eeea; background: transparent; border: none; font-family: 'Segoe UI'; }
        QLabel#BrandMark { background: #c7f58a; color: #172019; border-radius: 10px; font-size: 17px; font-weight: 800; }
        QLabel#Brand { font-size: 15px; font-weight: 600; }
        QLabel#Edition, QLabel#Eyebrow, QLabel#CardIndex, QLabel#Footer, QLabel#ArtCaption {
            color: #82928f; font-size: 10px; font-weight: 600; letter-spacing: 2px;
        }
        QLabel#Eyebrow { color: #c7f58a; }
        QLabel#Headline { font-size: 46px; font-weight: 600; letter-spacing: -2px; }
        QLabel#Intro { color: #93a29e; font-size: 14px; }
        QLabel#WorkspaceTitle { font-size: 23px; font-weight: 600; letter-spacing: -0.5px; }
        QFrame#CreateCard {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #202d26, stop:1 #131c1b);
            border: 1px solid #3b5040; border-radius: 16px;
        }
        QFrame#OpenCard { background: #111a21; border: 1px solid #28353c; border-radius: 16px; }
        QFrame#CreateCard:hover { border-color: #749557; }
        QFrame#OpenCard:hover { border-color: #50666b; }
        QLabel#CardTitle { font-size: 20px; font-weight: 600; letter-spacing: -0.4px; }
        QLabel#CardDescription { color: #9baaa6; font-size: 12px; }
        QLabel#FieldLabel { color: #a6b5ae; font-size: 10px; font-weight: 600; letter-spacing: 1px; }
        QWidget#ProjectStartup QPushButton {
            font-family: 'Segoe UI'; font-size: 12px; font-weight: 600;
            padding: 0 16px; min-height: 40px; border-radius: 8px;
        }
        QPushButton#CreateButton { background: #c7f58a; color: #172019; border: 1px solid #c7f58a; }
        QPushButton#CreateButton:hover { background: #d8ffa4; border-color: #e4ffc3; }
        QPushButton#CreateButton:pressed { background: #a9d36e; }
        QPushButton#LoadButton { background: #24332e; color: #d4ecc5; border: 1px solid #405644; }
        QPushButton#LoadButton:hover { background: #314638; border-color: #88a66a; }
        QPushButton#LoadButton:pressed { background: #1a2822; }
        QPushButton#LoadButton:disabled { background: #172126; color: #6d7d7e; border-color: #2c393d; }
        QPushButton#CreateButton:disabled { background: #46583b; color: #aebba3; border-color: #46583b; }
        QPushButton#BrowseButton { background: #1d2931; color: #b7c6c7; border: 1px solid #35444d; padding: 0 12px; }
        QPushButton#BrowseButton:hover { background: #2c3b44; color: #edf5ef; }
        QWidget#ProjectStartup QPushButton:focus { border: 1px solid #e0ffb9; }
        QWidget#ProjectStartup QLineEdit {
            background: #0c1319; color: #dce6e0; border: 1px solid #304049; border-radius: 8px;
            padding: 0 12px; min-height: 40px; font-family: 'Segoe UI'; font-size: 12px;
            selection-background-color: #c7f58a; selection-color: #172019;
        }
        QWidget#ProjectStartup QLineEdit:focus { border-color: #a8d377; background: #111c20; }
        QWidget#ProjectStartup QLineEdit:disabled { color: #687875; }
        QLabel#StartupStatus { color: #f1b49d; font-size: 12px; }
    )");

    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(40, 28, 40, 22);
    page->setSpacing(0);

    auto* header = new QHBoxLayout();
    auto* mark   = new QLabel("VP", this);
    mark->setObjectName("BrandMark");
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(38, 38);
    header->addWidget(mark);
    header->addSpacing(12);
    auto* brand = new QLabel("VulkanPlayGround", this);
    brand->setObjectName("Brand");
    header->addWidget(brand);
    header->addStretch();
    auto* edition = new QLabel("THE REALTIME GRAPHICS LAB", this);
    edition->setObjectName("Edition");
    header->addWidget(edition);
    page->addLayout(header);
    page->addSpacing(38);

    auto* content = new QHBoxLayout();
    content->setSpacing(44);
    auto* hero = new QVBoxLayout();
    hero->setSpacing(12);
    addLabel(hero, "IMAGINE. ITERATE. RENDER.", "Eyebrow");
    addLabel(hero, "Ideas in light.<br>Worlds in motion.", "Headline");
    addLabel(hero, "A playground for your next dimension.", "Intro");
    hero->addWidget(new StartupArtwork(this), 1);
    auto* artFooter = new QHBoxLayout();
    auto* artLabel  = new QLabel("01 / PARAMETRIC STUDY", this);
    artLabel->setObjectName("ArtCaption");
    artFooter->addWidget(artLabel);
    artFooter->addStretch();
    auto* liveLabel = new QLabel("FORM + LIGHT", this);
    liveLabel->setObjectName("ArtCaption");
    artFooter->addWidget(liveLabel);
    hero->addLayout(artFooter);
    content->addLayout(hero, 1);

    _actions = new QWidget(this);
    _actions->setFixedWidth(360);
    auto* actions = new QVBoxLayout(_actions);
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(14);
    addLabel(actions, "YOUR WORKSPACE", "Eyebrow");
    addLabel(actions, "Where do we begin?", "WorkspaceTitle");

    auto* createCard = new QFrame(_actions);
    createCard->setObjectName("CreateCard");
    auto* createLayout = new QVBoxLayout(createCard);
    createLayout->setContentsMargins(22, 18, 22, 20);
    createLayout->setSpacing(10);
    addLabel(createLayout, "01 / A FRESH PERSPECTIVE", "CardIndex");
    addLabel(createLayout, "Create a project", "CardTitle");
    addLabel(createLayout, "Choose an empty folder. Your project will be saved here.", "CardDescription");
    createLayout->addSpacing(4);
    auto* createPathLabel = addLabel(createLayout, "SAVE LOCATION", "FieldLabel");
    auto* createPathRow   = new QHBoxLayout();
    createPathRow->setSpacing(8);
    auto* createPathEdit = new QLineEdit(createCard);
    createPathEdit->setMinimumWidth(0);
    createPathEdit->setPlaceholderText("Select an empty folder...");
    createPathEdit->setClearButtonEnabled(true);
    createPathEdit->setAccessibleName("New project save location");
    createPathLabel->setBuddy(createPathEdit);
    createPathRow->addWidget(createPathEdit, 1);
    auto* createBrowseButton = new QPushButton("Browse", createCard);
    createBrowseButton->setObjectName("BrowseButton");
    createBrowseButton->setCursor(Qt::PointingHandCursor);
    createPathRow->addWidget(createBrowseButton);
    createLayout->addLayout(createPathRow);
    auto* createButton = new QPushButton("Create project  +", createCard);
    createButton->setObjectName("CreateButton");
    createButton->setCursor(Qt::PointingHandCursor);
    createButton->setEnabled(false);
    createLayout->addWidget(createButton);
    actions->addWidget(createCard);
    connect(createBrowseButton, &QPushButton::clicked, this,
            [this, createPathEdit]()
            {
                const QString directory =
                    QFileDialog::getExistingDirectory(this, "Choose or Create an Empty Project Folder", createPathEdit->text().trimmed());
                if (!directory.isEmpty())
                {
                    createPathEdit->setText(QDir::toNativeSeparators(directory));
                }
            });
    connect(createPathEdit, &QLineEdit::textChanged, this,
            [this, createButton, createPathEdit](const QString& text)
            {
                createButton->setEnabled(!text.trimmed().isEmpty());
                createPathEdit->setToolTip(text);
                _status->clear();
            });
    connect(createButton, &QPushButton::clicked, this,
            [this, createPathEdit]()
            {
                const QFileInfo directory(createPathEdit->text().trimmed());
                if (createPathEdit->text().trimmed().isEmpty() || !directory.isDir() || !directory.isWritable())
                {
                    showError("Choose an existing, writable folder for your new project.");
                    createPathEdit->setFocus();
                    return;
                }
                if (!QDir(directory.absoluteFilePath()).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
                {
                    showError("Choose an empty folder for a new project. To use an existing project, choose Open project.");
                    createPathEdit->setFocus();
                    return;
                }
                emit projectSelected(directory.absoluteFilePath());
            });
    connect(createPathEdit, &QLineEdit::returnPressed, createButton, &QPushButton::click);

    auto* openCard = new QFrame(_actions);
    openCard->setObjectName("OpenCard");
    auto* openLayout = new QVBoxLayout(openCard);
    openLayout->setContentsMargins(22, 18, 22, 20);
    openLayout->setSpacing(10);
    addLabel(openLayout, "02 / BACK TO EXPLORING", "CardIndex");
    addLabel(openLayout, "Open a project", "CardTitle");
    addLabel(openLayout, "Pick up where your last idea left off.", "CardDescription");
    openLayout->addSpacing(4);
    auto* pathLabel = addLabel(openLayout, "PROJECT FOLDER", "FieldLabel");
    auto* pathRow   = new QHBoxLayout();
    pathRow->setSpacing(8);
    auto* pathEdit = new QLineEdit(openCard);
    pathEdit->setMinimumWidth(0);
    pathEdit->setPlaceholderText("Select a folder...");
    pathEdit->setClearButtonEnabled(true);
    pathEdit->setAccessibleName("Project folder");
    pathLabel->setBuddy(pathEdit);
    pathRow->addWidget(pathEdit, 1);
    auto* browseButton = new QPushButton("Browse", openCard);
    browseButton->setObjectName("BrowseButton");
    browseButton->setCursor(Qt::PointingHandCursor);
    pathRow->addWidget(browseButton);
    openLayout->addLayout(pathRow);
    auto* loadButton = new QPushButton("Open project  /", openCard);
    loadButton->setObjectName("LoadButton");
    loadButton->setCursor(Qt::PointingHandCursor);
    loadButton->setEnabled(false);
    openLayout->addWidget(loadButton);
    actions->addWidget(openCard);
    actions->addStretch();
    content->addWidget(_actions);
    page->addLayout(content, 1);

    _status = new QLabel(this);
    _status->setObjectName("StartupStatus");
    _status->setWordWrap(true);
    _status->setMinimumHeight(30);
    page->addWidget(_status);
    auto* footer     = new QHBoxLayout();
    auto* footerLeft = new QLabel("VULKAN / REALTIME / EXPERIMENTAL", this);
    footerLeft->setObjectName("Footer");
    footer->addWidget(footerLeft);
    footer->addStretch();
    auto* footerRight = new QLabel("BUILT FOR CURIOSITY", this);
    footerRight->setObjectName("Footer");
    footer->addWidget(footerRight);
    page->addLayout(footer);

    connect(browseButton, &QPushButton::clicked, this,
            [this, pathEdit]()
            {
                const QString directory = QFileDialog::getExistingDirectory(this, "Select Project Folder", pathEdit->text().trimmed());
                if (!directory.isEmpty())
                {
                    pathEdit->setText(QDir::toNativeSeparators(directory));
                }
            });
    connect(pathEdit, &QLineEdit::textChanged, this,
            [this, loadButton, pathEdit](const QString& text)
            {
                loadButton->setEnabled(!text.trimmed().isEmpty());
                pathEdit->setToolTip(text);
                _status->clear();
            });
    connect(loadButton, &QPushButton::clicked, this,
            [this, pathEdit]()
            {
                const QFileInfo directory(pathEdit->text().trimmed());
                if (pathEdit->text().trimmed().isEmpty() || !directory.isDir())
                {
                    showError("That folder doesn't exist. Choose an existing project folder to continue.");
                    pathEdit->setFocus();
                    return;
                }
                emit projectSelected(directory.absoluteFilePath());
            });
    connect(pathEdit, &QLineEdit::returnPressed, loadButton, &QPushButton::click);
}

void ProjectStartupWidget::showError(const QString& message)
{
    _status->setText(message);
}

void ProjectStartupWidget::showStarting()
{
    _actions->setEnabled(false);
    _status->setStyleSheet("color: #c7f58a;");
    _status->setText("Preparing your workspace...");
}

} // namespace Play::editor
