/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "customshaderhelper.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace LeapK {

class ParameterEditDialog : public QDialog
{
public:
    explicit ParameterEditDialog(QWidget *parent = nullptr, const LeapKShader::ShaderParameter *initial = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(initial ? QStringLiteral("Edit Uniform Parameter") : QStringLiteral("Add Uniform Parameter"));
        setMinimumWidth(440);

        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout();

        m_editId = new QLineEdit(this);
        m_editId->setPlaceholderText(QStringLiteral("e.g. speed, intensity, matrixColor"));
        form->addRow(QStringLiteral("Uniform ID (in GLSL):"), m_editId);

        m_editName = new QLineEdit(this);
        m_editName->setPlaceholderText(QStringLiteral("e.g. Wave Speed"));
        form->addRow(QStringLiteral("Name / Label:"), m_editName);

        m_comboType = new QComboBox(this);
        m_comboType->addItem(QStringLiteral("float (floating point)"), QStringLiteral("float"));
        m_comboType->addItem(QStringLiteral("int (integer)"), QStringLiteral("int"));
        m_comboType->addItem(QStringLiteral("bool (toggle / flag)"), QStringLiteral("bool"));
        m_comboType->addItem(QStringLiteral("color (RGBA color)"), QStringLiteral("color"));
        form->addRow(QStringLiteral("Data type:"), m_comboType);

        m_spinMin = new QDoubleSpinBox(this);
        m_spinMin->setRange(-100000.0, 100000.0);
        m_spinMin->setDecimals(3);
        m_spinMin->setValue(0.0);
        form->addRow(QStringLiteral("Minimum value:"), m_spinMin);

        m_spinMax = new QDoubleSpinBox(this);
        m_spinMax->setRange(-100000.0, 100000.0);
        m_spinMax->setDecimals(3);
        m_spinMax->setValue(1.0);
        form->addRow(QStringLiteral("Maximum value:"), m_spinMax);

        m_spinStep = new QDoubleSpinBox(this);
        m_spinStep->setRange(0.0001, 1000.0);
        m_spinStep->setDecimals(4);
        m_spinStep->setValue(0.05);
        form->addRow(QStringLiteral("Step:"), m_spinStep);

        m_spinDefault = new QDoubleSpinBox(this);
        m_spinDefault->setRange(-100000.0, 100000.0);
        m_spinDefault->setDecimals(3);
        m_spinDefault->setValue(0.5);
        form->addRow(QStringLiteral("Default value:"), m_spinDefault);

        m_editSuffix = new QLineEdit(this);
        m_editSuffix->setPlaceholderText(QStringLiteral("e.g.  Hz, x, %,  px"));
        form->addRow(QStringLiteral("Unit / Suffix:"), m_editSuffix);

        m_btnColor = new QPushButton(QStringLiteral("Select default color…"), this);
        m_selectedColor = QColor(QStringLiteral("#00f0ff"));
        updateColorButton();
        form->addRow(QStringLiteral("Default color:"), m_btnColor);

        layout->addLayout(form);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        layout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &ParameterEditDialog::validateAndAccept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        connect(m_comboType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            const QString type = m_comboType->currentData().toString();
            const bool isNumber = (type == QStringLiteral("float") || type == QStringLiteral("int"));
            const bool isColor = (type == QStringLiteral("color"));
            m_spinMin->setEnabled(isNumber);
            m_spinMax->setEnabled(isNumber);
            m_spinStep->setEnabled(isNumber);
            m_spinDefault->setEnabled(isNumber);
            m_editSuffix->setEnabled(isNumber);
            m_btnColor->setEnabled(isColor);
        });

        connect(m_btnColor, &QPushButton::clicked, this, [this]() {
            const QColor c = QColorDialog::getColor(m_selectedColor, this, QStringLiteral("Select Color"));
            if (c.isValid()) {
                m_selectedColor = c;
                updateColorButton();
            }
        });

        if (initial) {
            m_editId->setText(initial->id);
            m_editName->setText(initial->name);
            int idx = m_comboType->findData(initial->type);
            if (idx >= 0) m_comboType->setCurrentIndex(idx);
            m_spinMin->setValue(initial->minValue);
            m_spinMax->setValue(initial->maxValue);
            m_spinStep->setValue(initial->step);
            m_spinDefault->setValue(initial->defaultValue);
            m_editSuffix->setText(initial->suffix);
            m_selectedColor = QColor(initial->colorDefault);
            updateColorButton();
        }

        // Trigger update for initial type
        const QString type = m_comboType->currentData().toString();
        const bool isNumber = (type == QStringLiteral("float") || type == QStringLiteral("int"));
        const bool isColor = (type == QStringLiteral("color"));
        m_spinMin->setEnabled(isNumber);
        m_spinMax->setEnabled(isNumber);
        m_spinStep->setEnabled(isNumber);
        m_spinDefault->setEnabled(isNumber);
        m_editSuffix->setEnabled(isNumber);
        m_btnColor->setEnabled(isColor);
    }

    LeapKShader::ShaderParameter getParameter() const
    {
        LeapKShader::ShaderParameter p;
        p.id = m_editId->text().trimmed();
        p.name = m_editName->text().trimmed().isEmpty() ? p.id : m_editName->text().trimmed();
        p.type = m_comboType->currentData().toString();
        p.minValue = m_spinMin->value();
        p.maxValue = m_spinMax->value();
        p.step = m_spinStep->value();
        p.defaultValue = m_spinDefault->value();
        p.currentValue = p.defaultValue;
        p.suffix = m_editSuffix->text();
        p.colorDefault = m_selectedColor.name();
        p.colorCurrent = p.colorDefault;
        return p;
    }

private:
    void updateColorButton()
    {
        m_btnColor->setStyleSheet(QStringLiteral("background-color: %1; color: %2; font-weight: bold; border-radius: 4px; padding: 4px;")
            .arg(m_selectedColor.name(), (m_selectedColor.lightness() > 128) ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
        m_btnColor->setText(m_selectedColor.name());
    }

    void validateAndAccept()
    {
        const QString id = m_editId->text().trimmed();
        if (id.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Validation Error"), QStringLiteral("Parameter ID (uniform name) cannot be empty."));
            return;
        }
        const QRegularExpression regex(QStringLiteral("^[a-zA-Z_][a-zA-Z0-9_]*$"));
        if (!regex.match(id).hasMatch()) {
            QMessageBox::warning(this, QStringLiteral("Validation Error"), QStringLiteral("Parameter ID must be a valid C/GLSL identifier (no spaces or special characters)."));
            return;
        }
        accept();
    }

    QLineEdit *m_editId;
    QLineEdit *m_editName;
    QComboBox *m_comboType;
    QDoubleSpinBox *m_spinMin;
    QDoubleSpinBox *m_spinMax;
    QDoubleSpinBox *m_spinStep;
    QDoubleSpinBox *m_spinDefault;
    QLineEdit *m_editSuffix;
    QPushButton *m_btnColor;
    QColor m_selectedColor;
};

class ShaderEditorDialog : public QDialog
{
public:
    explicit ShaderEditorDialog(QWidget *parent = nullptr, const LeapKShader::CustomShaderEffect *initialEffect = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(initialEffect
            ? QStringLiteral("Shader Effect Editor - %1").arg(initialEffect->name)
            : QStringLiteral("New GLSL Shader Effect"));
        resize(780, 640);

        auto *mainLayout = new QVBoxLayout(this);
        auto *tabs = new QTabWidget(this);

        // --- TAB 1: General & Parameters ---
        auto *tabGeneral = new QWidget(this);
        auto *layoutGeneral = new QVBoxLayout(tabGeneral);

        auto *groupMeta = new QGroupBox(QStringLiteral("Metadata"), tabGeneral);
        auto *formMeta = new QFormLayout(groupMeta);

        m_editName = new QLineEdit(groupMeta);
        m_editName->setPlaceholderText(QStringLiteral("e.g. Cyberpunk Grid Scanner"));
        formMeta->addRow(QStringLiteral("Effect name:"), m_editName);

        m_editId = new QLineEdit(groupMeta);
        m_editId->setPlaceholderText(QStringLiteral("e.g. cyberpunk_grid"));
        formMeta->addRow(QStringLiteral("Identifier (ID):"), m_editId);

        m_comboTarget = new QComboBox(groupMeta);
        m_comboTarget->addItem(QStringLiteral("Window Aura / Glow"), QStringLiteral("glow"));
        m_comboTarget->addItem(QStringLiteral("Window Outline / Border"), QStringLiteral("outline"));
        m_comboTarget->addItem(QStringLiteral("Both Aura and Outline"), QStringLiteral("both"));
        formMeta->addRow(QStringLiteral("Rendering target:"), m_comboTarget);

        m_editDesc = new QLineEdit(groupMeta);
        m_editDesc->setPlaceholderText(QStringLiteral("Short visual effect description..."));
        formMeta->addRow(QStringLiteral("Description:"), m_editDesc);

        m_editAuthor = new QLineEdit(groupMeta);
        m_editAuthor->setPlaceholderText(QStringLiteral("Your name or handle"));
        formMeta->addRow(QStringLiteral("Author:"), m_editAuthor);

        m_editVersion = new QLineEdit(groupMeta);
        m_editVersion->setText(QStringLiteral("1.0"));
        formMeta->addRow(QStringLiteral("Version:"), m_editVersion);

        layoutGeneral->addWidget(groupMeta);

        // Parameters Table
        auto *groupParams = new QGroupBox(QStringLiteral("Dynamic Parameters (Uniforms)"), tabGeneral);
        auto *layoutParams = new QVBoxLayout(groupParams);

        m_tableParams = new QTableWidget(groupParams);
        m_tableParams->setColumnCount(5);
        m_tableParams->setHorizontalHeaderLabels({
            QStringLiteral("Uniform ID"),
            QStringLiteral("Name / Label"),
            QStringLiteral("Type"),
            QStringLiteral("Range / Details"),
            QStringLiteral("Default Value")
        });
        m_tableParams->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        m_tableParams->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_tableParams->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_tableParams->setSelectionMode(QAbstractItemView::SingleSelection);
        layoutParams->addWidget(m_tableParams);

        auto *layoutParamButtons = new QHBoxLayout();
        m_btnAddParam = new QPushButton(QStringLiteral("➕ Add Parameter…"), groupParams);
        m_btnEditParam = new QPushButton(QStringLiteral("✏️ Edit Parameter…"), groupParams);
        m_btnRemoveParam = new QPushButton(QStringLiteral("🗑️ Remove Parameter"), groupParams);
        layoutParamButtons->addWidget(m_btnAddParam);
        layoutParamButtons->addWidget(m_btnEditParam);
        layoutParamButtons->addWidget(m_btnRemoveParam);
        layoutParamButtons->addStretch();
        layoutParams->addLayout(layoutParamButtons);

        layoutGeneral->addWidget(groupParams);
        tabs->addTab(tabGeneral, QStringLiteral("General & Parameters"));

        // --- TAB 2: GLSL Code ---
        auto *tabGlsl = new QWidget(this);
        auto *layoutGlsl = new QVBoxLayout(tabGlsl);

        auto *labelInfo = new QLabel(
            QStringLiteral("<b>Available GLSL Inputs:</b><br/>"
                           "• <code>baseColor</code> (vec4) – base aura/outline color with corner fading and distance roll-off<br/>"
                           "• <code>pos</code> (vec2) – pixel screen coordinates (position0)<br/>"
                           "• <code>box</code> (vec4) – window coordinates and dimensions (box.xy = center, box.zw = half sizes)<br/>"
                           "• <code>dist</code> (float) – distance from window border<br/>"
                           "• <code>time</code> (float) – elapsed time in seconds (60 FPS animation)<br/>"
                           "• Any defined uniform variables from the Parameters tab (as <code>uniform float / vec4 / int</code>)"),
            tabGlsl
        );
        labelInfo->setStyleSheet(QStringLiteral("background-color: palette(base); border: 1px solid palette(mid); border-radius: 4px; padding: 6px; font-size: 11px;"));
        layoutGlsl->addWidget(labelInfo);

        m_editGlsl = new QPlainTextEdit(tabGlsl);
        QFont monoFont(QStringLiteral("Monospace"));
        monoFont.setStyleHint(QFont::Monospace);
        monoFont.setPointSize(10);
        m_editGlsl->setFont(monoFont);
        m_editGlsl->setTabStopDistance(32);
        layoutGlsl->addWidget(m_editGlsl);

        auto *layoutCodeActions = new QHBoxLayout();
        m_btnValidate = new QPushButton(QStringLiteral("🔍 Validate GLSL Code"), tabGlsl);
        m_btnInsertTemplate = new QPushButton(QStringLiteral("📋 Insert Starter Template"), tabGlsl);
        layoutCodeActions->addWidget(m_btnValidate);
        layoutCodeActions->addWidget(m_btnInsertTemplate);
        layoutCodeActions->addStretch();
        layoutGlsl->addLayout(layoutCodeActions);

        tabs->addTab(tabGlsl, QStringLiteral("GLSL Source Code"));

        mainLayout->addWidget(tabs);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        mainLayout->addWidget(buttonBox);

        // Connections
        connect(m_editName, &QLineEdit::textChanged, this, [this](const QString &text) {
            if (m_editId->text().isEmpty() || m_autoId) {
                m_autoId = true;
                QString slug = text.toLower();
                slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9_]+")), QStringLiteral("_"));
                slug.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
                m_editId->setText(slug);
            }
        });
        connect(m_editId, &QLineEdit::textEdited, this, [this]() {
            m_autoId = false;
        });

        connect(m_btnAddParam, &QPushButton::clicked, this, &ShaderEditorDialog::addParameter);
        connect(m_btnEditParam, &QPushButton::clicked, this, &ShaderEditorDialog::editParameter);
        connect(m_btnRemoveParam, &QPushButton::clicked, this, &ShaderEditorDialog::removeParameter);
        connect(m_tableParams, &QTableWidget::cellDoubleClicked, this, &ShaderEditorDialog::editParameter);

        connect(m_btnValidate, &QPushButton::clicked, this, &ShaderEditorDialog::validateGlsl);
        connect(m_btnInsertTemplate, &QPushButton::clicked, this, &ShaderEditorDialog::insertTemplate);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &ShaderEditorDialog::validateAndSave);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        if (initialEffect) {
            m_autoId = false;
            m_editName->setText(initialEffect->name);
            m_editId->setText(initialEffect->id);
            m_editDesc->setText(initialEffect->description);
            m_editAuthor->setText(initialEffect->author);
            m_editVersion->setText(initialEffect->version);
            int idx = m_comboTarget->findData(initialEffect->target);
            if (idx >= 0) m_comboTarget->setCurrentIndex(idx);
            m_parameters = initialEffect->parameters;
            m_editGlsl->setPlainText(initialEffect->glslCode);
            m_isBuiltIn = initialEffect->isBuiltIn;
        } else {
            insertTemplate();
        }

        refreshParametersTable();
    }

    LeapKShader::CustomShaderEffect getEffect() const
    {
        LeapKShader::CustomShaderEffect e;
        e.id = m_editId->text().trimmed();
        e.name = m_editName->text().trimmed();
        e.description = m_editDesc->text().trimmed();
        e.author = m_editAuthor->text().trimmed();
        e.version = m_editVersion->text().trimmed().isEmpty() ? QStringLiteral("1.0") : m_editVersion->text().trimmed();
        e.target = m_comboTarget->currentData().toString();
        e.parameters = m_parameters;
        e.glslCode = m_editGlsl->toPlainText();
        e.isBuiltIn = m_isBuiltIn;
        return e;
    }

private:
    void refreshParametersTable()
    {
        m_tableParams->setRowCount(m_parameters.size());
        for (int i = 0; i < m_parameters.size(); ++i) {
            const auto &p = m_parameters[i];
            m_tableParams->setItem(i, 0, new QTableWidgetItem(p.id));
            m_tableParams->setItem(i, 1, new QTableWidgetItem(p.name));
            m_tableParams->setItem(i, 2, new QTableWidgetItem(p.type));
            QString details;
            if (p.type == QStringLiteral("float") || p.type == QStringLiteral("int")) {
                details = QStringLiteral("[%1 - %2, step: %3%4]").arg(p.minValue).arg(p.maxValue).arg(p.step).arg(p.suffix);
            } else if (p.type == QStringLiteral("color")) {
                details = QStringLiteral("RGBA Color");
            } else {
                details = QStringLiteral("Toggle (On / Off)");
            }
            m_tableParams->setItem(i, 3, new QTableWidgetItem(details));

            QString defVal;
            if (p.type == QStringLiteral("color")) {
                defVal = p.colorDefault;
            } else {
                defVal = QString::number(p.defaultValue) + p.suffix;
            }
            m_tableParams->setItem(i, 4, new QTableWidgetItem(defVal));
        }
    }

    void addParameter()
    {
        ParameterEditDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            const auto p = dlg.getParameter();
            for (const auto &existing : m_parameters) {
                if (existing.id == p.id) {
                    QMessageBox::warning(this, QStringLiteral("Duplicate Parameter"),
                                         QStringLiteral("A parameter with identifier '%1' already exists.").arg(p.id));
                    return;
                }
            }
            m_parameters.append(p);
            refreshParametersTable();
        }
    }

    void editParameter()
    {
        const int row = m_tableParams->currentRow();
        if (row < 0 || row >= m_parameters.size()) {
            return;
        }
        ParameterEditDialog dlg(this, &m_parameters[row]);
        if (dlg.exec() == QDialog::Accepted) {
            const auto p = dlg.getParameter();
            for (int i = 0; i < m_parameters.size(); ++i) {
                if (i != row && m_parameters[i].id == p.id) {
                    QMessageBox::warning(this, QStringLiteral("Duplicate Parameter"),
                                         QStringLiteral("A parameter with identifier '%1' already exists.").arg(p.id));
                    return;
                }
            }
            m_parameters[row] = p;
            refreshParametersTable();
        }
    }

    void removeParameter()
    {
        const int row = m_tableParams->currentRow();
        if (row < 0 || row >= m_parameters.size()) {
            return;
        }
        m_parameters.removeAt(row);
        refreshParametersTable();
    }

    void insertTemplate()
    {
        if (!m_editGlsl->toPlainText().trimmed().isEmpty()) {
            if (QMessageBox::question(this, QStringLiteral("Insert Template"),
                                      QStringLiteral("Are you sure you want to replace current code with the starter template?"),
                                      QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
                return;
            }
        }
        m_editGlsl->setPlainText(QStringLiteral(
            "// Custom GLSL Effect\n"
            "// Available inputs:\n"
            "// - vec4 baseColor : base color with distance and corner falloff\n"
            "// - vec2 pos       : pixel coordinates (x, y)\n"
            "// - vec4 box       : outerBox of window (box.xy: center, box.zw: half sizes)\n"
            "// - float dist     : distance to window border\n"
            "// - float time     : elapsed time in seconds (smooth animation)\n"
            "// - defined uniform variables from Parameters tab\n\n"
            "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n"
            "{\n"
            "    // Example: Smooth dynamic color wave around window\n"
            "    float s = computePerimeterDistance(pos, box, 6.0);\n"
            "    float wave = 0.5 + 0.5 * sin(s * 0.01 - time * 3.0);\n"
            "    vec3 highlight = vec3(0.0, 0.9, 1.0) * wave;\n"
            "    return vec4(baseColor.rgb + highlight, baseColor.a);\n"
            "}\n"
        ));
    }

    void validateGlsl()
    {
        QString err;
        const bool valid = LeapKShader::validateShaderCode(m_editGlsl->toPlainText(), m_parameters, &err);
        if (valid) {
            QMessageBox::information(this, QStringLiteral("GLSL Validation"),
                                     QStringLiteral("✅ GLSL syntax and parameter definitions are valid!"));
        } else {
            QMessageBox::warning(this, QStringLiteral("GLSL Validation Error"),
                                 QStringLiteral("⚠️ Problem detected in GLSL code:\n\n%1").arg(err));
        }
    }

    void validateAndSave()
    {
        if (m_editName->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Missing Name"), QStringLiteral("Please enter a name for the shader effect."));
            return;
        }
        if (m_editId->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Missing ID"), QStringLiteral("Please enter an ID for the shader effect."));
            return;
        }
        QString err;
        if (!LeapKShader::validateShaderCode(m_editGlsl->toPlainText(), m_parameters, &err)) {
            if (QMessageBox::question(this, QStringLiteral("GLSL Syntax Warning"),
                                      QStringLiteral("Potential issue detected in GLSL code:\n\n%1\n\nDo you want to save anyway?").arg(err),
                                      QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
                return;
            }
        }
        accept();
    }

    QLineEdit *m_editName;
    QLineEdit *m_editId;
    QComboBox *m_comboTarget;
    QLineEdit *m_editDesc;
    QLineEdit *m_editAuthor;
    QLineEdit *m_editVersion;
    QTableWidget *m_tableParams;
    QPushButton *m_btnAddParam;
    QPushButton *m_btnEditParam;
    QPushButton *m_btnRemoveParam;
    QPlainTextEdit *m_editGlsl;
    QPushButton *m_btnValidate;
    QPushButton *m_btnInsertTemplate;
    QList<LeapKShader::ShaderParameter> m_parameters;
    bool m_autoId = true;
    bool m_isBuiltIn = false;
};

} // namespace LeapK
