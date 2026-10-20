/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "activewindowoutline_config.h"
#include "ui_activewindowoutline_config.h"

#include <KColorScheme>
#include <KConfigGroup>
#include <KConfigSkeleton>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QColorDialog>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QPainter>
#include <QPixmap>

#ifndef ACTIVE_WINDOW_OUTLINE_VERSION
#define ACTIVE_WINDOW_OUTLINE_VERSION "development"
#endif

K_PLUGIN_CLASS(ActiveWindowOutlineConfig)

ActiveWindowOutlineConfig::ActiveWindowOutlineConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
    , m_ui(new Ui::ActiveWindowOutlineConfigForm)
{
    m_ui->setupUi(widget());
    m_ui->label_versionValue->setText(QStringLiteral(ACTIVE_WINDOW_OUTLINE_VERSION));

    QPalette infoPalette = m_ui->label_info->palette();
    const QColor infoText = KColorScheme(QPalette::Active, KColorScheme::Window)
                                .foreground(KColorScheme::ActiveText)
                                .color();
    infoPalette.setColor(QPalette::WindowText, infoText);
    m_ui->label_info->setPalette(infoPalette);

    m_configSkeleton = new KConfigSkeleton(QStringLiteral("kwinrc"), this);
    m_configSkeleton->setCurrentGroup(QStringLiteral("Effect-active-window-outline"));

    // Tab 1: General
    m_configSkeleton->addItemBool(QStringLiteral("UseAccentColor"), *new bool, true);
    m_configSkeleton->addItemInt(QStringLiteral("AccentOpacity"), *new int, 66);
    m_configSkeleton->addItemColor(QStringLiteral("CustomColor"), *new QColor, QColor(61, 174, 233));
    m_configSkeleton->addItemInt(QStringLiteral("BorderWidth"), *new int, 2);
    m_configSkeleton->addItemInt(QStringLiteral("OutlineGap"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BorderRadius"), *new int, 6);
    m_configSkeleton->addItemBool(QStringLiteral("DrawInside"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("SmoothEdges"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("GlowEnabled"), *new bool, true);
    m_configSkeleton->addItemInt(QStringLiteral("GlowRadius"), *new int, 6);
    m_configSkeleton->addItemBool(QStringLiteral("HideOnFullScreen"), *new bool, true);

    // Tab 4: Behavior & Buffer Delays
    m_configSkeleton->addItemInt(QStringLiteral("MenuHideDelay"), *new int, 300);
    m_configSkeleton->addItemInt(QStringLiteral("MenuShowDelay"), *new int, 300);
    m_configSkeleton->addItemInt(QStringLiteral("WindowHideDelay"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("WindowShowDelay"), *new int, 0);

    // Tab 2: Color Cycling
    m_configSkeleton->addItemBool(QStringLiteral("ColorCycleEnabled"), *new bool, false);
    m_configSkeleton->addItemDouble(QStringLiteral("ColorTransitionDuration"), *new double, 2.0);
    m_configSkeleton->addItemInt(QStringLiteral("ColorCycleOrder"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("ColorCycleEasing"), *new int, 0);

    // Tab 3: Shader Effects
    m_configSkeleton->addItemBool(QStringLiteral("BreatheEnabled"), *new bool, false);
    m_configSkeleton->addItemDouble(QStringLiteral("BreatheSpeed"), *new double, 0.5);
    m_configSkeleton->addItemInt(QStringLiteral("BreatheIntensity"), *new int, 40);

    m_configSkeleton->addItemBool(QStringLiteral("GlitchEnabled"), *new bool, false);
    m_configSkeleton->addItemDouble(QStringLiteral("GlitchSpeed"), *new double, 1.0);
    m_configSkeleton->addItemInt(QStringLiteral("GlitchIntensity"), *new int, 50);
    m_configSkeleton->addItemBool(QStringLiteral("GlitchAnomalyEnabled"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("GlitchFrequency"), *new int, 3);

    m_configSkeleton->addItemBool(QStringLiteral("NeonFlickerEnabled"), *new bool, false);
    m_configSkeleton->addItemDouble(QStringLiteral("NeonFlickerInterval"), *new double, 3.0);
    m_configSkeleton->addItemDouble(QStringLiteral("NeonFlickerDuration"), *new double, 0.25);
    m_configSkeleton->addItemInt(QStringLiteral("NeonFlickerIntensity"), *new int, 30);

    m_configSkeleton->addItemBool(QStringLiteral("FlashEnabled"), *new bool, false);
    m_configSkeleton->addItemDouble(QStringLiteral("FlashInterval"), *new double, 4.0);
    m_configSkeleton->addItemDouble(QStringLiteral("FlashDuration"), *new double, 0.15);

    addConfig(m_configSkeleton, widget());

    // Setup table
    m_ui->tableColors->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ui->tableColors->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    connect(m_ui->btnAddColor, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(QColor(61, 174, 233), widget(), QStringLiteral("Select Color"));
        if (color.isValid()) {
            addColorRow(color, 3.0);
            markAsChanged();
        }
    });

    connect(m_ui->btnRemoveColor, &QPushButton::clicked, this, [this] {
        const int row = m_ui->tableColors->currentRow();
        if (row >= 0) {
            m_ui->tableColors->removeRow(row);
            markAsChanged();
        }
    });

    connect(m_ui->btnMoveUp, &QPushButton::clicked, this, [this] {
        const int row = m_ui->tableColors->currentRow();
        if (row > 0) {
            auto *colorItem = m_ui->tableColors->takeItem(row, 0);
            auto *spinWidget = m_ui->tableColors->cellWidget(row, 1);
            double holdTime = 3.0;
            if (auto *spin = qobject_cast<QDoubleSpinBox *>(spinWidget)) {
                holdTime = spin->value();
            }
            m_ui->tableColors->removeRow(row);
            const int targetRow = row - 1;
            m_ui->tableColors->insertRow(targetRow);
            m_ui->tableColors->setItem(targetRow, 0, colorItem);
            auto *newSpin = new QDoubleSpinBox(m_ui->tableColors);
            newSpin->setRange(0.1, 60.0);
            newSpin->setValue(holdTime);
            newSpin->setSuffix(QStringLiteral(" s"));
            connect(newSpin, &QDoubleSpinBox::valueChanged, this, [this] { markAsChanged(); });
            m_ui->tableColors->setCellWidget(targetRow, 1, newSpin);
            m_ui->tableColors->selectRow(targetRow);
            markAsChanged();
        }
    });

    connect(m_ui->btnMoveDown, &QPushButton::clicked, this, [this] {
        const int row = m_ui->tableColors->currentRow();
        if (row >= 0 && row < m_ui->tableColors->rowCount() - 1) {
            auto *colorItem = m_ui->tableColors->takeItem(row, 0);
            auto *spinWidget = m_ui->tableColors->cellWidget(row, 1);
            double holdTime = 3.0;
            if (auto *spin = qobject_cast<QDoubleSpinBox *>(spinWidget)) {
                holdTime = spin->value();
            }
            m_ui->tableColors->removeRow(row);
            const int targetRow = row + 1;
            m_ui->tableColors->insertRow(targetRow);
            m_ui->tableColors->setItem(targetRow, 0, colorItem);
            auto *newSpin = new QDoubleSpinBox(m_ui->tableColors);
            newSpin->setRange(0.1, 60.0);
            newSpin->setValue(holdTime);
            newSpin->setSuffix(QStringLiteral(" s"));
            connect(newSpin, &QDoubleSpinBox::valueChanged, this, [this] { markAsChanged(); });
            m_ui->tableColors->setCellWidget(targetRow, 1, newSpin);
            m_ui->tableColors->selectRow(targetRow);
            markAsChanged();
        }
    });

    connect(m_ui->tableColors, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        if (col == 0) {
            auto *item = m_ui->tableColors->item(row, 0);
            if (item) {
                const QColor initialColor = item->data(Qt::UserRole).value<QColor>();
                const QColor color = QColorDialog::getColor(initialColor, widget(), QStringLiteral("Edit Color"));
                if (color.isValid()) {
                    QPixmap iconPix(24, 24);
                    iconPix.fill(color);
                    item->setIcon(QIcon(iconPix));
                    item->setText(color.name(QColor::HexRgb));
                    item->setData(Qt::UserRole, color);
                    markAsChanged();
                }
            }
        }
    });

    // Color UI enable/disable sync
    auto updateColorEnabled = [this] {
        const bool useAccent = m_ui->kcfg_UseAccentColor->isChecked();
        m_ui->kcfg_AccentOpacity->setEnabled(useAccent);
        m_ui->label_accentOpacity->setEnabled(useAccent);
        m_ui->kcfg_CustomColor->setEnabled(!useAccent);
        m_ui->label_customColor->setEnabled(!useAccent);
    };
    connect(m_ui->kcfg_UseAccentColor, &QCheckBox::toggled, this, updateColorEnabled);
    updateColorEnabled();

    auto updateGlowEnabled = [this] {
        const bool enabled = m_ui->kcfg_GlowEnabled->isChecked();
        m_ui->kcfg_GlowRadius->setEnabled(enabled);
        m_ui->label_glowRadius->setEnabled(enabled);
    };
    connect(m_ui->kcfg_GlowEnabled, &QCheckBox::toggled, this, updateGlowEnabled);
    updateGlowEnabled();

    auto updateColorCycleUI = [this] {
        const bool enabled = m_ui->kcfg_ColorCycleEnabled->isChecked();
        m_ui->tableColors->setEnabled(enabled);
        m_ui->btnAddColor->setEnabled(enabled);
        m_ui->btnRemoveColor->setEnabled(enabled);
        m_ui->btnMoveUp->setEnabled(enabled);
        m_ui->btnMoveDown->setEnabled(enabled);
        m_ui->groupTransition->setEnabled(enabled);
    };
    connect(m_ui->kcfg_ColorCycleEnabled, &QCheckBox::toggled, this, updateColorCycleUI);
    updateColorCycleUI();

    auto updateBreatheUI = [this] {
        const bool enabled = m_ui->kcfg_BreatheEnabled->isChecked();
        m_ui->label_breatheSpeed->setEnabled(enabled);
        m_ui->kcfg_BreatheSpeed->setEnabled(enabled);
        m_ui->label_breatheIntensity->setEnabled(enabled);
        m_ui->kcfg_BreatheIntensity->setEnabled(enabled);
    };
    connect(m_ui->kcfg_BreatheEnabled, &QCheckBox::toggled, this, updateBreatheUI);
    updateBreatheUI();

    auto updateGlitchUI = [this] {
        const bool glitchOn = m_ui->kcfg_GlitchEnabled->isChecked();
        m_ui->label_glitchSpeed->setEnabled(glitchOn);
        m_ui->kcfg_GlitchSpeed->setEnabled(glitchOn);
        m_ui->label_glitchIntensity->setEnabled(glitchOn);
        m_ui->kcfg_GlitchIntensity->setEnabled(glitchOn);
        m_ui->kcfg_GlitchAnomalyEnabled->setEnabled(glitchOn);

        const bool anomalyOn = glitchOn && m_ui->kcfg_GlitchAnomalyEnabled->isChecked();
        m_ui->label_glitchFrequency->setEnabled(anomalyOn);
        m_ui->kcfg_GlitchFrequency->setEnabled(anomalyOn);
    };
    connect(m_ui->kcfg_GlitchEnabled, &QCheckBox::toggled, this, updateGlitchUI);
    connect(m_ui->kcfg_GlitchAnomalyEnabled, &QCheckBox::toggled, this, updateGlitchUI);
    updateGlitchUI();

    auto updateNeonUI = [this] {
        const bool enabled = m_ui->kcfg_NeonFlickerEnabled->isChecked();
        m_ui->label_neonInterval->setEnabled(enabled);
        m_ui->kcfg_NeonFlickerInterval->setEnabled(enabled);
        m_ui->label_neonDuration->setEnabled(enabled);
        m_ui->kcfg_NeonFlickerDuration->setEnabled(enabled);
        m_ui->label_neonIntensity->setEnabled(enabled);
        m_ui->kcfg_NeonFlickerIntensity->setEnabled(enabled);
    };
    connect(m_ui->kcfg_NeonFlickerEnabled, &QCheckBox::toggled, this, updateNeonUI);
    updateNeonUI();

    auto updateFlashUI = [this] {
        const bool enabled = m_ui->kcfg_FlashEnabled->isChecked();
        m_ui->label_flashInterval->setEnabled(enabled);
        m_ui->kcfg_FlashInterval->setEnabled(enabled);
        m_ui->label_flashDuration->setEnabled(enabled);
        m_ui->kcfg_FlashDuration->setEnabled(enabled);
    };
    connect(m_ui->kcfg_FlashEnabled, &QCheckBox::toggled, this, updateFlashUI);
    updateFlashUI();

    load();
}

void ActiveWindowOutlineConfig::addColorRow(const QColor &color, double holdTime)
{
    const int row = m_ui->tableColors->rowCount();
    m_ui->tableColors->insertRow(row);

    QPixmap iconPix(24, 24);
    iconPix.fill(color);
    auto *colorItem = new QTableWidgetItem(QIcon(iconPix), color.name(QColor::HexRgb));
    colorItem->setData(Qt::UserRole, color);
    colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable);
    m_ui->tableColors->setItem(row, 0, colorItem);

    auto *spin = new QDoubleSpinBox(m_ui->tableColors);
    spin->setRange(0.1, 60.0);
    spin->setSingleStep(0.5);
    spin->setValue(holdTime);
    spin->setSuffix(QStringLiteral(" s"));
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { markAsChanged(); });
    m_ui->tableColors->setCellWidget(row, 1, spin);
}

void ActiveWindowOutlineConfig::populateColorTable(const QString &serialized)
{
    m_ui->tableColors->setRowCount(0);
    const QStringList entries = serialized.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const QStringList parts = entry.split(QLatin1Char(':'));
        if (parts.size() >= 1) {
            const QColor color(parts[0].trimmed());
            double hold = 3.0;
            if (parts.size() >= 2) {
                bool ok = false;
                double val = parts[1].trimmed().toDouble(&ok);
                if (ok) {
                    hold = val;
                }
            }
            if (color.isValid()) {
                addColorRow(color, hold);
            }
        }
    }
    if (m_ui->tableColors->rowCount() == 0) {
        addColorRow(QColor(QStringLiteral("#3daee9")), 3.0);
        addColorRow(QColor(QStringLiteral("#2ecc71")), 3.0);
        addColorRow(QColor(QStringLiteral("#9b59b6")), 3.0);
    }
}

QString ActiveWindowOutlineConfig::serializeColorTable() const
{
    QStringList entries;
    const int count = m_ui->tableColors->rowCount();
    for (int i = 0; i < count; ++i) {
        auto *item = m_ui->tableColors->item(i, 0);
        if (!item) {
            continue;
        }
        const QColor color = item->data(Qt::UserRole).value<QColor>();
        double hold = 3.0;
        if (auto *spin = qobject_cast<QDoubleSpinBox *>(m_ui->tableColors->cellWidget(i, 1))) {
            hold = spin->value();
        }
        entries.append(QStringLiteral("%1:%2").arg(color.name(QColor::HexRgb)).arg(hold, 0, 'f', 1));
    }
    return entries.join(QLatin1Char(';'));
}

void ActiveWindowOutlineConfig::load()
{
    KCModule::load();
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-active-window-outline"));
    populateColorTable(group.readEntry(QStringLiteral("ColorList"), QStringLiteral("#3daee9:3.0;#2ecc71:3.0;#9b59b6:3.0")));
    setNeedsSave(false);
}

void ActiveWindowOutlineConfig::defaults()
{
    KCModule::defaults();
    populateColorTable(QStringLiteral("#3daee9:3.0;#2ecc71:3.0;#9b59b6:3.0"));
    markAsChanged();
}

void ActiveWindowOutlineConfig::save()
{
    KCModule::save();

    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-active-window-outline"));
    group.writeEntry(QStringLiteral("ColorList"), serializeColorTable());
    group.sync();

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("reconfigureEffect"));
    message << QStringLiteral("active-window-outline");
    QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
}

#include "activewindowoutline_config.moc"
