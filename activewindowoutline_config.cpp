/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "activewindowoutline_config.h"
#include "ui_activewindowoutline_config.h"
#include "precedencehelper.h"
#include "presetactivationdialog.h"
#include "presethelper.h"
#include "updatehelper.h"

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
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBrowser>
#include <QTextEdit>
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

    // Wrap each tab in a scroll area so large tabs never force the dialog off screen
    struct TabInfo {
        QWidget *page;
        QString text;
        QIcon icon;
        QString toolTip;
    };
    QList<TabInfo> tabs;
    for (int i = 0; i < m_ui->tabWidget->count(); ++i) {
        tabs.append({
            m_ui->tabWidget->widget(i),
            m_ui->tabWidget->tabText(i),
            m_ui->tabWidget->tabIcon(i),
            m_ui->tabWidget->tabToolTip(i)
        });
    }
    m_ui->tabWidget->clear();
    for (const auto &t : tabs) {
        auto *scrollArea = new QScrollArea(m_ui->tabWidget);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->viewport()->setAutoFillBackground(false);
        scrollArea->setWidget(t.page);
        const int idx = m_ui->tabWidget->addTab(scrollArea, t.icon, t.text);
        if (!t.toolTip.isEmpty()) {
            m_ui->tabWidget->setTabToolTip(idx, t.toolTip);
        }
    }
    m_ui->tabWidget->setCurrentIndex(0);

    m_ui->label_versionValue->setText(QStringLiteral(ACTIVE_WINDOW_OUTLINE_VERSION));

    m_ui->btnCheckUpdate->setIcon(QIcon::fromTheme(QStringLiteral("system-software-update"),
                                                  QIcon::fromTheme(QStringLiteral("view-refresh"))));
    connect(m_ui->btnCheckUpdate, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::checkForUpdates);

    m_ui->btnResetDefaults->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo"),
                                                    QIcon::fromTheme(QStringLiteral("document-revert"))));
    connect(m_ui->btnResetDefaults, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(
            widget(),
            tr("Reset to Defaults"),
            tr("Are you sure you want to reset all settings to their default values?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            defaults();
        }
    });

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
    m_configSkeleton->addItemBool(QStringLiteral("GlowEnabled"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("GlowRadius"), *new int, 6);
    m_configSkeleton->addItemBool(QStringLiteral("HideOnFullScreen"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("TrayIconEnabled"), *new bool, true);

    // Tab 4: Behavior & Buffer Delays
    m_configSkeleton->addItemInt(QStringLiteral("FadeDuration"), *new int, 200);
    m_configSkeleton->addItemInt(QStringLiteral("MenuHideDelay"), *new int, 500);
    m_configSkeleton->addItemInt(QStringLiteral("MenuShowDelay"), *new int, 500);
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
    m_configSkeleton->addItemBool(QStringLiteral("MarkSmartAdaptive"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("MarkRandomColor"), *new bool, false);
    m_configSkeleton->addItemString(QStringLiteral("StylePrecedence"), *new QString, QStringLiteral("Marked,Active,Recent"));
    m_configSkeleton->addItemInt(QStringLiteral("MarkInactiveWidthReduction"), *new int, 1);
    m_configSkeleton->addItemBool(QStringLiteral("MarkInactiveDashed"), *new bool, true);
    m_configSkeleton->addItemInt(QStringLiteral("MarkInactiveDashGap"), *new int, 6);
    m_configSkeleton->addItemBool(QStringLiteral("MarkInactiveDashSmooth"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("MarkShowLetterBadges"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("BadgesAlwaysVisible"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeDisplayMode"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgePosition"), *new int, 1);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudPosition"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeSize"), *new int, 28);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeMargin"), *new int, 0);
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
    connect(m_ui->kcfg_MarkSmartAdaptive, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && m_ui->kcfg_MarkRandomColor->isChecked()) {
            m_ui->kcfg_MarkRandomColor->setChecked(false);
        }
        updateMarkWindowUI();
    });
    connect(m_ui->kcfg_MarkRandomColor, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && m_ui->kcfg_MarkSmartAdaptive->isChecked()) {
            m_ui->kcfg_MarkSmartAdaptive->setChecked(false);
        }
        updateMarkWindowUI();
    });
    updateMarkWindowUI();

    QString shortcutStr = QStringLiteral("Meta+Space");
    const auto shortcuts = KGlobalAccel::self()->globalShortcut(QStringLiteral("kwin"), QStringLiteral("JumpToWindow"));
    if (!shortcuts.isEmpty() && !shortcuts.first().isEmpty()) {
        shortcutStr = shortcuts.first().toString(QKeySequence::NativeText);
    }
    m_ui->label_jumpTargets->setText(tr("Jump targets (default: %1):").arg(shortcutStr));
    m_ui->label_jumpTargets->setToolTip(tr("Scope of windows included when pressing the Jump to Window shortcut (%1).").arg(shortcutStr));
    m_ui->kcfg_JumpTargets->setToolTip(tr("Scope of windows included when pressing the Jump to Window shortcut (%1).").arg(shortcutStr));

    const bool isPolish = (QLocale().language() == QLocale::Polish);
    if (isPolish) {
        m_ui->label_jumpTargets->setText(QStringLiteral("Cele skoku (%1):").arg(shortcutStr));
        m_ui->label_jumpTargets->setToolTip(QStringLiteral("Zakres okien uwzględnianych po wciśnięciu skrótu Skok do okna (%1).").arg(shortcutStr));
        m_ui->kcfg_JumpTargets->setToolTip(QStringLiteral("Zakres okien uwzględnianych po wciśnięciu skrótu Skok do okna (%1).").arg(shortcutStr));
        if (m_ui->kcfg_JumpTargets->count() >= 4) {
            m_ui->kcfg_JumpTargets->setItemText(0, QStringLiteral("Bieżący pulpit: widoczne okna + oznaczone okna ze wszystkich pulpitów"));
            m_ui->kcfg_JumpTargets->setItemText(1, QStringLiteral("Bieżący pulpit: wszystkie okna (widoczne i zminimalizowane) + oznaczone"));
            m_ui->kcfg_JumpTargets->setItemText(2, QStringLiteral("Wszystkie okna na wszystkich pulpitach wirtualnych (widoczne i zminimalizowane)"));
            m_ui->kcfg_JumpTargets->setItemText(3, QStringLiteral("Tylko oznaczone okna (wszystkie pulpity)"));
        }
    }

    connect(m_ui->btnPrecedenceUp, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::movePrecedenceUp);
    connect(m_ui->btnPrecedenceDown, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::movePrecedenceDown);
    connect(m_ui->btnPrecedenceReset, &QPushButton::clicked, this, [this] {
        populatePrecedenceList(QStringLiteral("Marked,Active,Recent"));
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

    // Presets tab controls
    initPresetsTab();
    connect(m_ui->btnActivatePreset, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::activateSelectedPreset);
    connect(m_ui->btnNewPreset, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::createNewPreset);
    connect(m_ui->btnEditPreset, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::editSelectedPreset);
    connect(m_ui->btnDuplicatePreset, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::duplicateSelectedPreset);
    connect(m_ui->btnDeletePreset, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::deleteSelectedPreset);
    connect(m_ui->btnImportPreset, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::importPreset);
    connect(m_ui->btnExportPreset, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::exportSelectedPreset);
    connect(m_ui->tablePresets, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        activateSelectedPreset();
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
    const bool smartAdaptive = m_ui->kcfg_MarkSmartAdaptive->isChecked();
    const bool randomColor = m_ui->kcfg_MarkRandomColor->isChecked();
    m_ui->kcfg_MarkDefaultColor->setEnabled(!smartAdaptive && !randomColor);

    const bool dashed = m_ui->kcfg_MarkInactiveDashed->isChecked();
    m_ui->label_markInactiveDashGap->setEnabled(dashed);
    m_ui->kcfg_MarkInactiveDashGap->setEnabled(dashed);
    m_ui->kcfg_MarkInactiveDashSmooth->setEnabled(dashed);

    const bool badgesEnabled = (m_ui->kcfg_BadgeDisplayMode->currentIndex() != 2);
    m_ui->label_badgePosition->setEnabled(badgesEnabled);
    m_ui->kcfg_BadgePosition->setEnabled(badgesEnabled);
    m_ui->label_badgeHudPosition->setEnabled(badgesEnabled);
    m_ui->kcfg_BadgeHudPosition->setEnabled(badgesEnabled);
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
    populatePrecedenceList(group.readEntry(QStringLiteral("StylePrecedence"), QStringLiteral("Marked,Active,Recent")));
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
    populatePrecedenceList(QStringLiteral("Marked,Active,Recent"));
    updatePresetSelector();
    const bool fadeCorners = m_ui->kcfg_FadeCorners->isChecked();
    m_ui->label_borderRadius->setEnabled(!fadeCorners);
    m_ui->kcfg_BorderRadius->setEnabled(!fadeCorners);
    m_ui->kcfg_BadgeDisplayMode->setCurrentIndex(0);
    m_ui->kcfg_BadgePosition->setCurrentIndex(1);
    m_ui->kcfg_BadgeHudPosition->setCurrentIndex(0);
    m_ui->kcfg_BadgeSize->setValue(28);
    m_ui->kcfg_BadgeMargin->setValue(0);
    m_ui->kcfg_MarkSmartAdaptive->setChecked(true);
    m_ui->kcfg_MarkRandomColor->setChecked(false);
    m_ui->kcfg_MarkInactiveWidthReduction->setValue(1);
    m_ui->kcfg_MarkInactiveDashed->setChecked(true);
    m_ui->kcfg_MarkInactiveDashSmooth->setChecked(true);
    m_ui->kcfg_GlowEnabled->setChecked(false);
    m_ui->kcfg_FadeDuration->setValue(200);
    m_ui->kcfg_MenuHideDelay->setValue(500);
    m_ui->kcfg_MenuShowDelay->setValue(500);
    m_ui->kcfg_TrayIconEnabled->setChecked(true);
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

    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/gmalinowski/leapk/releases")));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LeapK-KCM"));
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
        const bool isPolish = QLocale().language() == QLocale::Polish;

        if (!hasUpdate) {
            m_ui->label_updateStatus->setText(isPolish
                ? QStringLiteral("✓ Wtyczka jest aktualna (v%1)").arg(currentVerStr)
                : QStringLiteral("✓ Up to date (v%1)").arg(currentVerStr));
            m_ui->label_updateStatus->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));

            LeapK::showUpToDateDialog(widget(), currentVerStr, htmlUrl, isPolish);
            return;
        }

        // New version available!
        m_ui->label_updateStatus->setText(isPolish
            ? QStringLiteral("⬆ Dostępna nowa wersja: %1!").arg(tagName)
            : QStringLiteral("⬆ New version: %1!").arg(tagName));
        m_ui->label_updateStatus->setStyleSheet(QStringLiteral("color: #3daee9; font-weight: bold;"));

        LeapK::showNewVersionDialog(widget(), tagName, currentVerStr, isPrerelease, htmlUrl, body, isPolish);
    });
}

void ActiveWindowOutlineConfig::initPresetsTab()
{
    m_ui->tablePresets->setColumnCount(4);
    const bool isPolish = (QLocale().language() == QLocale::Polish);
    m_ui->tablePresets->setHorizontalHeaderLabels({
        isPolish ? QStringLiteral("Nazwa presetu") : QStringLiteral("Preset Name"),
        isPolish ? QStringLiteral("Opis") : QStringLiteral("Description"),
        isPolish ? QStringLiteral("Typ") : QStringLiteral("Type"),
        isPolish ? QStringLiteral("Eksport") : QStringLiteral("Export")
    });
    m_ui->tablePresets->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui->tablePresets->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_ui->tablePresets->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui->tablePresets->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_ui->tablePresets->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->tablePresets->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ui->tablePresets->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ui->tablePresets->verticalHeader()->setVisible(false);

    if (isPolish) {
        m_ui->btnActivatePreset->setText(QStringLiteral("Aktywuj preset…"));
        m_ui->btnNewPreset->setText(QStringLiteral("Zapisz obecne jako preset…"));
        m_ui->btnNewPreset->setToolTip(QStringLiteral("Zapisz bieżące ustawienia kompozytora jako nowy preset"));
        m_ui->btnEditPreset->setText(QStringLiteral("Edytuj…"));
        m_ui->btnEditPreset->setToolTip(QStringLiteral("Edytuj nazwę i opis wybranego presetu"));
        m_ui->btnDuplicatePreset->setText(QStringLiteral("Duplikuj…"));
        m_ui->btnDeletePreset->setText(QStringLiteral("Usuń"));
        m_ui->btnImportPreset->setText(QStringLiteral("Importuj preset…"));
        m_ui->btnExportPreset->setText(QStringLiteral("Eksportuj preset…"));
    }

    m_ui->tablePresets->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui->tablePresets, &QTableWidget::customContextMenuRequested, this, [this, isPolish](const QPoint &pos) {
        const int row = m_ui->tablePresets->rowAt(pos.y());
        if (row < 0) return;
        m_ui->tablePresets->selectRow(row);

        QMenu menu(widget());
        QAction *actActivate = menu.addAction(QIcon::fromTheme(QStringLiteral("dialog-ok")), isPolish ? QStringLiteral("Aktywuj preset…") : QStringLiteral("Activate Preset…"));
        menu.addSeparator();
        QAction *actEdit = menu.addAction(QIcon::fromTheme(QStringLiteral("document-edit")), isPolish ? QStringLiteral("Edytuj nazwę i opis…") : QStringLiteral("Edit Name & Description…"));
        QAction *actDuplicate = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), isPolish ? QStringLiteral("Duplikuj…") : QStringLiteral("Duplicate…"));
        QAction *actDelete = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), isPolish ? QStringLiteral("Usuń") : QStringLiteral("Delete"));
        menu.addSeparator();
        QAction *actExport = menu.addAction(QIcon::fromTheme(QStringLiteral("document-export")), isPolish ? QStringLiteral("Eksportuj…") : QStringLiteral("Export…"));

        connect(actActivate, &QAction::triggered, this, &ActiveWindowOutlineConfig::activateSelectedPreset);
        connect(actEdit, &QAction::triggered, this, &ActiveWindowOutlineConfig::editSelectedPreset);
        connect(actDuplicate, &QAction::triggered, this, &ActiveWindowOutlineConfig::duplicateSelectedPreset);
        connect(actDelete, &QAction::triggered, this, &ActiveWindowOutlineConfig::deleteSelectedPreset);
        connect(actExport, &QAction::triggered, this, &ActiveWindowOutlineConfig::exportSelectedPreset);

        menu.exec(m_ui->tablePresets->viewport()->mapToGlobal(pos));
    });

    refreshPresetsTable();
}

void ActiveWindowOutlineConfig::refreshPresetsTable()
{
    const QString presetsDir = OutlinePreset::defaultPresetsDirectory();
    const QList<OutlinePreset::PresetInfo> presets = OutlinePreset::loadPresets(presetsDir);

    m_ui->tablePresets->setRowCount(0);
    const bool isPolish = (QLocale().language() == QLocale::Polish);

    for (int row = 0; row < presets.size(); ++row) {
        const auto &p = presets[row];
        m_ui->tablePresets->insertRow(row);

        auto *item0 = new QTableWidgetItem(p.name);
        item0->setData(Qt::UserRole, p.name);
        item0->setData(Qt::UserRole + 1, p.filePath);
        item0->setData(Qt::UserRole + 2, p.isBuiltIn);
        QFont f = item0->font();
        if (p.isBuiltIn) {
            f.setBold(true);
        }
        item0->setFont(f);
        m_ui->tablePresets->setItem(row, 0, item0);

        auto *item1 = new QTableWidgetItem(p.description);
        item1->setToolTip(p.description);
        m_ui->tablePresets->setItem(row, 1, item1);

        QString typeStr;
        if (p.isBuiltIn) {
            typeStr = isPolish ? QStringLiteral("Wbudowany") : QStringLiteral("Built-in");
        } else {
            typeStr = isPolish ? QStringLiteral("Własny") : QStringLiteral("User");
        }
        auto *item2 = new QTableWidgetItem(typeStr);
        item2->setTextAlignment(Qt::AlignCenter);
        m_ui->tablePresets->setItem(row, 2, item2);

        QPushButton *btnExport = new QPushButton(QIcon::fromTheme(QStringLiteral("document-export")), isPolish ? QStringLiteral("Eksportuj") : QStringLiteral("Export"));
        btnExport->setToolTip(isPolish ? QStringLiteral("Eksportuj preset do pliku JSON") : QStringLiteral("Export preset to JSON file"));
        connect(btnExport, &QPushButton::clicked, this, [this, name = p.name]() {
            exportPresetByName(name);
        });
        m_ui->tablePresets->setCellWidget(row, 3, btnExport);
    }

    if (m_ui->tablePresets->rowCount() > 0) {
        m_ui->tablePresets->selectRow(0);
    }

    updatePresetsTableHeight();
}

void ActiveWindowOutlineConfig::updatePresetsTableHeight()
{
    m_ui->tablePresets->doItemsLayout();
    int headerHeight = m_ui->tablePresets->horizontalHeader()->sizeHint().height();
    if (headerHeight <= 0) {
        headerHeight = 28;
    }
    const int count = m_ui->tablePresets->rowCount();
    int rowHeight = 32;
    if (count > 0 && m_ui->tablePresets->rowHeight(0) > 0) {
        rowHeight = m_ui->tablePresets->rowHeight(0);
    }
    const int totalH = headerHeight + (count * rowHeight) + m_ui->tablePresets->frameWidth() * 2 + 4;
    m_ui->tablePresets->setFixedHeight(qBound(140, totalH, 360));
}

QJsonObject ActiveWindowOutlineConfig::collectCurrentSettings() const
{
    QJsonObject s;

    // General & Appearance
    s[QStringLiteral("UseAccentColor")] = m_ui->kcfg_UseAccentColor->isChecked();
    s[QStringLiteral("AccentOpacity")] = m_ui->kcfg_AccentOpacity->value();
    s[QStringLiteral("CustomColor")] = m_ui->kcfg_CustomColor->color().name(QColor::HexRgb);
    s[QStringLiteral("EnsurePerimeterContrast")] = m_ui->kcfg_EnsurePerimeterContrast->isChecked();
    s[QStringLiteral("BorderWidth")] = m_ui->kcfg_BorderWidth->value();
    s[QStringLiteral("OutlineGap")] = m_ui->kcfg_OutlineGap->value();
    s[QStringLiteral("BorderRadius")] = m_ui->kcfg_BorderRadius->value();
    s[QStringLiteral("FadeCorners")] = m_ui->kcfg_FadeCorners->isChecked();
    s[QStringLiteral("DrawInside")] = m_ui->kcfg_DrawInside->isChecked();
    s[QStringLiteral("SmoothEdges")] = m_ui->kcfg_SmoothEdges->isChecked();
    s[QStringLiteral("GlowEnabled")] = m_ui->kcfg_GlowEnabled->isChecked();
    s[QStringLiteral("GlowRadius")] = m_ui->kcfg_GlowRadius->value();
    s[QStringLiteral("HideOnFullScreen")] = m_ui->kcfg_HideOnFullScreen->isChecked();
    s[QStringLiteral("TrayIconEnabled")] = m_ui->kcfg_TrayIconEnabled->isChecked();

    // Timing & Delays
    s[QStringLiteral("FadeDuration")] = m_ui->kcfg_FadeDuration->value();
    s[QStringLiteral("MenuHideDelay")] = m_ui->kcfg_MenuHideDelay->value();
    s[QStringLiteral("MenuShowDelay")] = m_ui->kcfg_MenuShowDelay->value();
    s[QStringLiteral("WindowHideDelay")] = m_ui->kcfg_WindowHideDelay->value();
    s[QStringLiteral("WindowShowDelay")] = m_ui->kcfg_WindowShowDelay->value();

    // Recent Windows
    s[QStringLiteral("HighlightRecentWindow")] = m_ui->kcfg_HighlightRecentWindow->isChecked();
    s[QStringLiteral("RecentWindowsCount")] = m_ui->kcfg_RecentWindowsCount->value();
    s[QStringLiteral("RecentWindowColorMode")] = m_ui->kcfg_RecentWindowColorMode->currentIndex();
    s[QStringLiteral("RecentWindowCustomColor")] = m_ui->kcfg_RecentWindowCustomColor->color().name(QColor::HexRgb);
    s[QStringLiteral("RecentWindowOpacity")] = m_ui->kcfg_RecentWindowOpacity->value();
    s[QStringLiteral("RecentWindowHueShift")] = m_ui->kcfg_RecentWindowHueShift->value();
    s[QStringLiteral("RecentWindowSaturation")] = m_ui->kcfg_RecentWindowSaturation->value();
    s[QStringLiteral("RecentWindowWidthReduction")] = m_ui->kcfg_RecentWindowWidthReduction->value();
    s[QStringLiteral("RecentWindowDashed")] = m_ui->kcfg_RecentWindowDashed->isChecked();
    s[QStringLiteral("RecentWindowGapGrowth")] = m_ui->kcfg_RecentWindowGapGrowth->value();
    s[QStringLiteral("RecentWindowDashSmooth")] = m_ui->kcfg_RecentWindowDashSmooth->isChecked();

    // Marked Windows
    s[QStringLiteral("MarkDefaultColor")] = m_ui->kcfg_MarkDefaultColor->color().name(QColor::HexRgb);
    s[QStringLiteral("MarkSmartAdaptive")] = m_ui->kcfg_MarkSmartAdaptive->isChecked();
    s[QStringLiteral("MarkRandomColor")] = m_ui->kcfg_MarkRandomColor->isChecked();
    s[QStringLiteral("StylePrecedence")] = serializePrecedenceList();
    s[QStringLiteral("MarkInactiveWidthReduction")] = m_ui->kcfg_MarkInactiveWidthReduction->value();
    s[QStringLiteral("MarkInactiveDashed")] = m_ui->kcfg_MarkInactiveDashed->isChecked();
    s[QStringLiteral("MarkInactiveDashGap")] = m_ui->kcfg_MarkInactiveDashGap->value();
    s[QStringLiteral("MarkInactiveDashSmooth")] = m_ui->kcfg_MarkInactiveDashSmooth->isChecked();
    const int badgeMode = m_ui->kcfg_BadgeDisplayMode->currentIndex();
    s[QStringLiteral("BadgeDisplayMode")] = badgeMode;
    s[QStringLiteral("MarkShowLetterBadges")] = (badgeMode != 2);
    s[QStringLiteral("BadgesAlwaysVisible")] = (badgeMode == 1);
    s[QStringLiteral("BadgePosition")] = m_ui->kcfg_BadgePosition->currentIndex();
    s[QStringLiteral("BadgeHudPosition")] = m_ui->kcfg_BadgeHudPosition->currentIndex();
    s[QStringLiteral("BadgeSize")] = m_ui->kcfg_BadgeSize->value();
    s[QStringLiteral("BadgeMargin")] = m_ui->kcfg_BadgeMargin->value();
    s[QStringLiteral("JumpTargets")] = m_ui->kcfg_JumpTargets->currentIndex();

    // Color Cycling
    s[QStringLiteral("ColorCycleEnabled")] = m_ui->kcfg_ColorCycleEnabled->isChecked();
    s[QStringLiteral("ColorTransitionDuration")] = m_ui->kcfg_ColorTransitionDuration->value();
    s[QStringLiteral("ColorCycleOrder")] = m_ui->kcfg_ColorCycleOrder->currentIndex();
    s[QStringLiteral("ColorCycleEasing")] = m_ui->kcfg_ColorCycleEasing->currentIndex();
    s[QStringLiteral("ColorList")] = serializeColorTable();

    // Shader Effects
    s[QStringLiteral("BreatheEnabled")] = m_ui->kcfg_BreatheEnabled->isChecked();
    s[QStringLiteral("BreatheSpeed")] = m_ui->kcfg_BreatheSpeed->value();
    s[QStringLiteral("BreatheIntensity")] = m_ui->kcfg_BreatheIntensity->value();
    s[QStringLiteral("GlitchEnabled")] = m_ui->kcfg_GlitchEnabled->isChecked();
    s[QStringLiteral("GlitchSpeed")] = m_ui->kcfg_GlitchSpeed->value();
    s[QStringLiteral("GlitchIntensity")] = m_ui->kcfg_GlitchIntensity->value();
    s[QStringLiteral("GlitchAnomalyEnabled")] = m_ui->kcfg_GlitchAnomalyEnabled->isChecked();
    s[QStringLiteral("GlitchFrequency")] = m_ui->kcfg_GlitchFrequency->value();
    s[QStringLiteral("NeonFlickerEnabled")] = m_ui->kcfg_NeonFlickerEnabled->isChecked();
    s[QStringLiteral("NeonFlickerInterval")] = m_ui->kcfg_NeonFlickerInterval->value();
    s[QStringLiteral("NeonFlickerDuration")] = m_ui->kcfg_NeonFlickerDuration->value();
    s[QStringLiteral("NeonFlickerIntensity")] = m_ui->kcfg_NeonFlickerIntensity->value();
    s[QStringLiteral("FlashEnabled")] = m_ui->kcfg_FlashEnabled->isChecked();
    s[QStringLiteral("FlashInterval")] = m_ui->kcfg_FlashInterval->value();
    s[QStringLiteral("FlashDuration")] = m_ui->kcfg_FlashDuration->value();

    // Ambilight
    s[QStringLiteral("AmbilightEnabled")] = m_ui->kcfg_AmbilightEnabled->isChecked();
    s[QStringLiteral("AmbilightMode")] = m_ui->kcfg_AmbilightMode->currentIndex();
    s[QStringLiteral("AmbilightOpacity")] = m_ui->kcfg_AmbilightOpacity->value();
    s[QStringLiteral("AmbilightFrequency")] = m_ui->kcfg_AmbilightFrequency->value();
    s[QStringLiteral("AmbilightDuration")] = m_ui->kcfg_AmbilightDuration->value();
    s[QStringLiteral("AmbilightSaturationLeveling")] = m_ui->kcfg_AmbilightSaturationLeveling->value();
    s[QStringLiteral("AmbilightFallbackStrength")] = m_ui->kcfg_AmbilightFallbackStrength->value();
    s[QStringLiteral("AmbilightMinBrightness")] = m_ui->kcfg_AmbilightMinBrightness->value();
    s[QStringLiteral("AmbilightMaxBrightness")] = m_ui->kcfg_AmbilightMaxBrightness->value();
    s[QStringLiteral("AmbilightEnsureContrast")] = m_ui->kcfg_EnsurePerimeterContrast->isChecked();

    return s;
}

void ActiveWindowOutlineConfig::applySettings(const QJsonObject &s)
{
    if (s.isEmpty()) {
        return;
    }

    // General
    if (s.contains(QStringLiteral("UseAccentColor"))) m_ui->kcfg_UseAccentColor->setChecked(s[QStringLiteral("UseAccentColor")].toBool());
    if (s.contains(QStringLiteral("AccentOpacity"))) m_ui->kcfg_AccentOpacity->setValue(s[QStringLiteral("AccentOpacity")].toInt());
    if (s.contains(QStringLiteral("CustomColor"))) m_ui->kcfg_CustomColor->setColor(QColor(s[QStringLiteral("CustomColor")].toString()));
    if (s.contains(QStringLiteral("EnsurePerimeterContrast"))) {
        m_ui->kcfg_EnsurePerimeterContrast->setChecked(s[QStringLiteral("EnsurePerimeterContrast")].toBool());
    } else if (s.contains(QStringLiteral("AmbilightEnsureContrast"))) {
        m_ui->kcfg_EnsurePerimeterContrast->setChecked(s[QStringLiteral("AmbilightEnsureContrast")].toBool());
    }
    if (s.contains(QStringLiteral("BorderWidth"))) m_ui->kcfg_BorderWidth->setValue(s[QStringLiteral("BorderWidth")].toInt());
    if (s.contains(QStringLiteral("OutlineGap"))) m_ui->kcfg_OutlineGap->setValue(s[QStringLiteral("OutlineGap")].toInt());
    if (s.contains(QStringLiteral("BorderRadius"))) m_ui->kcfg_BorderRadius->setValue(s[QStringLiteral("BorderRadius")].toInt());
    if (s.contains(QStringLiteral("FadeCorners"))) m_ui->kcfg_FadeCorners->setChecked(s[QStringLiteral("FadeCorners")].toBool());
    if (s.contains(QStringLiteral("DrawInside"))) m_ui->kcfg_DrawInside->setChecked(s[QStringLiteral("DrawInside")].toBool());
    if (s.contains(QStringLiteral("SmoothEdges"))) m_ui->kcfg_SmoothEdges->setChecked(s[QStringLiteral("SmoothEdges")].toBool());
    if (s.contains(QStringLiteral("GlowEnabled"))) m_ui->kcfg_GlowEnabled->setChecked(s[QStringLiteral("GlowEnabled")].toBool());
    if (s.contains(QStringLiteral("GlowRadius"))) m_ui->kcfg_GlowRadius->setValue(s[QStringLiteral("GlowRadius")].toInt());
    if (s.contains(QStringLiteral("HideOnFullScreen"))) m_ui->kcfg_HideOnFullScreen->setChecked(s[QStringLiteral("HideOnFullScreen")].toBool());
    if (s.contains(QStringLiteral("TrayIconEnabled"))) m_ui->kcfg_TrayIconEnabled->setChecked(s[QStringLiteral("TrayIconEnabled")].toBool());

    // Delays
    if (s.contains(QStringLiteral("FadeDuration"))) m_ui->kcfg_FadeDuration->setValue(s[QStringLiteral("FadeDuration")].toInt());
    if (s.contains(QStringLiteral("MenuHideDelay"))) m_ui->kcfg_MenuHideDelay->setValue(s[QStringLiteral("MenuHideDelay")].toInt());
    if (s.contains(QStringLiteral("MenuShowDelay"))) m_ui->kcfg_MenuShowDelay->setValue(s[QStringLiteral("MenuShowDelay")].toInt());
    if (s.contains(QStringLiteral("WindowHideDelay"))) m_ui->kcfg_WindowHideDelay->setValue(s[QStringLiteral("WindowHideDelay")].toInt());
    if (s.contains(QStringLiteral("WindowShowDelay"))) m_ui->kcfg_WindowShowDelay->setValue(s[QStringLiteral("WindowShowDelay")].toInt());

    // Recent Windows
    if (s.contains(QStringLiteral("HighlightRecentWindow"))) m_ui->kcfg_HighlightRecentWindow->setChecked(s[QStringLiteral("HighlightRecentWindow")].toBool());
    if (s.contains(QStringLiteral("RecentWindowsCount"))) m_ui->kcfg_RecentWindowsCount->setValue(s[QStringLiteral("RecentWindowsCount")].toInt());
    if (s.contains(QStringLiteral("RecentWindowColorMode"))) m_ui->kcfg_RecentWindowColorMode->setCurrentIndex(s[QStringLiteral("RecentWindowColorMode")].toInt());
    if (s.contains(QStringLiteral("RecentWindowCustomColor"))) m_ui->kcfg_RecentWindowCustomColor->setColor(QColor(s[QStringLiteral("RecentWindowCustomColor")].toString()));
    if (s.contains(QStringLiteral("RecentWindowOpacity"))) m_ui->kcfg_RecentWindowOpacity->setValue(s[QStringLiteral("RecentWindowOpacity")].toInt());
    if (s.contains(QStringLiteral("RecentWindowHueShift"))) m_ui->kcfg_RecentWindowHueShift->setValue(s[QStringLiteral("RecentWindowHueShift")].toInt());
    if (s.contains(QStringLiteral("RecentWindowSaturation"))) m_ui->kcfg_RecentWindowSaturation->setValue(s[QStringLiteral("RecentWindowSaturation")].toInt());
    if (s.contains(QStringLiteral("RecentWindowWidthReduction"))) m_ui->kcfg_RecentWindowWidthReduction->setValue(s[QStringLiteral("RecentWindowWidthReduction")].toInt());
    if (s.contains(QStringLiteral("RecentWindowDashed"))) m_ui->kcfg_RecentWindowDashed->setChecked(s[QStringLiteral("RecentWindowDashed")].toBool());
    if (s.contains(QStringLiteral("RecentWindowGapGrowth"))) m_ui->kcfg_RecentWindowGapGrowth->setValue(s[QStringLiteral("RecentWindowGapGrowth")].toInt());
    if (s.contains(QStringLiteral("RecentWindowDashSmooth"))) m_ui->kcfg_RecentWindowDashSmooth->setChecked(s[QStringLiteral("RecentWindowDashSmooth")].toBool());

    // Marked Windows
    if (s.contains(QStringLiteral("MarkDefaultColor"))) m_ui->kcfg_MarkDefaultColor->setColor(QColor(s[QStringLiteral("MarkDefaultColor")].toString()));
    if (s.contains(QStringLiteral("MarkSmartAdaptive"))) m_ui->kcfg_MarkSmartAdaptive->setChecked(s[QStringLiteral("MarkSmartAdaptive")].toBool());
    if (s.contains(QStringLiteral("MarkRandomColor"))) m_ui->kcfg_MarkRandomColor->setChecked(s[QStringLiteral("MarkRandomColor")].toBool());
    if (s.contains(QStringLiteral("StylePrecedence"))) populatePrecedenceList(s[QStringLiteral("StylePrecedence")].toString());
    if (s.contains(QStringLiteral("MarkInactiveWidthReduction"))) m_ui->kcfg_MarkInactiveWidthReduction->setValue(s[QStringLiteral("MarkInactiveWidthReduction")].toInt());
    if (s.contains(QStringLiteral("MarkInactiveDashed"))) m_ui->kcfg_MarkInactiveDashed->setChecked(s[QStringLiteral("MarkInactiveDashed")].toBool());
    if (s.contains(QStringLiteral("MarkInactiveDashGap"))) m_ui->kcfg_MarkInactiveDashGap->setValue(s[QStringLiteral("MarkInactiveDashGap")].toInt());
    if (s.contains(QStringLiteral("MarkInactiveDashSmooth"))) m_ui->kcfg_MarkInactiveDashSmooth->setChecked(s[QStringLiteral("MarkInactiveDashSmooth")].toBool());
    if (s.contains(QStringLiteral("BadgeDisplayMode"))) {
        m_ui->kcfg_BadgeDisplayMode->setCurrentIndex(s[QStringLiteral("BadgeDisplayMode")].toInt());
    } else if (s.contains(QStringLiteral("MarkShowLetterBadges")) || s.contains(QStringLiteral("BadgesAlwaysVisible"))) {
        const bool show = s.value(QStringLiteral("MarkShowLetterBadges")).toBool(true);
        const bool always = s.value(QStringLiteral("BadgesAlwaysVisible")).toBool(false);
        m_ui->kcfg_BadgeDisplayMode->setCurrentIndex(!show ? 2 : (always ? 1 : 0));
    }
    if (s.contains(QStringLiteral("BadgePosition"))) m_ui->kcfg_BadgePosition->setCurrentIndex(s[QStringLiteral("BadgePosition")].toInt());
    if (s.contains(QStringLiteral("BadgeHudPosition"))) m_ui->kcfg_BadgeHudPosition->setCurrentIndex(s[QStringLiteral("BadgeHudPosition")].toInt());
    if (s.contains(QStringLiteral("BadgeSize"))) m_ui->kcfg_BadgeSize->setValue(s[QStringLiteral("BadgeSize")].toInt());
    if (s.contains(QStringLiteral("BadgeMargin"))) m_ui->kcfg_BadgeMargin->setValue(s[QStringLiteral("BadgeMargin")].toInt());
    if (s.contains(QStringLiteral("JumpTargets"))) m_ui->kcfg_JumpTargets->setCurrentIndex(s[QStringLiteral("JumpTargets")].toInt());

    // Color Cycling
    if (s.contains(QStringLiteral("ColorCycleEnabled"))) m_ui->kcfg_ColorCycleEnabled->setChecked(s[QStringLiteral("ColorCycleEnabled")].toBool());
    if (s.contains(QStringLiteral("ColorTransitionDuration"))) m_ui->kcfg_ColorTransitionDuration->setValue(s[QStringLiteral("ColorTransitionDuration")].toDouble());
    if (s.contains(QStringLiteral("ColorCycleOrder"))) m_ui->kcfg_ColorCycleOrder->setCurrentIndex(s[QStringLiteral("ColorCycleOrder")].toInt());
    if (s.contains(QStringLiteral("ColorCycleEasing"))) m_ui->kcfg_ColorCycleEasing->setCurrentIndex(s[QStringLiteral("ColorCycleEasing")].toInt());
    if (s.contains(QStringLiteral("ColorList"))) populateColorTable(s[QStringLiteral("ColorList")].toString());

    // Shader Effects
    if (s.contains(QStringLiteral("BreatheEnabled"))) m_ui->kcfg_BreatheEnabled->setChecked(s[QStringLiteral("BreatheEnabled")].toBool());
    if (s.contains(QStringLiteral("BreatheSpeed"))) m_ui->kcfg_BreatheSpeed->setValue(s[QStringLiteral("BreatheSpeed")].toDouble());
    if (s.contains(QStringLiteral("BreatheIntensity"))) m_ui->kcfg_BreatheIntensity->setValue(s[QStringLiteral("BreatheIntensity")].toInt());
    if (s.contains(QStringLiteral("GlitchEnabled"))) m_ui->kcfg_GlitchEnabled->setChecked(s[QStringLiteral("GlitchEnabled")].toBool());
    if (s.contains(QStringLiteral("GlitchSpeed"))) m_ui->kcfg_GlitchSpeed->setValue(s[QStringLiteral("GlitchSpeed")].toDouble());
    if (s.contains(QStringLiteral("GlitchIntensity"))) m_ui->kcfg_GlitchIntensity->setValue(s[QStringLiteral("GlitchIntensity")].toInt());
    if (s.contains(QStringLiteral("GlitchAnomalyEnabled"))) m_ui->kcfg_GlitchAnomalyEnabled->setChecked(s[QStringLiteral("GlitchAnomalyEnabled")].toBool());
    if (s.contains(QStringLiteral("GlitchFrequency"))) m_ui->kcfg_GlitchFrequency->setValue(s[QStringLiteral("GlitchFrequency")].toInt());
    if (s.contains(QStringLiteral("NeonFlickerEnabled"))) m_ui->kcfg_NeonFlickerEnabled->setChecked(s[QStringLiteral("NeonFlickerEnabled")].toBool());
    if (s.contains(QStringLiteral("NeonFlickerInterval"))) m_ui->kcfg_NeonFlickerInterval->setValue(s[QStringLiteral("NeonFlickerInterval")].toDouble());
    if (s.contains(QStringLiteral("NeonFlickerDuration"))) m_ui->kcfg_NeonFlickerDuration->setValue(s[QStringLiteral("NeonFlickerDuration")].toDouble());
    if (s.contains(QStringLiteral("NeonFlickerIntensity"))) m_ui->kcfg_NeonFlickerIntensity->setValue(s[QStringLiteral("NeonFlickerIntensity")].toInt());
    if (s.contains(QStringLiteral("FlashEnabled"))) m_ui->kcfg_FlashEnabled->setChecked(s[QStringLiteral("FlashEnabled")].toBool());
    if (s.contains(QStringLiteral("FlashInterval"))) m_ui->kcfg_FlashInterval->setValue(s[QStringLiteral("FlashInterval")].toDouble());
    if (s.contains(QStringLiteral("FlashDuration"))) m_ui->kcfg_FlashDuration->setValue(s[QStringLiteral("FlashDuration")].toDouble());

    // Ambilight
    if (s.contains(QStringLiteral("AmbilightEnabled"))) m_ui->kcfg_AmbilightEnabled->setChecked(s[QStringLiteral("AmbilightEnabled")].toBool());
    if (s.contains(QStringLiteral("AmbilightMode"))) m_ui->kcfg_AmbilightMode->setCurrentIndex(s[QStringLiteral("AmbilightMode")].toInt());
    if (s.contains(QStringLiteral("AmbilightOpacity"))) m_ui->kcfg_AmbilightOpacity->setValue(s[QStringLiteral("AmbilightOpacity")].toInt());
    if (s.contains(QStringLiteral("AmbilightFrequency"))) m_ui->kcfg_AmbilightFrequency->setValue(s[QStringLiteral("AmbilightFrequency")].toDouble());
    if (s.contains(QStringLiteral("AmbilightDuration"))) m_ui->kcfg_AmbilightDuration->setValue(s[QStringLiteral("AmbilightDuration")].toDouble());
    if (s.contains(QStringLiteral("AmbilightSaturationLeveling"))) m_ui->kcfg_AmbilightSaturationLeveling->setValue(s[QStringLiteral("AmbilightSaturationLeveling")].toInt());
    if (s.contains(QStringLiteral("AmbilightFallbackStrength"))) m_ui->kcfg_AmbilightFallbackStrength->setValue(s[QStringLiteral("AmbilightFallbackStrength")].toInt());
    if (s.contains(QStringLiteral("AmbilightMinBrightness"))) m_ui->kcfg_AmbilightMinBrightness->setValue(s[QStringLiteral("AmbilightMinBrightness")].toInt());
    if (s.contains(QStringLiteral("AmbilightMaxBrightness"))) m_ui->kcfg_AmbilightMaxBrightness->setValue(s[QStringLiteral("AmbilightMaxBrightness")].toInt());

    // Refresh UI dependencies
    const bool fadeCorners = m_ui->kcfg_FadeCorners->isChecked();
    m_ui->label_borderRadius->setEnabled(!fadeCorners);
    m_ui->kcfg_BorderRadius->setEnabled(!fadeCorners);
    updateRecentWindowUI();
    updateMarkWindowUI();
    updatePresetSelector();
    markAsChanged();
}

void ActiveWindowOutlineConfig::activateSelectedPreset()
{
    const int row = m_ui->tablePresets->currentRow();
    if (row < 0) return;

    const QString presetName = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole).toString();
    const QString filePath = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole + 1).toString();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }

    OutlinePreset::PresetInfo p;
    if (OutlinePreset::parsePresetJson(f.readAll(), p)) {
        const bool isPolish = (QLocale().language() == QLocale::Polish);
        const QJsonObject currentSettings = collectCurrentSettings();
        QJsonObject settingsToApply;

        if (LeapK::PresetActivationDialog::selectSettings(widget(), p, currentSettings, settingsToApply, isPolish)) {
            applySettings(settingsToApply);
            const int count = settingsToApply.size();
            if (count == p.settings.size()) {
                m_ui->labelPresetStatus->setText(isPolish
                    ? QStringLiteral("✅ Aktywowano cały preset: %1").arg(presetName)
                    : QStringLiteral("✅ Activated full preset: %1").arg(presetName));
            } else {
                m_ui->labelPresetStatus->setText(isPolish
                    ? QStringLiteral("✅ Zastosowano %1 wybranych opcji z presetu: %2").arg(count).arg(presetName)
                    : QStringLiteral("✅ Applied %1 selected settings from preset: %2").arg(count).arg(presetName));
            }
            m_ui->labelPresetStatus->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));
            markAsChanged();
        }
    }
}

void ActiveWindowOutlineConfig::createNewPreset()
{
    const bool isPolish = (QLocale().language() == QLocale::Polish);
    const QString title = isPolish ? QStringLiteral("Zapisz obecne jako preset") : QStringLiteral("Save Current as Preset");

    QDialog dlg(widget());
    dlg.setWindowTitle(title);
    dlg.setWindowIcon(QIcon::fromTheme(QStringLiteral("document-save")));
    dlg.resize(480, 240);

    auto *vbox = new QVBoxLayout(&dlg);
    vbox->setSpacing(12);

    auto *formLayout = new QFormLayout();
    auto *nameEdit = new QLineEdit(QStringLiteral("My Preset"), &dlg);
    nameEdit->selectAll();
    auto *descEdit = new QTextEdit(&dlg);
    descEdit->setPlaceholderText(isPolish ? QStringLiteral("Opcjonalny opis presetu…") : QStringLiteral("Optional preset description…"));
    descEdit->setMaximumHeight(80);

    formLayout->addRow(isPolish ? QStringLiteral("Nazwa presetu:") : QStringLiteral("Preset Name:"), nameEdit);
    formLayout->addRow(isPolish ? QStringLiteral("Opis:") : QStringLiteral("Description:"), descEdit);
    vbox->addLayout(formLayout);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    if (isPolish) {
        btnBox->button(QDialogButtonBox::Save)->setText(QStringLiteral("Zapisz"));
        btnBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Anuluj"));
    }
    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(btnBox);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString trimmedName = nameEdit->text().trimmed();
    if (trimmedName.isEmpty()) {
        return;
    }

    const QString trimmedDesc = descEdit->toPlainText().trimmed();
    const QString presetsDir = OutlinePreset::defaultPresetsDirectory();

    OutlinePreset::PresetInfo p;
    p.name = trimmedName;
    p.description = trimmedDesc.isEmpty()
        ? (isPolish ? QStringLiteral("Własny preset użytkownika") : QStringLiteral("User custom preset"))
        : trimmedDesc;
    p.isBuiltIn = false;
    p.settings = collectCurrentSettings();

    QString err;
    if (!OutlinePreset::savePreset(presetsDir, p, &err)) {
        QMessageBox::warning(widget(), title, err);
        return;
    }

    refreshPresetsTable();

    // Select the newly created preset
    for (int r = 0; r < m_ui->tablePresets->rowCount(); ++r) {
        if (m_ui->tablePresets->item(r, 0)->text() == trimmedName) {
            m_ui->tablePresets->selectRow(r);
            break;
        }
    }

    m_ui->labelPresetStatus->setText(isPolish
        ? QStringLiteral("✅ Pomyślnie zapisano preset: %1").arg(trimmedName)
        : QStringLiteral("✅ Preset successfully saved: %1").arg(trimmedName));
    m_ui->labelPresetStatus->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));
}

void ActiveWindowOutlineConfig::editSelectedPreset()
{
    const int row = m_ui->tablePresets->currentRow();
    if (row < 0) return;

    const QString originalName = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole).toString();
    const QString filePath = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole + 1).toString();
    const bool isBuiltIn = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole + 2).toBool();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }

    OutlinePreset::PresetInfo p;
    if (!OutlinePreset::parsePresetJson(f.readAll(), p)) {
        return;
    }
    f.close();

    const bool isPolish = (QLocale().language() == QLocale::Polish);
    const QString title = isPolish ? QStringLiteral("Edytuj preset") : QStringLiteral("Edit Preset");

    QDialog dlg(widget());
    dlg.setWindowTitle(title);
    dlg.setWindowIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    dlg.resize(480, 260);

    auto *vbox = new QVBoxLayout(&dlg);
    vbox->setSpacing(12);

    auto *formLayout = new QFormLayout();
    auto *nameEdit = new QLineEdit(p.name, &dlg);
    auto *descEdit = new QTextEdit(&dlg);
    descEdit->setPlainText(p.description);
    descEdit->setMaximumHeight(80);

    formLayout->addRow(isPolish ? QStringLiteral("Nazwa presetu:") : QStringLiteral("Preset Name:"), nameEdit);
    formLayout->addRow(isPolish ? QStringLiteral("Opis:") : QStringLiteral("Description:"), descEdit);
    vbox->addLayout(formLayout);

    if (isBuiltIn) {
        auto *noteLabel = new QLabel(isPolish
            ? QStringLiteral("<i>ℹ️ Ten preset jest wbudowany. Zapisanie zmian utworzy Twój własny preset.</i>")
            : QStringLiteral("<i>ℹ️ This is a built-in preset. Saving changes will create a custom user preset.</i>"), &dlg);
        noteLabel->setWordWrap(true);
        noteLabel->setStyleSheet(QStringLiteral("color: #3daee9;"));
        vbox->addWidget(noteLabel);
    }

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    if (isPolish) {
        btnBox->button(QDialogButtonBox::Save)->setText(QStringLiteral("Zapisz zmiany"));
        btnBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Anuluj"));
    }
    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(btnBox);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString newName = nameEdit->text().trimmed();
    if (newName.isEmpty()) {
        QMessageBox::warning(widget(), title,
            isPolish ? QStringLiteral("Nazwa presetu nie może być pusta.")
                     : QStringLiteral("Preset name cannot be empty."));
        return;
    }

    const QString newDesc = descEdit->toPlainText().trimmed();
    const QString presetsDir = OutlinePreset::defaultPresetsDirectory();

    // If name changed and this is a custom user preset, remove the old file
    if (!isBuiltIn && newName != originalName) {
        QFile::remove(filePath);
    }

    p.name = newName;
    p.description = newDesc;
    p.isBuiltIn = false; // User modifications become user presets

    QString err;
    if (!OutlinePreset::savePreset(presetsDir, p, &err)) {
        QMessageBox::warning(widget(), title, err);
        return;
    }

    refreshPresetsTable();

    // Select the edited preset
    for (int r = 0; r < m_ui->tablePresets->rowCount(); ++r) {
        if (m_ui->tablePresets->item(r, 0)->text() == newName) {
            m_ui->tablePresets->selectRow(r);
            break;
        }
    }

    m_ui->labelPresetStatus->setText(isPolish
        ? QStringLiteral("✅ Zaktualizowano preset: %1").arg(newName)
        : QStringLiteral("✅ Updated preset: %1").arg(newName));
    m_ui->labelPresetStatus->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));
}

void ActiveWindowOutlineConfig::duplicateSelectedPreset()
{
    const int row = m_ui->tablePresets->currentRow();
    if (row < 0) return;

    const QString originalName = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole).toString();
    const QString filePath = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole + 1).toString();

    const bool isPolish = (QLocale().language() == QLocale::Polish);
    const QString title = isPolish ? QStringLiteral("Duplikuj preset") : QStringLiteral("Duplicate Preset");
    const QString label = isPolish ? QStringLiteral("Podaj nazwę dla duplikatu:") : QStringLiteral("Enter name for duplicate:");

    bool ok = false;
    const QString newName = QInputDialog::getText(widget(), title, label, QLineEdit::Normal, originalName + QStringLiteral(" Copy"), &ok);
    if (!ok || newName.trimmed().isEmpty()) {
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }

    OutlinePreset::PresetInfo p;
    if (OutlinePreset::parsePresetJson(f.readAll(), p)) {
        p.name = newName.trimmed();
        p.isBuiltIn = false;
        OutlinePreset::savePreset(OutlinePreset::defaultPresetsDirectory(), p);
        refreshPresetsTable();

        for (int r = 0; r < m_ui->tablePresets->rowCount(); ++r) {
            if (m_ui->tablePresets->item(r, 0)->text() == p.name) {
                m_ui->tablePresets->selectRow(r);
                break;
            }
        }
    }
}

void ActiveWindowOutlineConfig::deleteSelectedPreset()
{
    const int row = m_ui->tablePresets->currentRow();
    if (row < 0) return;

    const QString name = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole).toString();
    const QString filePath = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole + 1).toString();
    const bool isBuiltIn = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole + 2).toBool();

    const bool isPolish = (QLocale().language() == QLocale::Polish);
    const QString title = isPolish ? QStringLiteral("Usuń preset") : QStringLiteral("Delete Preset");

    if (isBuiltIn) {
        const auto reply = QMessageBox::question(widget(), title,
            isPolish ? QStringLiteral("Preset '%1' jest presetem wbudowanym. Usunięcie go spowoduje przywrócenie wartości domyślnych przy kolejnym uruchomieniu. Czy na pewno chcesz go usunąć?").arg(name)
                     : QStringLiteral("Preset '%1' is a built-in preset. Deleting it will restore its defaults on restart. Are you sure?").arg(name),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    } else {
        const auto reply = QMessageBox::question(widget(), title,
            isPolish ? QStringLiteral("Czy na pewno chcesz usunąć preset '%1'?").arg(name)
                     : QStringLiteral("Are you sure you want to delete preset '%1'?").arg(name),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    QFile::remove(filePath);
    refreshPresetsTable();
}

void ActiveWindowOutlineConfig::exportPresetByName(const QString &presetName)
{
    const QString presetsDir = OutlinePreset::defaultPresetsDirectory();
    QString safeName = presetName;
    safeName.replace(QLatin1Char('/'), QLatin1Char('_'));
    const QString sourcePath = presetsDir + QStringLiteral("/") + safeName + QStringLiteral(".json");

    QFile f(sourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }

    const QByteArray data = f.readAll();
    const bool isPolish = (QLocale().language() == QLocale::Polish);
    const QString title = isPolish ? QStringLiteral("Eksportuj preset") : QStringLiteral("Export Preset");
    const QString defaultPath = QDir::homePath() + QStringLiteral("/") + safeName + QStringLiteral(".json");
    const QString filter = isPolish ? QStringLiteral("Pliki presetów JSON (*.json);;Wszystkie pliki (*)") : QStringLiteral("JSON Preset Files (*.json);;All Files (*)");

    const QString targetPath = QFileDialog::getSaveFileName(widget(), title, defaultPath, filter);
    if (targetPath.isEmpty()) {
        return;
    }

    QFile out(targetPath);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        out.write(data);
        out.close();
        QMessageBox::information(widget(), title,
            isPolish ? QStringLiteral("Preset '%1' został pomyślnie wyeksportowany do:\n%2").arg(presetName, targetPath)
                     : QStringLiteral("Preset '%1' successfully exported to:\n%2").arg(presetName, targetPath));
    }
}

void ActiveWindowOutlineConfig::exportSelectedPreset()
{
    const int row = m_ui->tablePresets->currentRow();
    if (row < 0) return;
    const QString presetName = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole).toString();
    exportPresetByName(presetName);
}

void ActiveWindowOutlineConfig::importPreset()
{
    const bool isPolish = (QLocale().language() == QLocale::Polish);
    const QString title = isPolish ? QStringLiteral("Importuj preset") : QStringLiteral("Import Preset");
    const QString filter = isPolish ? QStringLiteral("Pliki presetów JSON (*.json);;Wszystkie pliki (*)") : QStringLiteral("JSON Preset Files (*.json);;All Files (*)");

    const QString filePath = QFileDialog::getOpenFileName(widget(), title, QDir::homePath(), filter);
    if (filePath.isEmpty()) {
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(widget(), title,
            isPolish ? QStringLiteral("Nie udało się otworzyć pliku: %1").arg(f.errorString())
                     : QStringLiteral("Could not open file: %1").arg(f.errorString()));
        return;
    }

    OutlinePreset::PresetInfo p;
    QString err;
    if (!OutlinePreset::parsePresetJson(f.readAll(), p, &err)) {
        QMessageBox::warning(widget(), title,
            isPolish ? QStringLiteral("Nieprawidłowy plik presetu:\n%1").arg(err)
                     : QStringLiteral("Invalid preset file:\n%1").arg(err));
        return;
    }

    if (p.name.isEmpty()) {
        p.name = QFileInfo(filePath).baseName();
    }
    p.isBuiltIn = false;

    const QString presetsDir = OutlinePreset::defaultPresetsDirectory();
    OutlinePreset::savePreset(presetsDir, p);
    refreshPresetsTable();

    // Select imported preset
    for (int r = 0; r < m_ui->tablePresets->rowCount(); ++r) {
        if (m_ui->tablePresets->item(r, 0)->text() == p.name) {
            m_ui->tablePresets->selectRow(r);
            break;
        }
    }

    QMessageBox::information(widget(), title,
        isPolish ? QStringLiteral("Preset '%1' został zaimportowany i dodany do listy presetów.").arg(p.name)
                 : QStringLiteral("Preset '%1' was successfully imported and added to the preset list.").arg(p.name));
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

    const bool trayEnabled = m_ui->kcfg_TrayIconEnabled->isChecked();
    const QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
    const QString desktopPath = autostartDir + QStringLiteral("/active-window-outline-tray.desktop");
    if (trayEnabled) {
        if (!QFile::exists(desktopPath)) {
            const QString systemDesktop = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, QStringLiteral("active-window-outline-tray.desktop"));
            if (!systemDesktop.isEmpty()) {
                QDir().mkpath(autostartDir);
                QFile::copy(systemDesktop, desktopPath);
            }
        }
        QProcess::startDetached(QStringLiteral("active-window-outline-tray"), {});
    } else {
        QFile::remove(desktopPath);
        QProcess::execute(QStringLiteral("killall"), {QStringLiteral("active-window-outline-tray")});
    }
}

#include "activewindowoutline_config.moc"
