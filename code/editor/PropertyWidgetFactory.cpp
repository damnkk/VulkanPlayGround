#include "editor/PropertyWidgetFactory.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QAbstractItemView>

#include <rttr/enumeration.h>

#include "editor/RuntimeEditor.h"

namespace Play::editor
{

namespace
{

void updateAssetOptions(QComboBox* combo, const EditorProperty& property, const EditorSnapshot& snapshot)
{
    int  index     = 1; // Index zero is None.
    bool unchanged = combo->count() > 0;
    for (const auto& asset : snapshot.assets)
    {
        if (asset.type != property.resourceType) continue;
        unchanged = unchanged && index < combo->count() && combo->itemData(index).toString() == QString::fromStdString(asset.id) &&
                    combo->itemText(index) == QString::fromStdString(asset.name) &&
                    combo->itemData(index, Qt::ToolTipRole).toString() == QString::fromStdString(asset.path);
        ++index;
    }
    if (unchanged && index == combo->count()) return;

    combo->clear();
    combo->addItem("None", QString());
    for (const auto& asset : snapshot.assets)
    {
        if (asset.type != property.resourceType) continue;
        combo->addItem(QString::fromStdString(asset.name), QString::fromStdString(asset.id));
        combo->setItemData(combo->count() - 1, QString::fromStdString(asset.path), Qt::ToolTipRole);
    }
}

} // namespace

QWidget* PropertyWidgetFactory::create(const EditorProperty& property, const EditorSnapshot& snapshot, EditorNodeId nodeId,
                                       EditorComponentId componentId, RuntimeEditor& editor, QWidget* parent)
{
    const auto submitValue = [&editor, nodeId, componentId, name = property.name](rttr::variant value)
    {
        EditorCommand command;
        command.type        = EditorCommandType::SetProperty;
        command.nodeId      = nodeId;
        command.componentId = componentId;
        command.property    = name;
        command.value       = std::move(value);
        editor.submit(std::move(command));
    };

    if (!property.resourceType.empty())
    {
        QComboBox* combo = new QComboBox(parent);
        combo->setMinimumWidth(0);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setFixedHeight(26);
        combo->setEnabled(!property.readOnly);
        update(combo, property, snapshot);
        QObject::connect(combo, &QComboBox::activated, combo,
                         [&editor, combo, nodeId, componentId, name = property.name](int index)
                         {
                             EditorCommand command;
                             command.type        = index == 0 ? EditorCommandType::ClearAsset : EditorCommandType::BindAsset;
                             command.nodeId      = nodeId;
                             command.componentId = componentId;
                             command.property    = name;
                             command.assetId     = combo->itemData(index).toString().toStdString();
                             editor.submit(std::move(command));
                         });
        return combo;
    }

    if (property.type == rttr::type::get<bool>())
    {
        QCheckBox* checkBox = new QCheckBox(parent);
        checkBox->setChecked(property.value.to_bool());
        checkBox->setEnabled(!property.readOnly);
        QObject::connect(checkBox, &QCheckBox::toggled, checkBox, [submitValue](bool checked) { submitValue(rttr::variant(checked)); });
        return checkBox;
    }

    if (property.type.is_enumeration())
    {
        QComboBox*              combo       = new QComboBox(parent);
        const rttr::enumeration enumeration = property.type.get_enumeration();
        const std::string       currentName = enumeration.value_to_name(property.value).to_string();
        for (const rttr::string_view name : enumeration.get_names())
        {
            combo->addItem(QString::fromUtf8(name.data(), static_cast<int>(name.size())));
        }
        combo->setCurrentText(QString::fromStdString(currentName));
        combo->setEnabled(!property.readOnly);
        QObject::connect(combo, &QComboBox::currentTextChanged, combo,
                         [submitValue, enumeration](const QString& text) { submitValue(enumeration.name_to_value(text.toStdString())); });
        return combo;
    }

    if (property.type.is_arithmetic())
    {
        QDoubleSpinBox* spinBox = new QDoubleSpinBox(parent);
        spinBox->setKeyboardTracking(false);
        spinBox->setDecimals(4);
        spinBox->setRange(property.hasMinimum ? property.minimum : -1000000000.0, property.hasMaximum ? property.maximum : 1000000000.0);
        spinBox->setSingleStep(property.step);
        spinBox->setValue(property.value.to_double());
        spinBox->setEnabled(!property.readOnly);
        QObject::connect(spinBox, &QDoubleSpinBox::valueChanged, spinBox,
                         [submitValue, type = property.type](double value)
                         {
                             rttr::variant typedValue(value);
                             typedValue.convert(type);
                             submitValue(std::move(typedValue));
                         });
        return spinBox;
    }

    if (property.type == rttr::type::get<std::string>())
    {
        QLineEdit* lineEdit = new QLineEdit(QString::fromStdString(property.value.to_string()), parent);
        lineEdit->setReadOnly(property.readOnly);
        QObject::connect(lineEdit, &QLineEdit::editingFinished, lineEdit,
                         [submitValue, lineEdit]() { submitValue(rttr::variant(lineEdit->text().toStdString())); });
        return lineEdit;
    }

    QLabel* value = new QLabel(QString::fromStdString(property.value.to_string()), parent);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return value;
}

bool PropertyWidgetFactory::update(QWidget* widget, const EditorProperty& property, const EditorSnapshot& snapshot)
{
    const QSignalBlocker blocker(widget);
    if (!property.resourceType.empty())
    {
        auto* combo = qobject_cast<QComboBox*>(widget);
        if (combo->view()->isVisible()) return false;
        updateAssetOptions(combo, property, snapshot);
        const int index = combo->findData(QString::fromStdString(property.resourceId));
        combo->setCurrentIndex(index < 0 ? 0 : index);
        combo->setEnabled(!property.readOnly);
    }
    else if (auto* check = qobject_cast<QCheckBox*>(widget))
    {
        check->setChecked(property.value.to_bool());
        check->setEnabled(!property.readOnly);
    }
    else if (auto* spin = qobject_cast<QDoubleSpinBox*>(widget))
    {
        if (spin->hasFocus()) return false;
        spin->setValue(property.value.to_double());
    }
    else if (auto* combo = qobject_cast<QComboBox*>(widget))
    {
        if (combo->view()->isVisible()) return false;
        combo->setCurrentText(QString::fromStdString(property.type.get_enumeration().value_to_name(property.value).to_string()));
    }
    else if (auto* line = qobject_cast<QLineEdit*>(widget))
    {
        if (line->hasFocus()) return false;
        line->setText(QString::fromStdString(property.value.to_string()));
    }
    else if (auto* label = qobject_cast<QLabel*>(widget))
    {
        label->setText(QString::fromStdString(property.value.to_string()));
    }
    return true;
}

} // namespace Play::editor
