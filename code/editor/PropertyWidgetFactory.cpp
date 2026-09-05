#include "editor/PropertyWidgetFactory.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>

#include <rttr/enumeration.h>

#include "editor/RuntimeEditor.h"

namespace Play::editor
{

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
        combo->addItem("None", QString());
        for (const EditorAsset& asset : snapshot.assets)
        {
            if (asset.type == property.resourceType)
            {
                combo->addItem(QString::fromStdString(asset.name), QString::fromStdString(asset.id));
            }
        }

        const int currentIndex = combo->findData(QString::fromStdString(property.resourceId));
        combo->setCurrentIndex(currentIndex < 0 ? 0 : currentIndex);
        combo->setEnabled(!property.readOnly);
        QObject::connect(combo, &QComboBox::currentIndexChanged, combo,
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
        QComboBox* combo       = new QComboBox(parent);
        const rttr::enumeration enumeration = property.type.get_enumeration();
        const std::string currentName        = enumeration.value_to_name(property.value).to_string();
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

} // namespace Play::editor
