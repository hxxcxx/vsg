#include "app/param_panel.h"

#include "framework/param_set.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

using namespace vsglab::framework;

namespace {

QString qstr(const std::string& s) {
    return QString::fromStdString(s);
}

QColor toQColor(const vsg::vec4& c) {
    return QColor::fromRgbF(c.r, c.g, c.b, c.a);
}

void updateSwatch(QPushButton* button, const QColor& color) {
    button->setText(color.name(QColor::HexArgb));
    button->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: %2; }")
                                  .arg(color.name(QColor::HexRgb),
                                       color.lightnessF() > 0.5 ? QStringLiteral("black") : QStringLiteral("white")));
}

}  // namespace

ParamPanel::ParamPanel(QWidget* parent) : QWidget(parent) {
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(6);
}

void ParamPanel::clear() {
    while (auto* item = layout_->takeAt(0)) {
        if (auto* w = item->widget())
            w->deleteLater();
        delete item;
    }
}

void ParamPanel::addGroup(ParamSet& params, bool expanded) {
    if (params.empty())
        return;

    auto* header = new QToolButton(this);
    header->setText(qstr(params.title()));
    header->setCheckable(true);
    header->setChecked(expanded);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    header->setAutoRaise(true);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont font = header->font();
    font.setBold(true);
    header->setFont(font);

    auto* content = new QWidget(this);
    auto* form = new QFormLayout(content);
    form->setContentsMargins(12, 0, 0, 6);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const QString restartHint = tr("Changing this restarts the example");
    for (auto& param : params.params()) {
        QWidget* editor = createEditor(*param);
        if (param->apply() == Apply::Restart)
            editor->setToolTip(restartHint);

        if (param->kind() == ParamBase::Kind::Action) {
            form->addRow(editor);
        } else {
            auto* label = new QLabel(qstr(param->label()), content);
            if (param->apply() == Apply::Restart)
                label->setToolTip(restartHint);
            form->addRow(label, editor);
        }
    }

    content->setVisible(expanded);
    connect(header, &QToolButton::toggled, this, [header, content](bool on) {
        header->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        content->setVisible(on);
    });

    layout_->addWidget(header);
    layout_->addWidget(content);
}

void ParamPanel::finish() {
    layout_->addStretch(1);
}

QWidget* ParamPanel::createEditor(ParamBase& param) {
    // 先用当前值初始化，再连接信号，避免初始化触发修改通知
    switch (param.kind()) {
    case ParamBase::Kind::Bool: {
        auto& p = static_cast<BoolParam&>(param);
        auto* box = new QCheckBox(this);
        box->setChecked(p.value());
        connect(box, &QCheckBox::toggled, this, [&p](bool on) { p.set(on); });
        return box;
    }
    case ParamBase::Kind::Int: {
        auto& p = static_cast<IntParam&>(param);
        auto* spin = new QSpinBox(this);
        spin->setRange(p.min, p.max);
        spin->setValue(p.value());
        spin->setKeyboardTracking(false);  // 回车/失焦时才提交
        connect(spin, &QSpinBox::valueChanged, this, [&p](int v) { p.set(v); });
        return spin;
    }
    case ParamBase::Kind::Double: {
        auto& p = static_cast<DoubleParam&>(param);
        auto* spin = new QDoubleSpinBox(this);
        const int decimals = p.step > 0.0 ? std::max(0, static_cast<int>(std::ceil(-std::log10(p.step)))) + 1 : 3;
        spin->setDecimals(decimals);
        spin->setRange(p.min, p.max);
        spin->setSingleStep(p.step);
        spin->setValue(p.value());
        spin->setKeyboardTracking(false);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [&p](double v) { p.set(v); });
        return spin;
    }
    case ParamBase::Kind::Choice: {
        auto& p = static_cast<ChoiceParam&>(param);
        auto* combo = new QComboBox(this);
        for (auto& option : p.options)
            combo->addItem(qstr(option));
        combo->setCurrentText(qstr(p.value()));
        connect(combo, &QComboBox::currentTextChanged, this, [&p](const QString& text) { p.set(text.toStdString()); });
        return combo;
    }
    case ParamBase::Kind::File: {
        auto& p = static_cast<FileParam&>(param);
        auto* row = new QWidget(this);
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(0, 0, 0, 0);

        auto* combo = new QComboBox(row);
        combo->setEditable(true);
        combo->setInsertPolicy(QComboBox::NoInsert);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        for (auto& s : p.suggestions)
            combo->addItem(qstr(s));
        combo->setCurrentText(qstr(p.value()));

        auto* browse = new QToolButton(row);
        browse->setText(QStringLiteral("..."));

        hbox->addWidget(combo, 1);
        hbox->addWidget(browse);

        // 下拉选择即提交；手工输入在回车/失焦时提交
        connect(combo, &QComboBox::activated, this, [&p, combo](int) { p.set(combo->currentText().toStdString()); });
        connect(combo->lineEdit(), &QLineEdit::editingFinished, this,
                [&p, combo]() { p.set(combo->currentText().toStdString()); });
        connect(browse, &QToolButton::clicked, this, [this, &p, combo]() {
            const QString file = QFileDialog::getOpenFileName(this, qstr(p.label()), QStringLiteral(VSGLAB_DATA_DIR),
                                                              qstr(p.filter));
            if (file.isEmpty())
                return;
            combo->setCurrentText(file);
            p.set(file.toStdString());
        });
        return row;
    }
    case ParamBase::Kind::Color: {
        auto& p = static_cast<ColorParam&>(param);
        auto* button = new QPushButton(this);
        updateSwatch(button, toQColor(p.value()));
        connect(button, &QPushButton::clicked, this, [this, &p, button]() {
            const QColor color =
                    QColorDialog::getColor(toQColor(p.value()), this, qstr(p.label()), QColorDialog::ShowAlphaChannel);
            if (!color.isValid())
                return;
            updateSwatch(button, color);
            p.set(vsg::vec4(static_cast<float>(color.redF()), static_cast<float>(color.greenF()),
                            static_cast<float>(color.blueF()), static_cast<float>(color.alphaF())));
        });
        return button;
    }
    case ParamBase::Kind::Action: {
        auto& p = static_cast<ActionParam&>(param);
        auto* button = new QPushButton(qstr(p.label()), this);
        connect(button, &QPushButton::clicked, this, [&p]() { p.trigger(); });
        return button;
    }
    }
    return new QWidget(this);
}
