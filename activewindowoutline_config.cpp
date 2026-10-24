/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "activewindowoutline_config.h"
#include "ui_activewindowoutline_config.h"
#include "precedencehelper.h"

#include <KColorScheme>
#include <KConfigGroup>
#include <KConfigSkeleton>
#include <KGlobalAccel>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QClipboard>
#include <QColorDialog>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTextBrowser>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVersionNumber>
#include <cmath>

#ifndef ACTIVE_WINDOW_OUTLINE_VERSION
#define ACTIVE_WINDOW_OUTLINE_VERSION "development"
#endif

namespace {

QString formatColorHex(const QColor &color)
{
    return (color.alpha() < 255) ? color.name(QColor::HexArgb) : color.name(QColor::HexRgb);
}

QPixmap createColorIcon(const QColor &color)
{
    QPixmap iconPix(24, 24);
    if (color.alpha() < 255) {
        iconPix.fill(Qt::white);
        QPainter p(&iconPix);
        const QColor checkerGray(210, 210, 210);
        for (int y = 0; y < 24; y += 6) {
            for (int x = 0; x < 24; x += 6) {
                if (((x / 6) + (y / 6)) % 2 == 1) {
                    p.fillRect(x, y, 6, 6, checkerGray);
                }
            }
        }
        p.fillRect(0, 0, 24, 24, color);
        p.setPen(QColor(128, 128, 128, 120));
        p.drawRect(0, 0, 23, 23);
    } else {
        iconPix.fill(color);
        QPainter p(&iconPix);
        p.setPen(QColor(0, 0, 0, 40));
        p.drawRect(0, 0, 23, 23);
    }
    return iconPix;
}

struct AmbilightPreset {
    int mode;
    int opacity;
    double frequency;
    double duration;
    int saturationLeveling;
    int fallbackStrength;
    int minBrightness;
    int maxBrightness;
    bool ensureContrast;
};

// Preset index 0 is "Custom"
static const AmbilightPreset s_ambilightPresets[] = {
    { -1,  -1, -1.0, -1.0, -1, -1, -1, -1, false }, // 0: Custom
    {  0, 100,  1.5,  0.5, 65, 50, 25, 85,  true }, // 1: Balanced (Default)
    {  1, 100,  3.0,  0.25, 80, 50, 30, 95,  true }, // 2: Vivid & Dynamic
    {  2,  75,  1.0,  1.0, 45, 50, 20, 75,  true }, // 3: Subtle & Calm
    {  0, 100,  0.5,  1.0, 65, 50, 25, 85,  true }, // 4: Eco (Battery Saver)
    {  0, 100,  2.0,  0.4, 75, 90, 20, 92,  true }, // 5: High Contrast
    {  2, 100,  6.0,  0.15, 75, 50, 15, 90, false }, // 6: True Ambilight (Video / Cinema)
};

bool matchesPreset(Ui::ActiveWindowOutlineConfigForm *ui, const AmbilightPreset &p)
{
    if (ui->kcfg_AmbilightMode->currentIndex() != p.mode) return false;
    if (ui->kcfg_AmbilightOpacity->value() != p.opacity) return false;
    if (std::abs(ui->kcfg_AmbilightFrequency->value() - p.frequency) > 0.05) return false;
    if (std::abs(ui->kcfg_AmbilightDuration->value() - p.duration) > 0.05) return false;
    if (ui->kcfg_AmbilightSaturationLeveling->value() != p.saturationLeveling) return false;
    if (p.mode == 0 && ui->kcfg_AmbilightFallbackStrength->value() != p.fallbackStrength) return false;
    if (ui->kcfg_AmbilightMinBrightness->value() != p.minBrightness) return false;
    if (ui->kcfg_AmbilightMaxBrightness->value() != p.maxBrightness) return false;
    if (ui->kcfg_EnsurePerimeterContrast->isChecked() != p.ensureContrast) return false;
    return true;
}

} // namespace

K_PLUGIN_CLASS(ActiveWindowOutlineConfig)

ActiveWindowOutlineConfig::ActiveWindowOutlineConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
    , m_ui(new Ui::ActiveWindowOutlineConfigForm)
{
    m_ui->setupUi(widget());
    m_ui->label_versionValue->setText(QStringLiteral(ACTIVE_WINDOW_OUTLINE_VERSION));

    m_ui->btnCheckUpdate->setIcon(QIcon::fromTheme(QStringLiteral("system-software-update"),
                                                  QIcon::fromTheme(QStringLiteral("view-refresh"))));
    connect(m_ui->btnCheckUpdate, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::checkForUpdates);

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
    m_configSkeleton->addItemBool(QStringLiteral("EnsurePerimeterContrast"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("AmbilightEnabled"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("AmbilightMode"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("AmbilightOpacity"), *new int, 100);
    m_configSkeleton->addItemDouble(QStringLiteral("AmbilightFrequency"), *new double, 1.5);
    m_configSkeleton->addItemDouble(QStringLiteral("AmbilightDuration"), *new double, 0.5);
    m_configSkeleton->addItemInt(QStringLiteral("AmbilightSaturationLeveling"), *new int, 65);
    m_configSkeleton->addItemInt(QStringLiteral("AmbilightFallbackStrength"), *new int, 50);
    m_configSkeleton->addItemInt(QStringLiteral("AmbilightMinBrightness"), *new int, 25);
    m_configSkeleton->addItemInt(QStringLiteral("AmbilightMaxBrightness"), *new int, 85);
    m_configSkeleton->addItemBool(QStringLiteral("AmbilightEnsureContrast"), *new bool, true);
    m_configSkeleton->addItemInt(QStringLiteral("BorderWidth"), *new int, 2);
    m_configSkeleton->addItemInt(QStringLiteral("OutlineGap"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BorderRadius"), *new int, 6);
    m_configSkeleton->addItemBool(QStringLiteral("FadeCorners"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("DrawInside"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("SmoothEdges"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("GlowEnabled"), *new bool, true);
    m_configSkeleton->addItemInt(QStringLiteral("GlowRadius"), *new int, 6);
    m_configSkeleton->addItemBool(QStringLiteral("HideOnFullScreen"), *new bool, true);

    // Tab 4: Behavior & Buffer Delays
    m_configSkeleton->addItemInt(QStringLiteral("FadeDuration"), *new int, 200);
    m_configSkeleton->addItemInt(QStringLiteral("MenuHideDelay"), *new int, 300);
    m_configSkeleton->addItemInt(QStringLiteral("MenuShowDelay"), *new int, 300);
    m_configSkeleton->addItemInt(QStringLiteral("WindowHideDelay"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("WindowShowDelay"), *new int, 0);
    m_configSkeleton->addItemBool(QStringLiteral("HighlightRecentWindow"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("RecentWindowsCount"), *new int, 1);
    m_configSkeleton->addItemInt(QStringLiteral("RecentWindowColorMode"), *new int, 0);
    m_configSkeleton->addItemColor(QStringLiteral("RecentWindowCustomColor"), *new QColor, QColor(127, 140, 141));
    m_configSkeleton->addItemInt(QStringLiteral("RecentWindowOpacity"), *new int, 20);
    m_configSkeleton->addItemInt(QStringLiteral("RecentWindowHueShift"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("RecentWindowSaturation"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("RecentWindowWidthReduction"), *new int, 0);
    m_configSkeleton->addItemBool(QStringLiteral("RecentWindowDashed"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("RecentWindowGapGrowth"), *new int, 6);
    m_configSkeleton->addItemBool(QStringLiteral("RecentWindowDashSmooth"), *new bool, false);
    m_configSkeleton->addItemColor(QStringLiteral("MarkDefaultColor"), *new QColor, QColor(QStringLiteral("#e74c3c")));
    m_configSkeleton->addItemString(QStringLiteral("StylePrecedence"), *new QString, QStringLiteral("Active,Marked,Recent"));
    m_configSkeleton->addItemInt(QStringLiteral("MarkInactiveWidthReduction"), *new int, 0);
    m_configSkeleton->addItemBool(QStringLiteral("MarkInactiveDashed"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("MarkInactiveDashGap"), *new int, 6);
    m_configSkeleton->addItemBool(QStringLiteral("MarkInactiveDashSmooth"), *new bool, false);
    m_configSkeleton->addItemBool(QStringLiteral("MarkShowLetterBadges"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("BadgesAlwaysVisible"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeDisplayMode"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgePosition"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeSize"), *new int, 28);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeMargin"), *new int, 12);
    m_configSkeleton->addItemInt(QStringLiteral("JumpTargets"), *new int, 0);

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
        const QColor color = QColorDialog::getColor(QColor(61, 174, 233), widget(), QStringLiteral("Select Color"), QColorDialog::ShowAlphaChannel);
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
                const QColor color = QColorDialog::getColor(initialColor, widget(), QStringLiteral("Edit Color"), QColorDialog::ShowAlphaChannel);
                if (color.isValid()) {
                    item->setIcon(QIcon(createColorIcon(color)));
                    item->setText(formatColorHex(color));
                    item->setData(Qt::UserRole, color);
                    markAsChanged();
                }
            }
        }
    });

    // Color UI enable/disable sync
    auto updateColorEnabled = [this] {
        const bool colorCycle = m_ui->kcfg_ColorCycleEnabled->isChecked();
        const bool ambilight = m_ui->kcfg_AmbilightEnabled->isChecked();

        // When Color Sequence is active, color, custom color and ambilight are disabled
        m_ui->label_accentColor->setEnabled(!colorCycle);
        m_ui->kcfg_UseAccentColor->setEnabled(!colorCycle);

        const bool allowAccentDetails = !colorCycle && !ambilight;
        const bool useAccent = m_ui->kcfg_UseAccentColor->isChecked() && allowAccentDetails;

        m_ui->label_accentOpacity->setEnabled(useAccent);
        m_ui->kcfg_AccentOpacity->setEnabled(useAccent);

        m_ui->label_customColor->setEnabled(!useAccent && allowAccentDetails);
        m_ui->kcfg_CustomColor->setEnabled(!useAccent && allowAccentDetails);

        // Ambilight group is disabled if Color Sequence is active
        m_ui->groupAmbilight->setEnabled(!colorCycle);

        const bool allowAmbilightDetails = !colorCycle && ambilight;
        const bool isSmartMode = (m_ui->kcfg_AmbilightMode->currentIndex() == 0);
        m_ui->label_ambilightPreset->setEnabled(allowAmbilightDetails);
        m_ui->comboAmbilightPreset->setEnabled(allowAmbilightDetails);
        m_ui->btnAmbilightHelp->setEnabled(!colorCycle);
        m_ui->label_ambilightMode->setEnabled(allowAmbilightDetails);
        m_ui->kcfg_AmbilightMode->setEnabled(allowAmbilightDetails);
        m_ui->label_ambilightOpacity->setEnabled(allowAmbilightDetails);
        m_ui->kcfg_AmbilightOpacity->setEnabled(allowAmbilightDetails);
        m_ui->label_ambilightFrequency->setEnabled(allowAmbilightDetails);
        m_ui->kcfg_AmbilightFrequency->setEnabled(allowAmbilightDetails);
        m_ui->label_ambilightDuration->setEnabled(allowAmbilightDetails);
        m_ui->kcfg_AmbilightDuration->setEnabled(allowAmbilightDetails);
        m_ui->label_ambilightSaturationLeveling->setEnabled(allowAmbilightDetails);
        m_ui->kcfg_AmbilightSaturationLeveling->setEnabled(allowAmbilightDetails);
        m_ui->label_ambilightFallbackStrength->setEnabled(allowAmbilightDetails && isSmartMode);
        m_ui->kcfg_AmbilightFallbackStrength->setEnabled(allowAmbilightDetails && isSmartMode);
        m_ui->label_ambilightMinBrightness->setEnabled(allowAmbilightDetails);
        m_ui->kcfg_AmbilightMinBrightness->setEnabled(allowAmbilightDetails);
        m_ui->label_ambilightMaxBrightness->setEnabled(allowAmbilightDetails);
        m_ui->kcfg_AmbilightMaxBrightness->setEnabled(allowAmbilightDetails);
        m_ui->kcfg_EnsurePerimeterContrast->setEnabled(!colorCycle);
        m_ui->label_ambilightWarning->setEnabled(allowAmbilightDetails);
    };
    connect(m_ui->kcfg_EnsurePerimeterContrast, &QCheckBox::toggled, this, updateColorEnabled);
    connect(m_ui->kcfg_UseAccentColor, &QCheckBox::toggled, this, updateColorEnabled);
    connect(m_ui->kcfg_AmbilightEnabled, &QCheckBox::toggled, this, updateColorEnabled);
    connect(m_ui->kcfg_AmbilightMode, &QComboBox::currentIndexChanged, this, updateColorEnabled);
    connect(m_ui->kcfg_ColorCycleEnabled, &QCheckBox::toggled, this, updateColorEnabled);
    updateColorEnabled();

    const auto updateCornersEnabled = [this]() {
        const bool fadeCorners = m_ui->kcfg_FadeCorners->isChecked();
        m_ui->label_borderRadius->setEnabled(!fadeCorners);
        m_ui->kcfg_BorderRadius->setEnabled(!fadeCorners);
    };
    connect(m_ui->kcfg_FadeCorners, &QCheckBox::toggled, this, updateCornersEnabled);
    updateCornersEnabled();

    const QIcon helpIcon = QIcon::fromTheme(QStringLiteral("help-contextual"),
                                            QIcon::fromTheme(QStringLiteral("dialog-information")));
    if (!helpIcon.isNull()) {
        m_ui->btnAmbilightHelp->setIcon(helpIcon);
    } else {
        m_ui->btnAmbilightHelp->setText(QStringLiteral("?"));
    }
    m_ui->btnAmbilightHelp->setFixedWidth(28);
    m_ui->btnAmbilightHelp->setCursor(Qt::PointingHandCursor);
    connect(m_ui->btnAmbilightHelp, &QToolButton::clicked, this, &ActiveWindowOutlineConfig::showAmbilightHelpDialog);

    connect(m_ui->comboAmbilightPreset, qOverload<int>(&QComboBox::activated), this, [this, updateColorEnabled](int index) {
        const int numPresets = static_cast<int>(sizeof(s_ambilightPresets) / sizeof(s_ambilightPresets[0]));
        if (index <= 0 || index >= numPresets) {
            return;
        }
        m_isApplyingPreset = true;
        const auto &p = s_ambilightPresets[index];
        m_ui->kcfg_AmbilightMode->setCurrentIndex(p.mode);
        m_ui->kcfg_AmbilightOpacity->setValue(p.opacity);
        m_ui->kcfg_AmbilightFrequency->setValue(p.frequency);
        m_ui->kcfg_AmbilightDuration->setValue(p.duration);
        m_ui->kcfg_AmbilightSaturationLeveling->setValue(p.saturationLeveling);
        m_ui->kcfg_AmbilightFallbackStrength->setValue(p.fallbackStrength);
        m_ui->kcfg_AmbilightMinBrightness->setValue(p.minBrightness);
        m_ui->kcfg_AmbilightMaxBrightness->setValue(p.maxBrightness);
        m_ui->kcfg_EnsurePerimeterContrast->setChecked(p.ensureContrast);
        m_isApplyingPreset = false;
        updateColorEnabled();
        markAsChanged();
    });

    auto onAmbilightValueChanged = [this] {
        if (!m_isApplyingPreset) {
            updatePresetSelector();
        }
    };
    connect(m_ui->kcfg_AmbilightMode, &QComboBox::currentIndexChanged, this, onAmbilightValueChanged);
    connect(m_ui->kcfg_AmbilightOpacity, &QSpinBox::valueChanged, this, onAmbilightValueChanged);
    connect(m_ui->kcfg_AmbilightFrequency, &QDoubleSpinBox::valueChanged, this, onAmbilightValueChanged);
    connect(m_ui->kcfg_AmbilightDuration, &QDoubleSpinBox::valueChanged, this, onAmbilightValueChanged);
    connect(m_ui->kcfg_AmbilightSaturationLeveling, &QSpinBox::valueChanged, this, onAmbilightValueChanged);
    connect(m_ui->kcfg_AmbilightFallbackStrength, &QSpinBox::valueChanged, this, onAmbilightValueChanged);
    connect(m_ui->kcfg_AmbilightMinBrightness, &QSpinBox::valueChanged, this, onAmbilightValueChanged);
    connect(m_ui->kcfg_AmbilightMaxBrightness, &QSpinBox::valueChanged, this, onAmbilightValueChanged);
    connect(m_ui->kcfg_EnsurePerimeterContrast, &QCheckBox::toggled, this, onAmbilightValueChanged);

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

    connect(m_ui->kcfg_HighlightRecentWindow, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateRecentWindowUI);
    connect(m_ui->kcfg_RecentWindowColorMode, qOverload<int>(&QComboBox::currentIndexChanged), this, &ActiveWindowOutlineConfig::updateRecentWindowUI);
    connect(m_ui->kcfg_RecentWindowDashed, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateRecentWindowUI);
    updateRecentWindowUI();

    connect(m_ui->kcfg_MarkInactiveDashed, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    connect(m_ui->kcfg_BadgeDisplayMode, qOverload<int>(&QComboBox::currentIndexChanged), this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    updateMarkWindowUI();

    QString shortcutStr = QStringLiteral("Meta+Alt+Space");
    const auto shortcuts = KGlobalAccel::self()->globalShortcut(QStringLiteral("kwin"), QStringLiteral("JumpToWindow"));
    if (!shortcuts.isEmpty() && !shortcuts.first().isEmpty()) {
        shortcutStr = shortcuts.first().toString(QKeySequence::NativeText);
    }
    m_ui->label_jumpTargets->setText(tr("Jump targets (default: %1):").arg(shortcutStr));
    m_ui->label_jumpTargets->setToolTip(tr("Scope of windows included when pressing the Jump to Window shortcut (%1).").arg(shortcutStr));
    m_ui->kcfg_JumpTargets->setToolTip(tr("Scope of windows included when pressing the Jump to Window shortcut (%1).").arg(shortcutStr));

    connect(m_ui->btnPrecedenceUp, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::movePrecedenceUp);
    connect(m_ui->btnPrecedenceDown, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::movePrecedenceDown);
    connect(m_ui->btnPrecedenceReset, &QPushButton::clicked, this, [this] {
        populatePrecedenceList(QStringLiteral("Active,Marked,Recent"));
        markAsChanged();
    });
    connect(m_ui->listPrecedence, &QListWidget::currentRowChanged, this, [this] {
        updatePrecedenceButtons();
    });
    connect(m_ui->listPrecedence->model(), &QAbstractItemModel::rowsMoved, this, [this] {
        updatePrecedenceButtons();
        updatePrecedenceExplanation();
        markAsChanged();
    });

    load();
}

void ActiveWindowOutlineConfig::updateRecentWindowUI()
{
    const bool enabled = m_ui->kcfg_HighlightRecentWindow->isChecked();
    m_ui->label_recentWindowsCount->setEnabled(enabled);
    m_ui->kcfg_RecentWindowsCount->setEnabled(enabled);
    m_ui->label_recentWindowColorMode->setEnabled(enabled);
    m_ui->kcfg_RecentWindowColorMode->setEnabled(enabled);
    const bool isCustom = enabled && (m_ui->kcfg_RecentWindowColorMode->currentIndex() == 1);
    m_ui->label_recentWindowCustomColor->setEnabled(isCustom);
    m_ui->kcfg_RecentWindowCustomColor->setEnabled(isCustom);
    m_ui->label_recentWindowOpacity->setEnabled(enabled);
    m_ui->kcfg_RecentWindowOpacity->setEnabled(enabled);
    m_ui->label_recentWindowHueShift->setEnabled(enabled);
    m_ui->kcfg_RecentWindowHueShift->setEnabled(enabled);
    m_ui->label_recentWindowSaturation->setEnabled(enabled);
    m_ui->kcfg_RecentWindowSaturation->setEnabled(enabled);
    m_ui->label_recentWindowWidthReduction->setEnabled(enabled);
    m_ui->kcfg_RecentWindowWidthReduction->setEnabled(enabled);
    m_ui->kcfg_RecentWindowDashed->setEnabled(enabled);
    const bool dashed = enabled && m_ui->kcfg_RecentWindowDashed->isChecked();
    m_ui->label_recentWindowGapGrowth->setEnabled(dashed);
    m_ui->kcfg_RecentWindowGapGrowth->setEnabled(dashed);
    m_ui->kcfg_RecentWindowDashSmooth->setEnabled(dashed);
}

void ActiveWindowOutlineConfig::updateMarkWindowUI()
{
    const bool dashed = m_ui->kcfg_MarkInactiveDashed->isChecked();
    m_ui->label_markInactiveDashGap->setEnabled(dashed);
    m_ui->kcfg_MarkInactiveDashGap->setEnabled(dashed);
    m_ui->kcfg_MarkInactiveDashSmooth->setEnabled(dashed);

    const bool badgesEnabled = (m_ui->kcfg_BadgeDisplayMode->currentIndex() != 2);
    m_ui->label_badgePosition->setEnabled(badgesEnabled);
    m_ui->kcfg_BadgePosition->setEnabled(badgesEnabled);
    m_ui->label_badgeSize->setEnabled(badgesEnabled);
    m_ui->kcfg_BadgeSize->setEnabled(badgesEnabled);
    m_ui->label_badgeMargin->setEnabled(badgesEnabled);
    m_ui->kcfg_BadgeMargin->setEnabled(badgesEnabled);
    m_ui->label_jumpTargets->setEnabled(badgesEnabled);
    m_ui->kcfg_JumpTargets->setEnabled(badgesEnabled);
}

void ActiveWindowOutlineConfig::populatePrecedenceList(const QString &serialized)
{
    m_ui->listPrecedence->clear();
    const QStringList keys = OutlinePrecedence::parsePrecedence(serialized);

    for (const QString &key : keys) {
        if (key.compare(QStringLiteral("Active"), Qt::CaseInsensitive) == 0) {
            auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("preferences-system-windows")),
                                             tr("Active Window (focused window, active color & animations)"));
            item->setData(Qt::UserRole, QStringLiteral("Active"));
            m_ui->listPrecedence->addItem(item);
        } else if (key.compare(QStringLiteral("Marked"), Qt::CaseInsensitive) == 0) {
            auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("bookmarks")),
                                             tr("Marked Windows (pinned window with custom mark color & style)"));
            item->setData(Qt::UserRole, QStringLiteral("Marked"));
            m_ui->listPrecedence->addItem(item);
        } else if (key.compare(QStringLiteral("Recent"), Qt::CaseInsensitive) == 0) {
            auto *item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("document-open-recent")),
                                             tr("Recent Windows (previously focused history)"));
            item->setData(Qt::UserRole, QStringLiteral("Recent"));
            m_ui->listPrecedence->addItem(item);
        }
    }
    if (m_ui->listPrecedence->count() > 0) {
        m_ui->listPrecedence->setCurrentRow(0);
    }
    updatePrecedenceButtons();
    updatePrecedenceExplanation();
}

QString ActiveWindowOutlineConfig::serializePrecedenceList() const
{
    QStringList keys;
    for (int i = 0; i < m_ui->listPrecedence->count(); ++i) {
        keys.append(m_ui->listPrecedence->item(i)->data(Qt::UserRole).toString());
    }
    return keys.join(QLatin1Char(','));
}

void ActiveWindowOutlineConfig::updatePrecedenceButtons()
{
    const int row = m_ui->listPrecedence->currentRow();
    const int count = m_ui->listPrecedence->count();
    m_ui->btnPrecedenceUp->setEnabled(row > 0);
    m_ui->btnPrecedenceDown->setEnabled(row >= 0 && row < count - 1);
}

void ActiveWindowOutlineConfig::updatePrecedenceExplanation()
{
    QStringList order;
    for (int i = 0; i < m_ui->listPrecedence->count(); ++i) {
        order.append(m_ui->listPrecedence->item(i)->data(Qt::UserRole).toString());
    }
    const int rankActive = order.indexOf(QStringLiteral("Active"));
    const int rankMarked = order.indexOf(QStringLiteral("Marked"));
    const int rankRecent = order.indexOf(QStringLiteral("Recent"));

    QString explanation;
    explanation += QStringLiteral("<b>Priority order:</b> ");
    QStringList names;
    for (int i = 0; i < order.size(); ++i) {
        names.append(QStringLiteral("%1. %2").arg(i + 1).arg(order.at(i)));
    }
    explanation += names.join(QStringLiteral(" &gt; "));
    explanation += QStringLiteral("<br><br>");

    if (rankActive >= 0 && rankMarked >= 0) {
        if (rankActive < rankMarked) {
            explanation += tr("• <b>Focused marked window</b>: <i>Active style wins</i> — uses full active outline appearance (active color/ambilight/cycle, border width, and shader animations).<br>");
        } else {
            explanation += tr("• <b>Focused marked window</b>: <i>Marked style wins</i> — keeps its custom mark color and style without being overridden by active color or active shaders.<br>");
        }
    }

    if (rankMarked >= 0 && rankRecent >= 0) {
        if (rankMarked < rankRecent) {
            explanation += tr("• <b>Inactive marked window in recent history</b>: <i>Marked style wins</i> — retains its custom mark color and style, ignoring recent decay (saturation drop, hue shift, gap growth, width reduction).<br>");
        } else {
            explanation += tr("• <b>Inactive marked window in recent history</b>: <i>Recent style wins</i> — participates in recent window decay (takes recent color, opacity decay, width reduction, and dash gap growth).<br>");
        }
    }

    explanation += tr("<br><i>Note: Active and Recent are mutually exclusive because a window cannot be focused and inactive simultaneously.</i>");

    m_ui->label_precedenceExplanation->setText(explanation);
}

void ActiveWindowOutlineConfig::movePrecedenceUp()
{
    int row = m_ui->listPrecedence->currentRow();
    if (row <= 0) return;
    QListWidgetItem *item = m_ui->listPrecedence->takeItem(row);
    m_ui->listPrecedence->insertItem(row - 1, item);
    m_ui->listPrecedence->setCurrentRow(row - 1);
    updatePrecedenceButtons();
    updatePrecedenceExplanation();
    markAsChanged();
}

void ActiveWindowOutlineConfig::movePrecedenceDown()
{
    int row = m_ui->listPrecedence->currentRow();
    if (row < 0 || row >= m_ui->listPrecedence->count() - 1) return;
    QListWidgetItem *item = m_ui->listPrecedence->takeItem(row);
    m_ui->listPrecedence->insertItem(row + 1, item);
    m_ui->listPrecedence->setCurrentRow(row + 1);
    updatePrecedenceButtons();
    updatePrecedenceExplanation();
    markAsChanged();
}

void ActiveWindowOutlineConfig::addColorRow(const QColor &color, double holdTime)
{
    const int row = m_ui->tableColors->rowCount();
    m_ui->tableColors->insertRow(row);

    auto *colorItem = new QTableWidgetItem(QIcon(createColorIcon(color)), formatColorHex(color));
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
        entries.append(QStringLiteral("%1:%2").arg(formatColorHex(color)).arg(hold, 0, 'f', 1));
    }
    return entries.join(QLatin1Char(';'));
}

void ActiveWindowOutlineConfig::updatePresetSelector()
{
    if (m_isApplyingPreset) {
        return;
    }
    int matchedIndex = 0; // 0 is Custom
    const int numPresets = static_cast<int>(sizeof(s_ambilightPresets) / sizeof(s_ambilightPresets[0]));
    for (int i = 1; i < numPresets; ++i) {
        if (matchesPreset(m_ui, s_ambilightPresets[i])) {
            matchedIndex = i;
            break;
        }
    }
    const QSignalBlocker blocker(m_ui->comboAmbilightPreset);
    m_ui->comboAmbilightPreset->setCurrentIndex(matchedIndex);
}

void ActiveWindowOutlineConfig::showAmbilightHelpDialog()
{
    const bool isPolish = (QLocale().language() == QLocale::Polish);

    const QString title = isPolish
        ? QStringLiteral("Przewodnik po parametrach i profilach Ambilight")
        : QStringLiteral("Ambilight Parameters & Presets Guide");

    const QString html = isPolish ? QStringLiteral(R"raw(
<!DOCTYPE html>
<html>
<head>
<style>
  body { font-size: 10pt; line-height: 1.45; }
  h3 { margin-top: 0px; margin-bottom: 8px; color: palette(highlight); }
  h4 { margin-top: 14px; margin-bottom: 6px; }
  p { margin: 6px 0; }
  ul { margin-top: 4px; margin-bottom: 8px; padding-left: 22px; }
  li { margin-bottom: 6px; }
  hr { border: 0; height: 1px; background: rgba(128, 128, 128, 0.35); margin: 12px 0; }
  b { font-weight: bold; }
  i { font-style: italic; }
</style>
</head>
<body>
<h3>Ambilight &ndash; Adaptacyjny kolor ramki okna</h3>
<p>Funkcja <b>Ambilight</b> w czasie rzeczywistym analizuje kolorystykę zawartości aktywnego okna i płynnie animuje barwę ramki, dopasowując ją do aktualnie wyświetlanej treści.</p>

<hr>
<h4>Gotowe profile (Presety)</h4>
<ul>
  <li><b>Zrównoważony / Domyślny (Balanced):</b> Rekomendowany do codziennego użytku na pulpicie. Używa inteligentnego trybu <i>Smart Adaptive</i> z częstotliwością 1.5&nbsp;Hz, płynnym przejściem 0.5&nbsp;s i natychmiastową reakcją.</li>
  <li><b>Jaskrawy i dynamiczny (Vivid &amp; Dynamic):</b> Zwiększona częstotliwość 3.0&nbsp;Hz, agresywne śledzenie akcentów w oknie, szybkie przejście 0.25&nbsp;s i mocniejsze nasycenie barw. Idealny do gier, dynamicznych stron i teledysków.</li>
  <li><b>Subtelny i stonowany (Subtle &amp; Calm):</b> Uśrednione barwy całego okna, stonowane nasycenie, obniżona przezroczystość (75%), spokojna częstotliwość (1.0&nbsp;Hz) i bardzo długie, łagodne przenikanie barw (1.0&nbsp;s).</li>
  <li><b>Oszczędzanie baterii (Eco):</b> Zoptymalizowany pod kątem pracy na baterii w laptopach. Próbkowanie raz na 2 sekundy (0.5&nbsp;Hz) z płynnym przejściem 1.0&nbsp;s. Zmiana kart i stron WWW wyzwala natychmiastowe odświeżenie barwy bez czekania.</li>
  <li><b>Wysoki kontrast (High Contrast):</b> Maksymalna czytelność i odcięcie obrysu. Częstotliwość 2.0&nbsp;Hz, przejście 0.4&nbsp;s, silne odróżnienie fallbacku (90%) i wysoka saturacja gwarantują, że ramka zawsze wyraźnie odetnie się od tła okna.</li>
  <li><b>True Ambilight (Film / Wideo):</b> Stworzony specjalnie do oglądania filmów, wideo i streamów w oknie. Błyskawiczne próbkowanie 6.0&nbsp;Hz (co 166&nbsp;ms) z płynnym, szybkim przejściem 0.15&nbsp;s natychmiast dopasowuje barwę ramki do zmian ujęć w wideo bez zauważalnego opóźnienia i bez męczącego stroboskopu. Wykorzystuje addytywne uśrednianie barw (<i>Balanced average</i>), ignorując czarne pasy kinowe i odzwierciedlając ogólne światło sceny, z poszerzoną dynamiką jasności (15&ndash;90%).</li>
  <li><b>Własny (Custom):</b> Wybiera się automatycznie, gdy ręcznie zmodyfikujesz którykolwiek z suwaków.</li>
</ul>

<hr>
<h4>Szczegółowe wyjaśnienie parametrów</h4>
<ul>
  <li><b>Tryb wykrywania (Detection mode):</b>
    <ul>
      <li><i>Smart Adaptive (Hybrydowy):</i> Domyślny i najbardziej zaawansowany tryb. Analizuje jednocześnie kolor dominujący oraz średnią całego okna. Jeśli kolor dominujący zlewa się z tłem okna (np. jednolicie ciemne lub jasne okno programu), ramka automatycznie podbija kontrast i nasycenie, aby pozostać czytelna.</li>
      <li><i>Dominant accent:</i> Agresywnie blokuje się na najbardziej nasyconym, żywym kolorze w oknie (np. czerwony pasek YouTube, logo programu, miniaturka), stosując 15% histerezę chroniącą przed drganiem barw.</li>
      <li><i>Balanced average:</i> Miesza addytywnie barwy wszystkich elementów okna w jeden spokojny, spójny odcień kompozytowy.</li>
    </ul>
  </li>
  <li><b>Przezroczystość (Opacity):</b> Poziom krycia ramki Ambilight (0% to całkowita niewidoczność, 100% to pełne, jednolite krycie).</li>
  <li><b>Częstotliwość próbkowania (Sampling frequency):</b> Jak często (w hercach, czyli ile razy na sekundę) analizowane są barwy okna. Wartość 1.5&nbsp;Hz jest optymalna do pracy na pulpicie i stron WWW, a wyższe wartości (np. 3.0&ndash;6.0&nbsp;Hz) zapewniają błyskawiczną reakcję na filmy i dynamiczne wideo bez zauważalnego opóźnienia. Nawigacja po stronach WWW i przełączanie zakładek wyzwalają odświeżenie natychmiast.</li>
  <li><b>Czas przejścia (Transition duration):</b> Czas trwania animacji (w sekundach) przy płynnym przenikaniu starego koloru ramki w nowo wykryty kolor.</li>
  <li><b>Wygładzenie saturacji (Saturation leveling):</b> Inteligentna normalizacja nasycenia barwy. Łagodnie desaturuje/uspokaja zbyt krzykliwe neony, a blade i pastelowe kolory podciąga w stronę wybranego poziomu docelowego. Czyste szarości i czerń pozostają neutralne.</li>
  <li><b>Odróżnienie fallbacku (Fallback distinction):</b> <i>(Dostępne tylko w trybie Smart Adaptive)</i> Decyduje, jak mocno ramka ma zmodyfikować swój kolor i odskoczyć od tła okna, gdy dominujący kolor zlewa się ze średnią okna (0% = subtelnie/brak zmiany, 100% = mocny kontrast i wyskok barwy).</li>
  <li><b>Minimalna jasność (Minimum brightness):</b> Zabezpieczenie przed zbyt ciemną ramką na czarnych oknach i ciemnych motywach (nie pozwala jasności ramki spaść poniżej tego progu).</li>
  <li><b>Maksymalna jasność (Maximum brightness):</b> Zabezpieczenie przed oślepiającą bielą ramki na bardzo jasnych stronach internetowych i dokumentach (nie pozwala jasności ramki przekroczyć tego progu).</li>
  <li><b>Zapewnij kontrast krawędzi (Ensure perimeter contrast):</b> Analizuje obwód i krawędzie okna (m.in. pasek tytułowy/karty i obramowanie) w miejscu, gdzie faktycznie rysowany jest obrys. Jeśli tło pod ramką jest ciemne lub okno ma jednolity kolor (np. zielone okno przy zielonej ramce), algorytm dynamicznie koryguje jasność i saturację ramki, gwarantując jej stałą widoczność i odcięcie od krawędzi okna.</li>
</ul>
</body>
</html>
)raw") : QStringLiteral(R"raw(
<!DOCTYPE html>
<html>
<head>
<style>
  body { font-size: 10pt; line-height: 1.45; }
  h3 { margin-top: 0px; margin-bottom: 8px; color: palette(highlight); }
  h4 { margin-top: 14px; margin-bottom: 6px; }
  p { margin: 6px 0; }
  ul { margin-top: 4px; margin-bottom: 8px; padding-left: 22px; }
  li { margin-bottom: 6px; }
  hr { border: 0; height: 1px; background: rgba(128, 128, 128, 0.35); margin: 12px 0; }
  b { font-weight: bold; }
  i { font-style: italic; }
</style>
</head>
<body>
<h3>Ambilight &ndash; Adaptive Window Outline</h3>
<p><b>Ambilight</b> dynamically analyzes active window contents in real time and smoothly animates the outline color to match the displayed content.</p>

<hr>
<h4>Built-in Presets</h4>
<ul>
  <li><b>Balanced (Default):</b> Recommended for everyday desktop work. Uses intelligent <i>Smart Adaptive</i> mode with 1.5&nbsp;Hz sampling, smooth 0.5&nbsp;s transitions, and instant response.</li>
  <li><b>Vivid &amp; Dynamic:</b> Elevated 3.0&nbsp;Hz sampling, assertive accent locking, snappy 0.25&nbsp;s transitions, and boosted saturation. Perfect for gaming, dynamic web apps, and music clips.</li>
  <li><b>Subtle &amp; Calm:</b> Balanced composite color blending across the whole window, reduced opacity (75%), relaxed 1.0&nbsp;Hz sampling, and slow, gentle crossfades (1.0&nbsp;s).</li>
  <li><b>Eco (Battery Saver):</b> Maximizes laptop battery life when away from a charger. Samples once every 2 seconds (0.5&nbsp;Hz) with smooth 1.0&nbsp;s crossfading. Tab switching and page navigations still trigger instant updates.</li>
  <li><b>High Contrast:</b> Maximum visibility and outline differentiation. 2.0&nbsp;Hz sampling, snappy 0.4&nbsp;s transitions, high fallback pop (90%) and strong saturation leveling ensure the outline always stands out clearly.</li>
  <li><b>True Ambilight (Video / Cinema):</b> Tailored specifically for watching movies, videos, and live streams in a window. Ultra-responsive 6.0&nbsp;Hz sampling (every 166&nbsp;ms) combined with snappy 0.15&nbsp;s crossfading follows scene cuts in real-time with minimal latency and zero eye fatigue or strobing. Uses <i>Balanced average</i> additive mixing to reflect atmospheric scene lighting while ignoring black letterbox bars, with an expanded dynamic brightness range (15&ndash;90%).</li>
  <li><b>Custom:</b> Automatically selected whenever individual parameters are adjusted manually.</li>
</ul>

<hr>
<h4>Detailed Parameter Explanations</h4>
<ul>
  <li><b>Detection mode:</b>
    <ul>
      <li><i>Smart Adaptive (Hybrid):</i> Default and most intelligent mode. Simultaneously analyzes dominant accent and average window tone. If the dominant accent matches the background canvas, it automatically shifts vibrance and contrast to prevent the outline from blending in.</li>
      <li><i>Dominant accent:</i> Decisively locks onto the most vibrant chromatic accent (e.g. YouTube red, Spotify green) with 15% inertia hysteresis to prevent flickering between competing colors.</li>
      <li><i>Balanced average:</i> Harmoniously merges all window colors using additive RGB light mixing into a calm, unified composite tone.</li>
    </ul>
  </li>
  <li><b>Opacity:</b> Transparency level of the Ambilight outline (0% is completely invisible, 100% is fully opaque).</li>
  <li><b>Sampling frequency:</b> How often (in Hertz / times per second) active window colors are analyzed. 1.5&nbsp;Hz is recommended for desktop work, while higher values (e.g. 3.0&ndash;6.0&nbsp;Hz) follow dynamic video content and movies in real time. Browser navigations and active window repaints trigger opportunistic updates immediately.</li>
  <li><b>Transition duration:</b> Animation duration (in seconds) when crossfading from the previous outline color into newly sampled colors.</li>
  <li><b>Saturation leveling:</b> Normalizes outline saturation towards a balanced target. Softens harsh neon colors while lifting pale pastels. Pure monochrome grays remain neutral.</li>
  <li><b>Fallback distinction:</b> <i>(Smart Adaptive mode only)</i> How strongly the outline is modified to stand out when window content matches the dominant accent (0% = subtle/unchanged, 100% = bold contrast and vibrance pop).</li>
  <li><b>Minimum brightness:</b> Prevents the outline from becoming too dark on black or dark-mode windows (clamps luminance from falling below this threshold).</li>
  <li><b>Maximum brightness:</b> Prevents the outline from being blindingly bright on pure white windows or bright documents (clamps luminance from exceeding this threshold).</li>
  <li><b>Ensure perimeter contrast:</b> Analyzes the window edges and perimeter (such as title bars, tab strips, and borders) directly beneath the outline. If the perimeter is dark, or if window colors match the outline (e.g. green outline on a green window), brightness and saturation are dynamically adjusted to guarantee crisp separation and high visibility.</li>
</ul>
</body>
</html>
)raw");

    QDialog dialog(widget());
    dialog.setWindowTitle(title);
    dialog.resize(620, 560);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto *viewer = new QTextBrowser(&dialog);
    viewer->setOpenExternalLinks(false);
    viewer->setHtml(html);
    layout->addWidget(viewer);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::accept);
    layout->addWidget(buttonBox);

    dialog.exec();
}

void ActiveWindowOutlineConfig::load()
{
    KCModule::load();
    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-active-window-outline"));
    const bool ensureContrast = group.readEntry("EnsurePerimeterContrast", group.readEntry("AmbilightEnsureContrast", true));
    m_ui->kcfg_EnsurePerimeterContrast->setChecked(ensureContrast);
    populateColorTable(group.readEntry(QStringLiteral("ColorList"), QStringLiteral("#3daee9:3.0;#2ecc71:3.0;#9b59b6:3.0")));
    populatePrecedenceList(group.readEntry(QStringLiteral("StylePrecedence"), QStringLiteral("Active,Marked,Recent")));
    updatePresetSelector();
    const bool fadeCorners = m_ui->kcfg_FadeCorners->isChecked();
    m_ui->label_borderRadius->setEnabled(!fadeCorners);
    m_ui->kcfg_BorderRadius->setEnabled(!fadeCorners);
    if (!group.hasKey(QStringLiteral("BadgeDisplayMode"))) {
        if (!group.readEntry("MarkShowLetterBadges", true)) {
            m_ui->kcfg_BadgeDisplayMode->setCurrentIndex(2);
        } else if (group.readEntry("BadgesAlwaysVisible", false)) {
            m_ui->kcfg_BadgeDisplayMode->setCurrentIndex(1);
        } else {
            m_ui->kcfg_BadgeDisplayMode->setCurrentIndex(0);
        }
    }
    updateRecentWindowUI();
    updateMarkWindowUI();
    setNeedsSave(false);
}

void ActiveWindowOutlineConfig::defaults()
{
    KCModule::defaults();
    populateColorTable(QStringLiteral("#3daee9:3.0;#2ecc71:3.0;#9b59b6:3.0"));
    populatePrecedenceList(QStringLiteral("Active,Marked,Recent"));
    updatePresetSelector();
    const bool fadeCorners = m_ui->kcfg_FadeCorners->isChecked();
    m_ui->label_borderRadius->setEnabled(!fadeCorners);
    m_ui->kcfg_BorderRadius->setEnabled(!fadeCorners);
    m_ui->kcfg_BadgeDisplayMode->setCurrentIndex(0);
    m_ui->kcfg_BadgePosition->setCurrentIndex(0);
    m_ui->kcfg_BadgeSize->setValue(28);
    m_ui->kcfg_BadgeMargin->setValue(12);
    updateRecentWindowUI();
    updateMarkWindowUI();
    markAsChanged();
}

void ActiveWindowOutlineConfig::checkForUpdates()
{
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
    }

    m_ui->btnCheckUpdate->setEnabled(false);
    m_ui->btnCheckUpdate->setText(QStringLiteral("Checking…"));
    m_ui->label_updateStatus->setText(QStringLiteral("Connecting to GitHub…"));
    m_ui->label_updateStatus->setStyleSheet(QString());

    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/gmalinowski/kwin-active-window-outline/releases")));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ActiveWindowOutline-KCM"));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_ui->btnCheckUpdate->setEnabled(true);
        m_ui->btnCheckUpdate->setText(QStringLiteral("Check for updates"));

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            m_ui->label_updateStatus->setText(QStringLiteral("⚠️ Error: %1").arg(err));
            m_ui->label_updateStatus->setStyleSheet(QStringLiteral("color: #e74c3c;"));
            return;
        }

        const QByteArray responseData = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
            m_ui->label_updateStatus->setText(QStringLiteral("⚠️ Invalid GitHub response"));
            m_ui->label_updateStatus->setStyleSheet(QStringLiteral("color: #e74c3c;"));
            return;
        }

        const QJsonArray releases = doc.array();
        if (releases.isEmpty()) {
            m_ui->label_updateStatus->setText(QStringLiteral("No releases found on GitHub"));
            return;
        }

        // The first release in the array is the newest chronological release
        const QJsonObject latestRelease = releases.first().toObject();
        const QString tagName = latestRelease.value(QStringLiteral("tag_name")).toString();
        const bool isPrerelease = latestRelease.value(QStringLiteral("prerelease")).toBool(false);
        const QString htmlUrl = latestRelease.value(QStringLiteral("html_url")).toString();
        const QString body = latestRelease.value(QStringLiteral("body")).toString();

        QString cleanTag = tagName;
        if (cleanTag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
            cleanTag.remove(0, 1);
        }

        const QString currentVerStr = QStringLiteral(ACTIVE_WINDOW_OUTLINE_VERSION);
        const QVersionNumber currentVer = QVersionNumber::fromString(currentVerStr);
        const QVersionNumber remoteVer = QVersionNumber::fromString(cleanTag);

        const bool isDev = currentVer.isNull();
        const bool hasUpdate = isDev || (remoteVer > currentVer);

        if (!hasUpdate) {
            m_ui->label_updateStatus->setText(QStringLiteral("✓ Up to date (v%1)").arg(currentVerStr));
            m_ui->label_updateStatus->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));

            QMessageBox::information(widget(),
                QStringLiteral("Plugin Updates"),
                QStringLiteral("You have the latest version of Active Window Outline installed (v%1).").arg(currentVerStr));
            return;
        }

        // New version available!
        m_ui->label_updateStatus->setText(QStringLiteral("⬆ New version: %1!").arg(tagName));
        m_ui->label_updateStatus->setStyleSheet(QStringLiteral("color: #3daee9; font-weight: bold;"));

        const QString installCmd = isPrerelease
            ? QStringLiteral("curl -sSL https://raw.githubusercontent.com/gmalinowski/kwin-active-window-outline/main/install.sh | bash -s -- --pre")
            : QStringLiteral("curl -sSL https://raw.githubusercontent.com/gmalinowski/kwin-active-window-outline/main/install.sh | bash");

        QDialog dlg(widget());
        dlg.setWindowTitle(QStringLiteral("New Version Available!"));
        dlg.resize(540, 440);

        auto *vbox = new QVBoxLayout(&dlg);
        vbox->setContentsMargins(14, 14, 14, 14);
        vbox->setSpacing(10);

        auto *info = new QLabel(&dlg);
        info->setTextFormat(Qt::RichText);
        info->setWordWrap(true);
        info->setText(QStringLiteral("<h3>🎉 A new version is available: <b>%1</b></h3>"
                                     "<p>Currently installed: <b>v%2</b><br>"
                                     "Latest release: <b>%1</b> %3</p>")
                        .arg(tagName)
                        .arg(currentVerStr)
                        .arg(isPrerelease ? QStringLiteral("(pre-release)") : QStringLiteral("(stable)")));
        vbox->addWidget(info);

        if (!body.trimmed().isEmpty()) {
            auto *notesLabel = new QLabel(QStringLiteral("<b>Release Notes:</b>"), &dlg);
            vbox->addWidget(notesLabel);

            auto *browser = new QTextBrowser(&dlg);
            browser->setOpenExternalLinks(true);
            browser->setPlainText(body);
            browser->setMaximumHeight(160);
            vbox->addWidget(browser);
        }

        auto *cmdLabel = new QLabel(QStringLiteral("Terminal installation / update command:"), &dlg);
        vbox->addWidget(cmdLabel);

        auto *cmdBox = new QLineEdit(installCmd, &dlg);
        cmdBox->setReadOnly(true);
        vbox->addWidget(cmdBox);

        auto *btnLayout = new QHBoxLayout();
        auto *btnOpenUrl = new QPushButton(QStringLiteral("Open on GitHub"), &dlg);
        btnOpenUrl->setIcon(QIcon::fromTheme(QStringLiteral("internet-web-browser")));
        connect(btnOpenUrl, &QPushButton::clicked, [htmlUrl]() {
            if (!htmlUrl.isEmpty()) {
                QDesktopServices::openUrl(QUrl(htmlUrl));
            }
        });
        btnLayout->addWidget(btnOpenUrl);

        auto *btnCopy = new QPushButton(QStringLiteral("Copy Command"), &dlg);
        btnCopy->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
        connect(btnCopy, &QPushButton::clicked, [installCmd, btnCopy]() {
            QGuiApplication::clipboard()->setText(installCmd);
            btnCopy->setText(QStringLiteral("Copied!"));
        });
        btnLayout->addWidget(btnCopy);

        btnLayout->addStretch();

        auto *btnClose = new QPushButton(QStringLiteral("Close"), &dlg);
        connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
        btnLayout->addWidget(btnClose);

        vbox->addLayout(btnLayout);
        dlg.exec();
    });
}

void ActiveWindowOutlineConfig::save()
{
    KCModule::save();

    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-active-window-outline"));
    group.writeEntry(QStringLiteral("ColorList"), serializeColorTable());
    group.writeEntry(QStringLiteral("AmbilightEnsureContrast"), m_ui->kcfg_EnsurePerimeterContrast->isChecked());
    group.writeEntry(QStringLiteral("StylePrecedence"), serializePrecedenceList());
    const int badgeMode = m_ui->kcfg_BadgeDisplayMode->currentIndex();
    group.writeEntry(QStringLiteral("MarkShowLetterBadges"), badgeMode != 2);
    group.writeEntry(QStringLiteral("BadgesAlwaysVisible"), badgeMode == 1);
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
