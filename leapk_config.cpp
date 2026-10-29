/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk_config.h"
#include "ui_leapk_config.h"
#include "precedencehelper.h"
#include "presetactivationdialog.h"
#include "presethelper.h"
#include "shadereditordialog.h"
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
#include <QScrollBar>
#include <QSplitter>
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
        if (t.page == m_ui->tabEffects) {
            const int idx = m_ui->tabWidget->addTab(t.page, t.icon, t.text);
            if (!t.toolTip.isEmpty()) {
                m_ui->tabWidget->setTabToolTip(idx, t.toolTip);
            }
            continue;
        }
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

    auto kwinConfig = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    if (!kwinConfig->hasGroup(QStringLiteral("Effect-leapk")) && kwinConfig->hasGroup(QStringLiteral("Effect-active-window-outline"))) {
        KConfigGroup oldGrp = kwinConfig->group(QStringLiteral("Effect-active-window-outline"));
        KConfigGroup newGrp = kwinConfig->group(QStringLiteral("Effect-leapk"));
        oldGrp.copyTo(&newGrp);
        newGrp.sync();
    }

    m_configSkeleton = new KConfigSkeleton(QStringLiteral("kwinrc"), this);
    m_configSkeleton->setCurrentGroup(QStringLiteral("Effect-leapk"));

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
    m_configSkeleton->addItemBool(QStringLiteral("BadgeWindowBadgesEnabled"), *new bool, true);
    m_configSkeleton->addItemInt(QStringLiteral("BadgePosition"), *new int, 1);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudPosition"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeSize"), *new int, 28);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeMargin"), *new int, 0);
    m_configSkeleton->addItemBool(QStringLiteral("BadgeShowIcon"), *new bool, false);
    m_configSkeleton->addItemBool(QStringLiteral("BadgeShowThumbnail"), *new bool, false);
    m_configSkeleton->addItemBool(QStringLiteral("BadgeWindowShowIcon"), *new bool, false);
    m_configSkeleton->addItemBool(QStringLiteral("BadgeWindowShowThumbnail"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeWindowIconSize"), *new int, 20);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeWindowThumbnailSize"), *new int, 56);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeWindowThumbnailWidth"), *new int, 56);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeWindowThumbnailHeight"), *new int, 32);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeWindowLetterFontSize"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeWindowLabelFontSize"), *new int, 0);
    m_configSkeleton->addItemBool(QStringLiteral("BadgeHudShowIcon"), *new bool, true);
    m_configSkeleton->addItemBool(QStringLiteral("BadgeHudShowThumbnail"), *new bool, true);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudIconSize"), *new int, 24);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudThumbnailSize"), *new int, 80);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudThumbnailWidth"), *new int, 80);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudThumbnailHeight"), *new int, 48);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudLetterFontSize"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudCaptionFontSize"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudLocationFontSize"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudNavFontSize"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudHintsFontSize"), *new int, 0);
    m_configSkeleton->addItemBool(QStringLiteral("BadgeHudShowHints"), *new bool, true);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeJumpSize"), *new int, 34);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeIconSize"), *new int, 20);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeThumbnailWidth"), *new int, 56);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeThumbnailHeight"), *new int, 32);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeCornerRadius"), *new int, 6);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeOpacity"), *new int, 92);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeHudMarkColorMode"), *new int, 0);
    m_configSkeleton->addItemColor(QStringLiteral("BadgeDefaultColor"), *new QColor, QColor(QStringLiteral("#232629")));
    m_configSkeleton->addItemBool(QStringLiteral("BadgeLetterColorAuto"), *new bool, true);
    m_configSkeleton->addItemColor(QStringLiteral("BadgeLetterColor"), *new QColor, QColor(QStringLiteral("#ffffff")));
    m_configSkeleton->addItemBool(QStringLiteral("BadgeLabelColorAuto"), *new bool, true);
    m_configSkeleton->addItemColor(QStringLiteral("BadgeLabelUnmarkedColor"), *new QColor, QColor(QStringLiteral("#f1c40f")));
    m_configSkeleton->addItemBool(QStringLiteral("BadgeCaptionColorAuto"), *new bool, true);
    m_configSkeleton->addItemColor(QStringLiteral("BadgeCaptionColor"), *new QColor, QColor(QStringLiteral("#dce1e6")));
    m_configSkeleton->addItemBool(QStringLiteral("BadgeLocationColorAuto"), *new bool, true);
    m_configSkeleton->addItemColor(QStringLiteral("BadgeLocationColor"), *new QColor, QColor(QStringLiteral("#f1c40f")));
    m_configSkeleton->addItemInt(QStringLiteral("BadgeLetterFontSize"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeLabelFontSize"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeCaptionFontSize"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("BadgeLocationFontSize"), *new int, 0);
    m_configSkeleton->addItemString(QStringLiteral("BadgeLetterFont"), *new QString, QString());
    m_configSkeleton->addItemString(QStringLiteral("BadgeLabelFont"), *new QString, QString());
    m_configSkeleton->addItemString(QStringLiteral("BadgeCaptionFont"), *new QString, QString());
    m_configSkeleton->addItemString(QStringLiteral("BadgeLocationFont"), *new QString, QString());
    m_configSkeleton->addItemInt(QStringLiteral("JumpTargets"), *new int, 0);
    m_configSkeleton->addItemInt(QStringLiteral("CursorWarpMode"), *new int, 0);
    m_configSkeleton->addItemBool(QStringLiteral("SoundFeedbackEnabled"), *new bool, false);
    m_configSkeleton->addItemInt(QStringLiteral("SoundFeedbackVolume"), *new int, 30);

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

    m_configSkeleton->addItemBool(QStringLiteral("CustomShaderEnabled"), *new bool, false);
    m_configSkeleton->addItemString(QStringLiteral("ActiveCustomShaderId"), *new QString, QString());

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
    connect(m_ui->combo_BadgeLetterFont, &QFontComboBox::currentFontChanged, this, &ActiveWindowOutlineConfig::markAsChanged);
    connect(m_ui->combo_BadgeLabelFont, &QFontComboBox::currentFontChanged, this, &ActiveWindowOutlineConfig::markAsChanged);
    connect(m_ui->combo_BadgeCaptionFont, &QFontComboBox::currentFontChanged, this, &ActiveWindowOutlineConfig::markAsChanged);
    connect(m_ui->combo_BadgeLocationFont, &QFontComboBox::currentFontChanged, this, &ActiveWindowOutlineConfig::markAsChanged);
    connect(m_ui->kcfg_BadgeWindowBadgesEnabled, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    connect(m_ui->kcfg_BadgeWindowShowIcon, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    connect(m_ui->kcfg_BadgeWindowShowThumbnail, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    connect(m_ui->kcfg_BadgeHudShowIcon, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    connect(m_ui->kcfg_BadgeHudShowThumbnail, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    connect(m_ui->kcfg_BadgeLetterColorAuto, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    connect(m_ui->kcfg_BadgeLabelColorAuto, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    connect(m_ui->kcfg_BadgeCaptionColorAuto, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    connect(m_ui->kcfg_BadgeLocationColorAuto, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateMarkWindowUI);
    updateMarkWindowUI();

    auto onBadgeSettingChanged = [this]() {
        if (m_ui->check_previewWindowBadges->isChecked() || m_ui->check_previewHudBadges->isChecked()) {
            updateBadgePreview();
        }
    };
    connect(m_ui->check_previewWindowBadges, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateBadgePreview);
    connect(m_ui->check_previewHudBadges, &QCheckBox::toggled, this, &ActiveWindowOutlineConfig::updateBadgePreview);

    connect(m_ui->kcfg_BadgePosition, qOverload<int>(&QComboBox::currentIndexChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudPosition, qOverload<int>(&QComboBox::currentIndexChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeMargin, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeJumpSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeWindowBadgesEnabled, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeWindowShowIcon, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeWindowShowThumbnail, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeWindowIconSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeWindowThumbnailSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeWindowLetterFontSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeWindowLabelFontSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudShowIcon, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudShowThumbnail, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudShowHints, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudIconSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudThumbnailSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudLetterFontSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudCaptionFontSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudLocationFontSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudNavFontSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudHintsFontSize, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);

    connect(m_ui->kcfg_BadgeLetterColorAuto, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeLetterColor, &KColorButton::changed, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeLabelColorAuto, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeLabelUnmarkedColor, &KColorButton::changed, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeCaptionColorAuto, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeCaptionColor, &KColorButton::changed, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeLocationColorAuto, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeLocationColor, &KColorButton::changed, this, onBadgeSettingChanged);
    connect(m_ui->combo_BadgeLetterFont, &QFontComboBox::currentFontChanged, this, onBadgeSettingChanged);
    connect(m_ui->combo_BadgeLabelFont, &QFontComboBox::currentFontChanged, this, onBadgeSettingChanged);
    connect(m_ui->combo_BadgeCaptionFont, &QFontComboBox::currentFontChanged, this, onBadgeSettingChanged);
    connect(m_ui->combo_BadgeLocationFont, &QFontComboBox::currentFontChanged, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudIncludeAll, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudGroupWindows, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudGroupBorderWidth, &QDoubleSpinBox::valueChanged, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudGroupFontSize, &QSpinBox::valueChanged, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudGroupCornerRadius, &QSpinBox::valueChanged, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeHudGroupShaderEnabled, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeLivePreviews, &QCheckBox::toggled, this, onBadgeSettingChanged);
    connect(m_ui->kcfg_BadgeTimeout, qOverload<int>(&QSpinBox::valueChanged), this, onBadgeSettingChanged);
    connect(m_ui->kcfg_JumpRecentKey, &QKeySequenceEdit::keySequenceChanged, this, onBadgeSettingChanged);

    QString shortcutStr = QStringLiteral("Meta+Space");
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

    // Custom Shaders controls
    initShadersSection();
    connect(m_ui->btnToggleShader, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::toggleSelectedShader);
    connect(m_ui->btnNewShader, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::createNewShader);
    connect(m_ui->btnEditShader, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::editSelectedShader);
    connect(m_ui->btnDeleteShader, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::deleteSelectedShader);
    connect(m_ui->btnImportShader, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::importShader);
    connect(m_ui->btnExportShader, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::exportSelectedShader);
    connect(m_ui->tableShaders, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        editSelectedShader();
    });

    // Persistent Marks section
    initPersistentMarksSection();

    // Themes section
    initThemesTab();

    load();
}

ActiveWindowOutlineConfig::~ActiveWindowOutlineConfig()
{
    if (m_ui && (m_ui->check_previewWindowBadges->isChecked() || m_ui->check_previewHudBadges->isChecked())) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/LeapK"),
            QStringLiteral("org.kde.LeapK"),
            QStringLiteral("clearBadgePreview")
        );
        QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
    }
    delete m_ui;
}

void ActiveWindowOutlineConfig::updateBadgePreview()
{
    const bool showWin = m_ui->check_previewWindowBadges->isChecked();
    const bool showHud = m_ui->check_previewHudBadges->isChecked();
    const bool active = showWin || showHud;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/LeapK"),
        QStringLiteral("org.kde.LeapK"),
        QStringLiteral("setBadgePreview")
    );

    QString jsonStr;
    if (active) {
        const QJsonObject s = collectCurrentSettings();
        jsonStr = QString::fromUtf8(QJsonDocument(s).toJson(QJsonDocument::Compact));
    }

    msg << active << showWin << showHud << jsonStr;
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
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
    const bool windowBadgesEnabled = badgesEnabled && m_ui->kcfg_BadgeWindowBadgesEnabled->isChecked();
    m_ui->label_badgePosition->setEnabled(windowBadgesEnabled);
    m_ui->kcfg_BadgePosition->setEnabled(windowBadgesEnabled);
    m_ui->label_badgeHudPosition->setEnabled(badgesEnabled);
    m_ui->kcfg_BadgeHudPosition->setEnabled(badgesEnabled);
    m_ui->label_badgeSize->setEnabled(windowBadgesEnabled);
    m_ui->kcfg_BadgeSize->setEnabled(windowBadgesEnabled);
    m_ui->label_badgeMargin->setEnabled(windowBadgesEnabled);
    m_ui->kcfg_BadgeMargin->setEnabled(windowBadgesEnabled);
    m_ui->label_jumpTargets->setEnabled(badgesEnabled);
    m_ui->kcfg_JumpTargets->setEnabled(badgesEnabled);
    m_ui->label_cursorWarpMode->setEnabled(badgesEnabled);
    m_ui->kcfg_CursorWarpMode->setEnabled(badgesEnabled);
    m_ui->label_soundFeedbackEnabled->setEnabled(badgesEnabled);
    m_ui->kcfg_SoundFeedbackEnabled->setEnabled(badgesEnabled);
    m_ui->label_soundFeedbackVolume->setEnabled(badgesEnabled && m_ui->kcfg_SoundFeedbackEnabled->isChecked());
    m_ui->kcfg_SoundFeedbackVolume->setEnabled(badgesEnabled && m_ui->kcfg_SoundFeedbackEnabled->isChecked());

    m_ui->groupWindowBadges->setEnabled(badgesEnabled);
    m_ui->check_previewWindowBadges->setEnabled(windowBadgesEnabled);
    m_ui->kcfg_BadgeWindowShowIcon->setEnabled(windowBadgesEnabled);
    m_ui->kcfg_BadgeWindowShowThumbnail->setEnabled(windowBadgesEnabled);
    m_ui->groupHudBadges->setEnabled(badgesEnabled);

    m_ui->groupBadgeTypography->setEnabled(badgesEnabled);

    const bool showWindowIcon = windowBadgesEnabled && m_ui->kcfg_BadgeWindowShowIcon->isChecked();
    m_ui->label_badgeWindowIconSize->setEnabled(showWindowIcon);
    m_ui->kcfg_BadgeWindowIconSize->setEnabled(showWindowIcon);

    const bool showWindowThumb = windowBadgesEnabled && m_ui->kcfg_BadgeWindowShowThumbnail->isChecked();
    m_ui->label_badgeWindowThumbnailSize->setEnabled(showWindowThumb);
    m_ui->kcfg_BadgeWindowThumbnailSize->setEnabled(showWindowThumb);

    const bool showHudIcon = badgesEnabled && m_ui->kcfg_BadgeHudShowIcon->isChecked();
    m_ui->label_badgeHudIconSize->setEnabled(showHudIcon);
    m_ui->kcfg_BadgeHudIconSize->setEnabled(showHudIcon);

    const bool showHudThumb = badgesEnabled && m_ui->kcfg_BadgeHudShowThumbnail->isChecked();
    m_ui->label_badgeHudThumbnailSize->setEnabled(showHudThumb);
    m_ui->kcfg_BadgeHudThumbnailSize->setEnabled(showHudThumb);

    m_ui->kcfg_BadgeLetterColor->setEnabled(badgesEnabled && !m_ui->kcfg_BadgeLetterColorAuto->isChecked());
    m_ui->kcfg_BadgeLabelUnmarkedColor->setEnabled(badgesEnabled && !m_ui->kcfg_BadgeLabelColorAuto->isChecked());
    m_ui->kcfg_BadgeCaptionColor->setEnabled(badgesEnabled && !m_ui->kcfg_BadgeCaptionColorAuto->isChecked());
    m_ui->kcfg_BadgeLocationColor->setEnabled(badgesEnabled && !m_ui->kcfg_BadgeLocationColorAuto->isChecked());
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
    const QString title = QStringLiteral("Ambilight Parameters & Presets Guide");

    const QString html = QStringLiteral(R"raw(
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
    auto cfg = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup group = cfg->hasGroup(QStringLiteral("Effect-leapk"))
        ? cfg->group(QStringLiteral("Effect-leapk"))
        : cfg->group(QStringLiteral("Effect-active-window-outline"));
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
    const QString letterFont = group.readEntry("BadgeLetterFont", QString());
    if (!letterFont.trimmed().isEmpty()) {
        m_ui->combo_BadgeLetterFont->setCurrentFont(QFont(letterFont.trimmed()));
    }
    const QString labelFont = group.readEntry("BadgeLabelFont", QString());
    if (!labelFont.trimmed().isEmpty()) {
        m_ui->combo_BadgeLabelFont->setCurrentFont(QFont(labelFont.trimmed()));
    }
    const QString capFont = group.readEntry("BadgeCaptionFont", QString());
    if (!capFont.trimmed().isEmpty()) {
        m_ui->combo_BadgeCaptionFont->setCurrentFont(QFont(capFont.trimmed()));
    }
    const QString locFont = group.readEntry("BadgeLocationFont", QString());
    if (!locFont.trimmed().isEmpty()) {
        m_ui->combo_BadgeLocationFont->setCurrentFont(QFont(locFont.trimmed()));
    }
    m_ui->kcfg_BadgeWindowBadgesEnabled->setChecked(group.readEntry("BadgeWindowBadgesEnabled", true));
    m_ui->kcfg_BadgeHudIncludeAll->setChecked(group.readEntry("BadgeHudIncludeAll", false));
    m_ui->kcfg_BadgeHudGroupWindows->setChecked(group.readEntry("BadgeHudGroupWindows", true));
    m_ui->kcfg_BadgeHudGroupBorderWidth->setValue(group.readEntry("BadgeHudGroupBorderWidth", 2.0));
    m_ui->kcfg_BadgeHudGroupFontSize->setValue(group.readEntry("BadgeHudGroupFontSize", 0));
    m_ui->kcfg_BadgeHudGroupCornerRadius->setValue(group.readEntry("BadgeHudGroupCornerRadius", 8));
    m_ui->kcfg_BadgeHudGroupShaderEnabled->setChecked(group.readEntry("BadgeHudGroupShaderEnabled", true));
    refreshPersistentMarksTable();
    m_ui->kcfg_BadgeLivePreviews->setChecked(group.readEntry("BadgeLivePreviews", false));
    m_ui->kcfg_BadgeTimeout->setValue(group.readEntry("BadgeTimeout", 5));
    m_ui->kcfg_JumpRecentKey->setKeySequence(QKeySequence(group.readEntry("JumpRecentKey", QStringLiteral("Space"))));
    updateRecentWindowUI();
    updateMarkWindowUI();
    m_activeShaderId = group.readEntry(QStringLiteral("ActiveCustomShaderId"), QString());
    m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
    refreshShadersTable();

    m_activeThemeId = group.readEntry(QStringLiteral("ActiveThemeId"), QStringLiteral("default-breeze"));
    const QString themeParamsStr = group.readEntry(QStringLiteral("ActiveThemeParams"), QString());
    if (!themeParamsStr.isEmpty()) {
        m_themeUserOverrides = QJsonDocument::fromJson(themeParamsStr.toUtf8()).object();
    } else {
        m_themeUserOverrides = QJsonObject();
    }
    m_currentDisplayedThemeId.clear();
    refreshThemesTable();
    updateThemeParametersUI();

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
    m_ui->kcfg_BadgeJumpSize->setValue(34);
    m_ui->kcfg_BadgeWindowShowIcon->setChecked(false);
    m_ui->kcfg_BadgeWindowShowThumbnail->setChecked(false);
    m_ui->kcfg_BadgeWindowIconSize->setValue(20);
    m_ui->kcfg_BadgeWindowThumbnailSize->setValue(56);
    m_ui->kcfg_BadgeWindowLetterFontSize->setValue(0);
    m_ui->kcfg_BadgeWindowLabelFontSize->setValue(0);
    m_ui->kcfg_BadgeHudShowIcon->setChecked(true);
    m_ui->kcfg_BadgeHudShowThumbnail->setChecked(true);
    m_ui->kcfg_BadgeHudShowHints->setChecked(true);
    m_ui->kcfg_BadgeHudIconSize->setValue(24);
    m_ui->kcfg_BadgeHudThumbnailSize->setValue(80);
    m_ui->kcfg_BadgeHudLetterFontSize->setValue(0);
    m_ui->kcfg_BadgeHudCaptionFontSize->setValue(0);
    m_ui->kcfg_BadgeHudLocationFontSize->setValue(0);
    m_ui->kcfg_BadgeHudNavFontSize->setValue(0);
    m_ui->kcfg_BadgeHudHintsFontSize->setValue(0);

    m_ui->kcfg_BadgeLetterColorAuto->setChecked(true);
    m_ui->kcfg_BadgeLetterColor->setColor(QColor(QStringLiteral("#ffffff")));
    m_ui->kcfg_BadgeLabelColorAuto->setChecked(true);
    m_ui->kcfg_BadgeLabelUnmarkedColor->setColor(QColor(QStringLiteral("#f1c40f")));
    m_ui->kcfg_BadgeCaptionColorAuto->setChecked(true);
    m_ui->kcfg_BadgeCaptionColor->setColor(QColor(QStringLiteral("#dce1e6")));
    m_ui->kcfg_BadgeLocationColorAuto->setChecked(true);
    m_ui->kcfg_BadgeLocationColor->setColor(QColor(QStringLiteral("#f1c40f")));
    m_ui->kcfg_BadgeLetterFontSize->setValue(0);
    m_ui->kcfg_BadgeLabelFontSize->setValue(0);
    m_ui->kcfg_BadgeCaptionFontSize->setValue(0);
    m_ui->kcfg_BadgeLocationFontSize->setValue(0);
    m_ui->kcfg_BadgeWindowBadgesEnabled->setChecked(true);
    m_ui->kcfg_BadgeHudIncludeAll->setChecked(false);
    m_ui->kcfg_BadgeHudGroupWindows->setChecked(true);
    m_ui->kcfg_BadgeHudGroupBorderWidth->setValue(2.0);
    m_ui->kcfg_BadgeHudGroupFontSize->setValue(0);
    m_ui->kcfg_BadgeHudGroupCornerRadius->setValue(8);
    m_ui->kcfg_BadgeHudGroupShaderEnabled->setChecked(true);
    m_ui->kcfg_BadgeLivePreviews->setChecked(false);
    m_ui->kcfg_BadgeTimeout->setValue(5);
    m_ui->kcfg_JumpRecentKey->setKeySequence(QKeySequence(QStringLiteral("Space")));
    m_ui->combo_BadgeLetterFont->setCurrentFont(QFont(QStringLiteral("sans-serif")));
    m_ui->combo_BadgeLabelFont->setCurrentFont(QFont(QStringLiteral("sans-serif")));
    m_ui->combo_BadgeCaptionFont->setCurrentFont(QFont(QStringLiteral("sans-serif")));
    m_ui->combo_BadgeLocationFont->setCurrentFont(QFont(QStringLiteral("sans-serif")));
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
    m_activeThemeId = QStringLiteral("default-breeze");
    m_themeUserOverrides = QJsonObject();
    m_currentDisplayedThemeId.clear();
    refreshThemesTable();
    updateThemeParametersUI();
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

        if (!hasUpdate) {
            m_ui->label_updateStatus->setText(QStringLiteral("✓ Up to date (v%1)").arg(currentVerStr));
            m_ui->label_updateStatus->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));

            LeapK::showUpToDateDialog(widget(), currentVerStr, htmlUrl);
            return;
        }

        // New version available!
        m_ui->label_updateStatus->setText(QStringLiteral("⬆ New version: %1!").arg(tagName));
        m_ui->label_updateStatus->setStyleSheet(QStringLiteral("color: #3daee9; font-weight: bold;"));

        LeapK::showNewVersionDialog(widget(), tagName, currentVerStr, isPrerelease, htmlUrl, body);
    });
}

void ActiveWindowOutlineConfig::initPresetsTab()
{
    m_ui->tablePresets->setColumnCount(4);
    m_ui->tablePresets->setHorizontalHeaderLabels({
        QStringLiteral("Preset Name"),
        QStringLiteral("Description"),
        QStringLiteral("Type"),
        QStringLiteral("Export")
    });
    m_ui->tablePresets->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui->tablePresets->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_ui->tablePresets->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui->tablePresets->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_ui->tablePresets->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->tablePresets->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ui->tablePresets->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ui->tablePresets->verticalHeader()->setVisible(false);

    m_ui->tablePresets->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui->tablePresets, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const int row = m_ui->tablePresets->rowAt(pos.y());
        if (row < 0) return;
        m_ui->tablePresets->selectRow(row);

        QMenu menu(widget());
        QAction *actActivate = menu.addAction(QIcon::fromTheme(QStringLiteral("dialog-ok")), QStringLiteral("Activate Preset…"));
        menu.addSeparator();
        QAction *actEdit = menu.addAction(QIcon::fromTheme(QStringLiteral("document-edit")), QStringLiteral("Edit Name & Description…"));
        QAction *actDuplicate = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), QStringLiteral("Duplicate…"));
        QAction *actDelete = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), QStringLiteral("Delete"));
        menu.addSeparator();
        QAction *actExport = menu.addAction(QIcon::fromTheme(QStringLiteral("document-export")), QStringLiteral("Export…"));

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

        QString typeStr = p.isBuiltIn ? QStringLiteral("Built-in") : QStringLiteral("User");
        auto *item2 = new QTableWidgetItem(typeStr);
        item2->setTextAlignment(Qt::AlignCenter);
        m_ui->tablePresets->setItem(row, 2, item2);

        QPushButton *btnExport = new QPushButton(QIcon::fromTheme(QStringLiteral("document-export")), QStringLiteral("Export"));
        btnExport->setToolTip(QStringLiteral("Export preset to JSON file"));
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
    s[QStringLiteral("BadgeWindowBadgesEnabled")] = m_ui->kcfg_BadgeWindowBadgesEnabled->isChecked();
    s[QStringLiteral("MarkShowLetterBadges")] = (badgeMode != 2);
    s[QStringLiteral("BadgesAlwaysVisible")] = (badgeMode == 1);
    s[QStringLiteral("BadgePosition")] = m_ui->kcfg_BadgePosition->currentIndex();
    s[QStringLiteral("BadgeHudPosition")] = m_ui->kcfg_BadgeHudPosition->currentIndex();
    s[QStringLiteral("BadgeSize")] = m_ui->kcfg_BadgeSize->value();
    s[QStringLiteral("BadgeMargin")] = m_ui->kcfg_BadgeMargin->value();
    s[QStringLiteral("BadgeShowIcon")] = m_ui->kcfg_BadgeWindowShowIcon->isChecked() || m_ui->kcfg_BadgeHudShowIcon->isChecked();
    s[QStringLiteral("BadgeShowThumbnail")] = m_ui->kcfg_BadgeWindowShowThumbnail->isChecked() || m_ui->kcfg_BadgeHudShowThumbnail->isChecked();
    s[QStringLiteral("BadgeWindowShowIcon")] = m_ui->kcfg_BadgeWindowShowIcon->isChecked();
    s[QStringLiteral("BadgeWindowShowThumbnail")] = m_ui->kcfg_BadgeWindowShowThumbnail->isChecked();
    s[QStringLiteral("BadgeWindowIconSize")] = m_ui->kcfg_BadgeWindowIconSize->value();
    const int winThumbSize = m_ui->kcfg_BadgeWindowThumbnailSize->value();
    s[QStringLiteral("BadgeWindowThumbnailSize")] = winThumbSize;
    s[QStringLiteral("BadgeWindowThumbnailWidth")] = winThumbSize;
    s[QStringLiteral("BadgeWindowThumbnailHeight")] = winThumbSize;
    s[QStringLiteral("BadgeWindowLetterFontSize")] = m_ui->kcfg_BadgeWindowLetterFontSize->value();
    s[QStringLiteral("BadgeWindowLabelFontSize")] = m_ui->kcfg_BadgeWindowLabelFontSize->value();
    s[QStringLiteral("BadgeHudShowIcon")] = m_ui->kcfg_BadgeHudShowIcon->isChecked();
    s[QStringLiteral("BadgeHudShowThumbnail")] = m_ui->kcfg_BadgeHudShowThumbnail->isChecked();
    s[QStringLiteral("BadgeHudIconSize")] = m_ui->kcfg_BadgeHudIconSize->value();
    const int hudThumbSize = m_ui->kcfg_BadgeHudThumbnailSize->value();
    s[QStringLiteral("BadgeHudThumbnailSize")] = hudThumbSize;
    s[QStringLiteral("BadgeHudThumbnailWidth")] = hudThumbSize;
    s[QStringLiteral("BadgeHudThumbnailHeight")] = hudThumbSize;
    s[QStringLiteral("BadgeHudLetterFontSize")] = m_ui->kcfg_BadgeHudLetterFontSize->value();
    s[QStringLiteral("BadgeHudCaptionFontSize")] = m_ui->kcfg_BadgeHudCaptionFontSize->value();
    s[QStringLiteral("BadgeHudLocationFontSize")] = m_ui->kcfg_BadgeHudLocationFontSize->value();
    s[QStringLiteral("BadgeHudNavFontSize")] = m_ui->kcfg_BadgeHudNavFontSize->value();
    s[QStringLiteral("BadgeHudHintsFontSize")] = m_ui->kcfg_BadgeHudHintsFontSize->value();
    s[QStringLiteral("BadgeHudShowHints")] = m_ui->kcfg_BadgeHudShowHints->isChecked();
    s[QStringLiteral("JumpTargets")] = m_ui->kcfg_JumpTargets->currentIndex();
    s[QStringLiteral("CursorWarpMode")] = m_ui->kcfg_CursorWarpMode->currentIndex();
    s[QStringLiteral("SoundFeedbackEnabled")] = m_ui->kcfg_SoundFeedbackEnabled->isChecked();
    s[QStringLiteral("SoundFeedbackVolume")] = m_ui->kcfg_SoundFeedbackVolume->value();
    s[QStringLiteral("BadgeJumpSize")] = m_ui->kcfg_BadgeJumpSize->value();
    s[QStringLiteral("BadgeIconSize")] = m_ui->kcfg_BadgeWindowIconSize->value();
    s[QStringLiteral("BadgeThumbnailWidth")] = winThumbSize;
    s[QStringLiteral("BadgeThumbnailHeight")] = winThumbSize;

    s[QStringLiteral("BadgeLetterColorAuto")] = m_ui->kcfg_BadgeLetterColorAuto->isChecked();
    s[QStringLiteral("BadgeLetterColor")] = m_ui->kcfg_BadgeLetterColor->color().name(QColor::HexRgb);
    s[QStringLiteral("BadgeLabelColorAuto")] = m_ui->kcfg_BadgeLabelColorAuto->isChecked();
    s[QStringLiteral("BadgeLabelUnmarkedColor")] = m_ui->kcfg_BadgeLabelUnmarkedColor->color().name(QColor::HexRgb);
    s[QStringLiteral("BadgeCaptionColorAuto")] = m_ui->kcfg_BadgeCaptionColorAuto->isChecked();
    s[QStringLiteral("BadgeCaptionColor")] = m_ui->kcfg_BadgeCaptionColor->color().name(QColor::HexRgb);
    s[QStringLiteral("BadgeLocationColorAuto")] = m_ui->kcfg_BadgeLocationColorAuto->isChecked();
    s[QStringLiteral("BadgeLocationColor")] = m_ui->kcfg_BadgeLocationColor->color().name(QColor::HexRgb);
    s[QStringLiteral("BadgeLetterFontSize")] = m_ui->kcfg_BadgeLetterFontSize->value();
    s[QStringLiteral("BadgeLabelFontSize")] = m_ui->kcfg_BadgeLabelFontSize->value();
    s[QStringLiteral("BadgeCaptionFontSize")] = m_ui->kcfg_BadgeCaptionFontSize->value();
    s[QStringLiteral("BadgeLocationFontSize")] = m_ui->kcfg_BadgeLocationFontSize->value();
    s[QStringLiteral("BadgeLetterFont")] = m_ui->combo_BadgeLetterFont->currentFont().family();
    s[QStringLiteral("BadgeLabelFont")] = m_ui->combo_BadgeLabelFont->currentFont().family();
    s[QStringLiteral("BadgeCaptionFont")] = m_ui->combo_BadgeCaptionFont->currentFont().family();
    s[QStringLiteral("BadgeLocationFont")] = m_ui->combo_BadgeLocationFont->currentFont().family();
    s[QStringLiteral("BadgeHudIncludeAll")] = m_ui->kcfg_BadgeHudIncludeAll->isChecked();
    s[QStringLiteral("BadgeHudGroupWindows")] = m_ui->kcfg_BadgeHudGroupWindows->isChecked();
    s[QStringLiteral("BadgeHudGroupBorderWidth")] = m_ui->kcfg_BadgeHudGroupBorderWidth->value();
    s[QStringLiteral("BadgeHudGroupFontSize")] = m_ui->kcfg_BadgeHudGroupFontSize->value();
    s[QStringLiteral("BadgeHudGroupCornerRadius")] = m_ui->kcfg_BadgeHudGroupCornerRadius->value();
    s[QStringLiteral("BadgeHudGroupShaderEnabled")] = m_ui->kcfg_BadgeHudGroupShaderEnabled->isChecked();
    s[QStringLiteral("BadgeLivePreviews")] = m_ui->kcfg_BadgeLivePreviews->isChecked();
    s[QStringLiteral("BadgeTimeout")] = m_ui->kcfg_BadgeTimeout->value();
    s[QStringLiteral("JumpRecentKey")] = m_ui->kcfg_JumpRecentKey->keySequence().toString();

    // Color Cycling
    s[QStringLiteral("ColorCycleEnabled")] = m_ui->kcfg_ColorCycleEnabled->isChecked();
    s[QStringLiteral("ColorTransitionDuration")] = m_ui->kcfg_ColorTransitionDuration->value();
    s[QStringLiteral("ColorCycleOrder")] = m_ui->kcfg_ColorCycleOrder->currentIndex();
    s[QStringLiteral("ColorCycleEasing")] = m_ui->kcfg_ColorCycleEasing->currentIndex();
    s[QStringLiteral("ColorList")] = serializeColorTable();

    // Shader Effects
    const bool shaderOn = m_ui->kcfg_CustomShaderEnabled->isChecked();
    s[QStringLiteral("CustomShaderEnabled")] = shaderOn;
    s[QStringLiteral("ActiveCustomShaderId")] = m_activeShaderId;

    if (!m_activeShaderId.isEmpty()) {
        s[QStringLiteral("ShaderEffect")] = LeapKShader::createShaderBundle(m_activeShaderId, shaderOn, m_shaders);
    }

    s[QStringLiteral("BreatheEnabled")] = (shaderOn && m_activeShaderId == QStringLiteral("breathe_pulse"));
    s[QStringLiteral("GlitchEnabled")] = (shaderOn && m_activeShaderId == QStringLiteral("futuristic_glitch"));
    s[QStringLiteral("NeonFlickerEnabled")] = (shaderOn && m_activeShaderId == QStringLiteral("neon_flicker"));
    s[QStringLiteral("FlashEnabled")] = (shaderOn && m_activeShaderId == QStringLiteral("flash_strobe"));

    for (const auto &sh : std::as_const(m_shaders)) {
        if (sh.id == QStringLiteral("breathe_pulse")) {
            for (const auto &p : sh.parameters) {
                if (p.id == QStringLiteral("speed")) s[QStringLiteral("BreatheSpeed")] = p.currentValue;
                else if (p.id == QStringLiteral("intensity")) s[QStringLiteral("BreatheIntensity")] = static_cast<int>(p.currentValue);
            }
        } else if (sh.id == QStringLiteral("futuristic_glitch")) {
            for (const auto &p : sh.parameters) {
                if (p.id == QStringLiteral("speed")) s[QStringLiteral("GlitchSpeed")] = p.currentValue;
                else if (p.id == QStringLiteral("intensity")) s[QStringLiteral("GlitchIntensity")] = static_cast<int>(p.currentValue);
                else if (p.id == QStringLiteral("anomalyEnabled")) s[QStringLiteral("GlitchAnomalyEnabled")] = (p.currentValue > 0.5);
                else if (p.id == QStringLiteral("frequency")) s[QStringLiteral("GlitchFrequency")] = static_cast<int>(p.currentValue);
            }
        } else if (sh.id == QStringLiteral("neon_flicker")) {
            for (const auto &p : sh.parameters) {
                if (p.id == QStringLiteral("interval")) s[QStringLiteral("NeonFlickerInterval")] = p.currentValue;
                else if (p.id == QStringLiteral("duration")) s[QStringLiteral("NeonFlickerDuration")] = p.currentValue;
                else if (p.id == QStringLiteral("intensity")) s[QStringLiteral("NeonFlickerIntensity")] = static_cast<int>(p.currentValue);
            }
        } else if (sh.id == QStringLiteral("flash_strobe")) {
            for (const auto &p : sh.parameters) {
                if (p.id == QStringLiteral("interval")) s[QStringLiteral("FlashInterval")] = p.currentValue;
                else if (p.id == QStringLiteral("duration")) s[QStringLiteral("FlashDuration")] = p.currentValue;
            }
        }
    }

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
    if (s.contains(QStringLiteral("BadgeWindowBadgesEnabled"))) m_ui->kcfg_BadgeWindowBadgesEnabled->setChecked(s[QStringLiteral("BadgeWindowBadgesEnabled")].toBool());
    if (s.contains(QStringLiteral("BadgePosition"))) m_ui->kcfg_BadgePosition->setCurrentIndex(s[QStringLiteral("BadgePosition")].toInt());
    if (s.contains(QStringLiteral("BadgeHudPosition"))) m_ui->kcfg_BadgeHudPosition->setCurrentIndex(s[QStringLiteral("BadgeHudPosition")].toInt());
    if (s.contains(QStringLiteral("BadgeSize"))) m_ui->kcfg_BadgeSize->setValue(s[QStringLiteral("BadgeSize")].toInt());
    if (s.contains(QStringLiteral("BadgeMargin"))) m_ui->kcfg_BadgeMargin->setValue(s[QStringLiteral("BadgeMargin")].toInt());
    if (s.contains(QStringLiteral("BadgeShowIcon"))) {
        const bool iconVal = s[QStringLiteral("BadgeShowIcon")].toBool();
        m_ui->kcfg_BadgeWindowShowIcon->setChecked(s.contains(QStringLiteral("BadgeWindowShowIcon")) ? s[QStringLiteral("BadgeWindowShowIcon")].toBool() : iconVal);
        m_ui->kcfg_BadgeHudShowIcon->setChecked(s.contains(QStringLiteral("BadgeHudShowIcon")) ? s[QStringLiteral("BadgeHudShowIcon")].toBool() : true);
    }
    if (s.contains(QStringLiteral("BadgeShowThumbnail"))) {
        const bool thumbVal = s[QStringLiteral("BadgeShowThumbnail")].toBool();
        m_ui->kcfg_BadgeWindowShowThumbnail->setChecked(s.contains(QStringLiteral("BadgeWindowShowThumbnail")) ? s[QStringLiteral("BadgeWindowShowThumbnail")].toBool() : thumbVal);
        m_ui->kcfg_BadgeHudShowThumbnail->setChecked(s.contains(QStringLiteral("BadgeHudShowThumbnail")) ? s[QStringLiteral("BadgeHudShowThumbnail")].toBool() : true);
    }
    if (s.contains(QStringLiteral("BadgeWindowShowIcon"))) m_ui->kcfg_BadgeWindowShowIcon->setChecked(s[QStringLiteral("BadgeWindowShowIcon")].toBool());
    if (s.contains(QStringLiteral("BadgeWindowShowThumbnail"))) m_ui->kcfg_BadgeWindowShowThumbnail->setChecked(s[QStringLiteral("BadgeWindowShowThumbnail")].toBool());
    if (s.contains(QStringLiteral("BadgeWindowIconSize"))) m_ui->kcfg_BadgeWindowIconSize->setValue(s[QStringLiteral("BadgeWindowIconSize")].toInt());
    if (s.contains(QStringLiteral("BadgeWindowThumbnailSize"))) {
        m_ui->kcfg_BadgeWindowThumbnailSize->setValue(s[QStringLiteral("BadgeWindowThumbnailSize")].toInt());
    } else if (s.contains(QStringLiteral("BadgeWindowThumbnailWidth"))) {
        m_ui->kcfg_BadgeWindowThumbnailSize->setValue(s[QStringLiteral("BadgeWindowThumbnailWidth")].toInt());
    }
    if (s.contains(QStringLiteral("BadgeWindowLetterFontSize"))) m_ui->kcfg_BadgeWindowLetterFontSize->setValue(s[QStringLiteral("BadgeWindowLetterFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeWindowLabelFontSize"))) m_ui->kcfg_BadgeWindowLabelFontSize->setValue(s[QStringLiteral("BadgeWindowLabelFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeHudShowIcon"))) m_ui->kcfg_BadgeHudShowIcon->setChecked(s[QStringLiteral("BadgeHudShowIcon")].toBool());
    if (s.contains(QStringLiteral("BadgeHudShowThumbnail"))) m_ui->kcfg_BadgeHudShowThumbnail->setChecked(s[QStringLiteral("BadgeHudShowThumbnail")].toBool());
    if (s.contains(QStringLiteral("BadgeHudIconSize"))) m_ui->kcfg_BadgeHudIconSize->setValue(s[QStringLiteral("BadgeHudIconSize")].toInt());
    if (s.contains(QStringLiteral("BadgeHudThumbnailSize"))) {
        m_ui->kcfg_BadgeHudThumbnailSize->setValue(s[QStringLiteral("BadgeHudThumbnailSize")].toInt());
    } else if (s.contains(QStringLiteral("BadgeHudThumbnailWidth"))) {
        m_ui->kcfg_BadgeHudThumbnailSize->setValue(s[QStringLiteral("BadgeHudThumbnailWidth")].toInt());
    }
    if (s.contains(QStringLiteral("BadgeHudLetterFontSize"))) m_ui->kcfg_BadgeHudLetterFontSize->setValue(s[QStringLiteral("BadgeHudLetterFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeHudCaptionFontSize"))) m_ui->kcfg_BadgeHudCaptionFontSize->setValue(s[QStringLiteral("BadgeHudCaptionFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeHudLocationFontSize"))) m_ui->kcfg_BadgeHudLocationFontSize->setValue(s[QStringLiteral("BadgeHudLocationFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeHudNavFontSize"))) m_ui->kcfg_BadgeHudNavFontSize->setValue(s[QStringLiteral("BadgeHudNavFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeHudHintsFontSize"))) m_ui->kcfg_BadgeHudHintsFontSize->setValue(s[QStringLiteral("BadgeHudHintsFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeHudShowHints"))) m_ui->kcfg_BadgeHudShowHints->setChecked(s[QStringLiteral("BadgeHudShowHints")].toBool());
    if (s.contains(QStringLiteral("JumpTargets"))) m_ui->kcfg_JumpTargets->setCurrentIndex(s[QStringLiteral("JumpTargets")].toInt());
    if (s.contains(QStringLiteral("CursorWarpMode"))) m_ui->kcfg_CursorWarpMode->setCurrentIndex(s[QStringLiteral("CursorWarpMode")].toInt());
    if (s.contains(QStringLiteral("SoundFeedbackEnabled"))) m_ui->kcfg_SoundFeedbackEnabled->setChecked(s[QStringLiteral("SoundFeedbackEnabled")].toBool());
    if (s.contains(QStringLiteral("SoundFeedbackVolume"))) m_ui->kcfg_SoundFeedbackVolume->setValue(s[QStringLiteral("SoundFeedbackVolume")].toInt());
    if (s.contains(QStringLiteral("BadgeJumpSize"))) m_ui->kcfg_BadgeJumpSize->setValue(s[QStringLiteral("BadgeJumpSize")].toInt());
    if (s.contains(QStringLiteral("BadgeIconSize"))) {
        const int iconSz = s[QStringLiteral("BadgeIconSize")].toInt();
        m_ui->kcfg_BadgeWindowIconSize->setValue(iconSz);
        m_ui->kcfg_BadgeHudIconSize->setValue(iconSz);
    }
    if (s.contains(QStringLiteral("BadgeThumbnailWidth"))) {
        const int tw = s[QStringLiteral("BadgeThumbnailWidth")].toInt();
        m_ui->kcfg_BadgeWindowThumbnailSize->setValue(tw);
        m_ui->kcfg_BadgeHudThumbnailSize->setValue(tw);
    }

    if (s.contains(QStringLiteral("BadgeLetterColorAuto"))) m_ui->kcfg_BadgeLetterColorAuto->setChecked(s[QStringLiteral("BadgeLetterColorAuto")].toBool());
    if (s.contains(QStringLiteral("BadgeLetterColor"))) m_ui->kcfg_BadgeLetterColor->setColor(QColor(s[QStringLiteral("BadgeLetterColor")].toString()));
    if (s.contains(QStringLiteral("BadgeLabelColorAuto"))) m_ui->kcfg_BadgeLabelColorAuto->setChecked(s[QStringLiteral("BadgeLabelColorAuto")].toBool());
    if (s.contains(QStringLiteral("BadgeLabelUnmarkedColor"))) m_ui->kcfg_BadgeLabelUnmarkedColor->setColor(QColor(s[QStringLiteral("BadgeLabelUnmarkedColor")].toString()));
    if (s.contains(QStringLiteral("BadgeCaptionColorAuto"))) m_ui->kcfg_BadgeCaptionColorAuto->setChecked(s[QStringLiteral("BadgeCaptionColorAuto")].toBool());
    if (s.contains(QStringLiteral("BadgeCaptionColor"))) m_ui->kcfg_BadgeCaptionColor->setColor(QColor(s[QStringLiteral("BadgeCaptionColor")].toString()));
    if (s.contains(QStringLiteral("BadgeLocationColorAuto"))) m_ui->kcfg_BadgeLocationColorAuto->setChecked(s[QStringLiteral("BadgeLocationColorAuto")].toBool());
    if (s.contains(QStringLiteral("BadgeLocationColor"))) m_ui->kcfg_BadgeLocationColor->setColor(QColor(s[QStringLiteral("BadgeLocationColor")].toString()));
    if (s.contains(QStringLiteral("BadgeLetterFontSize"))) m_ui->kcfg_BadgeLetterFontSize->setValue(s[QStringLiteral("BadgeLetterFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeLabelFontSize"))) m_ui->kcfg_BadgeLabelFontSize->setValue(s[QStringLiteral("BadgeLabelFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeCaptionFontSize"))) m_ui->kcfg_BadgeCaptionFontSize->setValue(s[QStringLiteral("BadgeCaptionFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeLocationFontSize"))) m_ui->kcfg_BadgeLocationFontSize->setValue(s[QStringLiteral("BadgeLocationFontSize")].toInt());
    if (s.contains(QStringLiteral("BadgeLetterFont"))) m_ui->combo_BadgeLetterFont->setCurrentFont(QFont(s[QStringLiteral("BadgeLetterFont")].toString()));
    if (s.contains(QStringLiteral("BadgeLabelFont"))) m_ui->combo_BadgeLabelFont->setCurrentFont(QFont(s[QStringLiteral("BadgeLabelFont")].toString()));
    if (s.contains(QStringLiteral("BadgeCaptionFont"))) m_ui->combo_BadgeCaptionFont->setCurrentFont(QFont(s[QStringLiteral("BadgeCaptionFont")].toString()));
    if (s.contains(QStringLiteral("BadgeLocationFont"))) m_ui->combo_BadgeLocationFont->setCurrentFont(QFont(s[QStringLiteral("BadgeLocationFont")].toString()));
    if (s.contains(QStringLiteral("BadgeHudIncludeAll"))) m_ui->kcfg_BadgeHudIncludeAll->setChecked(s[QStringLiteral("BadgeHudIncludeAll")].toBool());
    if (s.contains(QStringLiteral("BadgeHudGroupWindows"))) m_ui->kcfg_BadgeHudGroupWindows->setChecked(s[QStringLiteral("BadgeHudGroupWindows")].toBool());
    if (s.contains(QStringLiteral("BadgeHudGroupBorderWidth"))) m_ui->kcfg_BadgeHudGroupBorderWidth->setValue(s[QStringLiteral("BadgeHudGroupBorderWidth")].toDouble(2.0));
    if (s.contains(QStringLiteral("BadgeHudGroupFontSize"))) m_ui->kcfg_BadgeHudGroupFontSize->setValue(s[QStringLiteral("BadgeHudGroupFontSize")].toInt(0));
    if (s.contains(QStringLiteral("BadgeHudGroupCornerRadius"))) m_ui->kcfg_BadgeHudGroupCornerRadius->setValue(s[QStringLiteral("BadgeHudGroupCornerRadius")].toInt(8));
    if (s.contains(QStringLiteral("BadgeHudGroupShaderEnabled"))) m_ui->kcfg_BadgeHudGroupShaderEnabled->setChecked(s[QStringLiteral("BadgeHudGroupShaderEnabled")].toBool(true));
    if (s.contains(QStringLiteral("BadgeLivePreviews"))) m_ui->kcfg_BadgeLivePreviews->setChecked(s[QStringLiteral("BadgeLivePreviews")].toBool());
    if (s.contains(QStringLiteral("BadgeTimeout"))) m_ui->kcfg_BadgeTimeout->setValue(s[QStringLiteral("BadgeTimeout")].toInt());
    if (s.contains(QStringLiteral("JumpRecentKey"))) m_ui->kcfg_JumpRecentKey->setKeySequence(QKeySequence(s[QStringLiteral("JumpRecentKey")].toString()));

    // Color Cycling
    if (s.contains(QStringLiteral("ColorCycleEnabled"))) m_ui->kcfg_ColorCycleEnabled->setChecked(s[QStringLiteral("ColorCycleEnabled")].toBool());
    if (s.contains(QStringLiteral("ColorTransitionDuration"))) m_ui->kcfg_ColorTransitionDuration->setValue(s[QStringLiteral("ColorTransitionDuration")].toDouble());
    if (s.contains(QStringLiteral("ColorCycleOrder"))) m_ui->kcfg_ColorCycleOrder->setCurrentIndex(s[QStringLiteral("ColorCycleOrder")].toInt());
    if (s.contains(QStringLiteral("ColorCycleEasing"))) m_ui->kcfg_ColorCycleEasing->setCurrentIndex(s[QStringLiteral("ColorCycleEasing")].toInt());
    if (s.contains(QStringLiteral("ColorList"))) populateColorTable(s[QStringLiteral("ColorList")].toString());

    // Shader Effects
    bool hasShaderSetting = false;
    if (s.contains(QStringLiteral("ShaderEffect")) && s.value(QStringLiteral("ShaderEffect")).isObject()) {
        const QJsonObject bundle = s.value(QStringLiteral("ShaderEffect")).toObject();
        if (LeapKShader::applyShaderBundle(bundle, &m_shaders)) {
            if (bundle.contains(QStringLiteral("id"))) {
                m_activeShaderId = bundle.value(QStringLiteral("id")).toString();
                m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
            }
            if (bundle.contains(QStringLiteral("enabled"))) {
                m_ui->kcfg_CustomShaderEnabled->setChecked(bundle.value(QStringLiteral("enabled")).toBool());
            }
            hasShaderSetting = true;
        }
    }

    if (s.contains(QStringLiteral("CustomShaderEnabled"))) {
        m_ui->kcfg_CustomShaderEnabled->setChecked(s[QStringLiteral("CustomShaderEnabled")].toBool());
        hasShaderSetting = true;
    }
    if (s.contains(QStringLiteral("ActiveCustomShaderId"))) {
        m_activeShaderId = s[QStringLiteral("ActiveCustomShaderId")].toString();
        m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
        hasShaderSetting = true;
    }

    // Backwards compatibility with presets using legacy keys:
    if (s.value(QStringLiteral("GlitchEnabled")).toBool(false)) {
        m_ui->kcfg_CustomShaderEnabled->setChecked(true);
        m_activeShaderId = QStringLiteral("futuristic_glitch");
        m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
        hasShaderSetting = true;
    } else if (s.value(QStringLiteral("BreatheEnabled")).toBool(false)) {
        m_ui->kcfg_CustomShaderEnabled->setChecked(true);
        m_activeShaderId = QStringLiteral("breathe_pulse");
        m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
        hasShaderSetting = true;
    } else if (s.value(QStringLiteral("NeonFlickerEnabled")).toBool(false)) {
        m_ui->kcfg_CustomShaderEnabled->setChecked(true);
        m_activeShaderId = QStringLiteral("neon_flicker");
        m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
        hasShaderSetting = true;
    } else if (s.value(QStringLiteral("FlashEnabled")).toBool(false)) {
        m_ui->kcfg_CustomShaderEnabled->setChecked(true);
        m_activeShaderId = QStringLiteral("flash_strobe");
        m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
        hasShaderSetting = true;
    }

    // Update parameter values if present in preset
    for (auto &sh : m_shaders) {
        if (sh.id == QStringLiteral("breathe_pulse")) {
            for (auto &p : sh.parameters) {
                if (p.id == QStringLiteral("speed") && s.contains(QStringLiteral("BreatheSpeed"))) p.currentValue = s[QStringLiteral("BreatheSpeed")].toDouble();
                else if (p.id == QStringLiteral("intensity") && s.contains(QStringLiteral("BreatheIntensity"))) p.currentValue = s[QStringLiteral("BreatheIntensity")].toDouble();
            }
        } else if (sh.id == QStringLiteral("futuristic_glitch")) {
            for (auto &p : sh.parameters) {
                if (p.id == QStringLiteral("speed") && s.contains(QStringLiteral("GlitchSpeed"))) p.currentValue = s[QStringLiteral("GlitchSpeed")].toDouble();
                else if (p.id == QStringLiteral("intensity") && s.contains(QStringLiteral("GlitchIntensity"))) p.currentValue = s[QStringLiteral("GlitchIntensity")].toDouble();
                else if (p.id == QStringLiteral("anomalyEnabled") && s.contains(QStringLiteral("GlitchAnomalyEnabled"))) p.currentValue = s[QStringLiteral("GlitchAnomalyEnabled")].toBool() ? 1.0 : 0.0;
                else if (p.id == QStringLiteral("frequency") && s.contains(QStringLiteral("GlitchFrequency"))) p.currentValue = s[QStringLiteral("GlitchFrequency")].toDouble();
            }
        } else if (sh.id == QStringLiteral("neon_flicker")) {
            for (auto &p : sh.parameters) {
                if (p.id == QStringLiteral("interval") && s.contains(QStringLiteral("NeonFlickerInterval"))) p.currentValue = s[QStringLiteral("NeonFlickerInterval")].toDouble();
                else if (p.id == QStringLiteral("duration") && s.contains(QStringLiteral("NeonFlickerDuration"))) p.currentValue = s[QStringLiteral("NeonFlickerDuration")].toDouble();
                else if (p.id == QStringLiteral("intensity") && s.contains(QStringLiteral("NeonFlickerIntensity"))) p.currentValue = s[QStringLiteral("NeonFlickerIntensity")].toDouble();
            }
        } else if (sh.id == QStringLiteral("flash_strobe")) {
            for (auto &p : sh.parameters) {
                if (p.id == QStringLiteral("interval") && s.contains(QStringLiteral("FlashInterval"))) p.currentValue = s[QStringLiteral("FlashInterval")].toDouble();
                else if (p.id == QStringLiteral("duration") && s.contains(QStringLiteral("FlashDuration"))) p.currentValue = s[QStringLiteral("FlashDuration")].toDouble();
            }
        }
    }
    if (hasShaderSetting) {
        refreshShadersTable();
    }

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
        const QJsonObject currentSettings = collectCurrentSettings();
        QJsonObject settingsToApply;

        if (LeapK::PresetActivationDialog::selectSettings(widget(), p, currentSettings, settingsToApply)) {
            applySettings(settingsToApply);
            const int count = settingsToApply.size();
            if (count == p.settings.size()) {
                m_ui->labelPresetStatus->setText(QStringLiteral("✅ Activated full preset: %1").arg(presetName));
            } else {
                m_ui->labelPresetStatus->setText(QStringLiteral("✅ Applied %1 selected settings from preset: %2").arg(count).arg(presetName));
            }
            m_ui->labelPresetStatus->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));
            markAsChanged();
        }
    }
}

void ActiveWindowOutlineConfig::createNewPreset()
{
    const QString title = QStringLiteral("Save Current as Preset");

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
    descEdit->setPlaceholderText(QStringLiteral("Optional preset description…"));
    descEdit->setMaximumHeight(80);

    formLayout->addRow(QStringLiteral("Preset Name:"), nameEdit);
    formLayout->addRow(QStringLiteral("Description:"), descEdit);
    vbox->addLayout(formLayout);

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
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
        ? QStringLiteral("User custom preset")
        : trimmedDesc;
    p.isBuiltIn = false;
    p.settings = collectCurrentSettings();
    if (p.settings.contains(QStringLiteral("ShaderEffect"))) {
        p.shader = p.settings.value(QStringLiteral("ShaderEffect")).toObject();
    }

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

    m_ui->labelPresetStatus->setText(QStringLiteral("✅ Preset successfully saved: %1").arg(trimmedName));
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

    const QString title = QStringLiteral("Edit Preset");

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

    formLayout->addRow(QStringLiteral("Preset Name:"), nameEdit);
    formLayout->addRow(QStringLiteral("Description:"), descEdit);
    vbox->addLayout(formLayout);

    if (isBuiltIn) {
        auto *noteLabel = new QLabel(QStringLiteral("<i>ℹ️ This is a built-in preset. Saving changes will create a custom user preset.</i>"), &dlg);
        noteLabel->setWordWrap(true);
        noteLabel->setStyleSheet(QStringLiteral("color: #3daee9;"));
        vbox->addWidget(noteLabel);
    }

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vbox->addWidget(btnBox);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString newName = nameEdit->text().trimmed();
    if (newName.isEmpty()) {
        QMessageBox::warning(widget(), title, QStringLiteral("Preset name cannot be empty."));
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

    m_ui->labelPresetStatus->setText(QStringLiteral("✅ Updated preset: %1").arg(newName));
    m_ui->labelPresetStatus->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));
}

void ActiveWindowOutlineConfig::duplicateSelectedPreset()
{
    const int row = m_ui->tablePresets->currentRow();
    if (row < 0) return;

    const QString originalName = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole).toString();
    const QString filePath = m_ui->tablePresets->item(row, 0)->data(Qt::UserRole + 1).toString();

    const QString title = QStringLiteral("Duplicate Preset");
    const QString label = QStringLiteral("Enter name for duplicate:");

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

    const QString title = QStringLiteral("Delete Preset");

    if (isBuiltIn) {
        const auto reply = QMessageBox::question(widget(), title,
            QStringLiteral("Preset '%1' is a built-in preset. Deleting it will restore its defaults on restart. Are you sure?").arg(name),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    } else {
        const auto reply = QMessageBox::question(widget(), title,
            QStringLiteral("Are you sure you want to delete preset '%1'?").arg(name),
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
    const QString title = QStringLiteral("Export Preset");
    const QString defaultPath = QDir::homePath() + QStringLiteral("/") + safeName + QStringLiteral(".json");
    const QString filter = QStringLiteral("JSON Preset Files (*.json);;All Files (*)");

    const QString targetPath = QFileDialog::getSaveFileName(widget(), title, defaultPath, filter);
    if (targetPath.isEmpty()) {
        return;
    }

    QFile out(targetPath);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        out.write(data);
        out.close();
        QMessageBox::information(widget(), title,
            QStringLiteral("Preset '%1' successfully exported to:\n%2").arg(presetName, targetPath));
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
    const QString title = QStringLiteral("Import Preset");
    const QString filter = QStringLiteral("JSON Preset Files (*.json);;All Files (*)");

    const QString filePath = QFileDialog::getOpenFileName(widget(), title, QDir::homePath(), filter);
    if (filePath.isEmpty()) {
        return;
    }

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(widget(), title,
            QStringLiteral("Could not open file: %1").arg(f.errorString()));
        return;
    }

    OutlinePreset::PresetInfo p;
    QString err;
    if (!OutlinePreset::parsePresetJson(f.readAll(), p, &err)) {
        QMessageBox::warning(widget(), title,
            QStringLiteral("Invalid preset file:\n%1").arg(err));
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
        QStringLiteral("Preset '%1' was successfully imported and added to the preset list.").arg(p.name));
}

void ActiveWindowOutlineConfig::save()
{
    KCModule::save();

    KConfigGroup group = KSharedConfig::openConfig(QStringLiteral("kwinrc"))->group(QStringLiteral("Effect-leapk"));
    group.writeEntry(QStringLiteral("ColorList"), serializeColorTable());
    group.writeEntry(QStringLiteral("AmbilightEnsureContrast"), m_ui->kcfg_EnsurePerimeterContrast->isChecked());
    group.writeEntry(QStringLiteral("StylePrecedence"), serializePrecedenceList());
    const int badgeMode = m_ui->kcfg_BadgeDisplayMode->currentIndex();
    group.writeEntry(QStringLiteral("MarkShowLetterBadges"), badgeMode != 2);
    group.writeEntry(QStringLiteral("BadgesAlwaysVisible"), badgeMode == 1);
    group.writeEntry(QStringLiteral("BadgeWindowBadgesEnabled"), m_ui->kcfg_BadgeWindowBadgesEnabled->isChecked());
    group.writeEntry(QStringLiteral("BadgeLetterFont"), m_ui->combo_BadgeLetterFont->currentFont().family());
    group.writeEntry(QStringLiteral("BadgeLabelFont"), m_ui->combo_BadgeLabelFont->currentFont().family());
    group.writeEntry(QStringLiteral("BadgeCaptionFont"), m_ui->combo_BadgeCaptionFont->currentFont().family());
    group.writeEntry(QStringLiteral("BadgeLocationFont"), m_ui->combo_BadgeLocationFont->currentFont().family());
    group.writeEntry(QStringLiteral("BadgeWindowShowIcon"), m_ui->kcfg_BadgeWindowShowIcon->isChecked());
    group.writeEntry(QStringLiteral("BadgeWindowShowThumbnail"), m_ui->kcfg_BadgeWindowShowThumbnail->isChecked());
    group.writeEntry(QStringLiteral("BadgeHudShowIcon"), m_ui->kcfg_BadgeHudShowIcon->isChecked());
    group.writeEntry(QStringLiteral("BadgeHudShowThumbnail"), m_ui->kcfg_BadgeHudShowThumbnail->isChecked());
    group.writeEntry(QStringLiteral("BadgeHudShowHints"), m_ui->kcfg_BadgeHudShowHints->isChecked());
    group.writeEntry(QStringLiteral("BadgeHudNavFontSize"), m_ui->kcfg_BadgeHudNavFontSize->value());
    group.writeEntry(QStringLiteral("BadgeHudHintsFontSize"), m_ui->kcfg_BadgeHudHintsFontSize->value());
    const int winThumbSize = m_ui->kcfg_BadgeWindowThumbnailSize->value();
    group.writeEntry(QStringLiteral("BadgeWindowThumbnailSize"), winThumbSize);
    group.writeEntry(QStringLiteral("BadgeWindowThumbnailWidth"), winThumbSize);
    group.writeEntry(QStringLiteral("BadgeWindowThumbnailHeight"), winThumbSize);
    group.writeEntry(QStringLiteral("BadgeThumbnailWidth"), winThumbSize);
    group.writeEntry(QStringLiteral("BadgeThumbnailHeight"), winThumbSize);
    const int hudThumbSize = m_ui->kcfg_BadgeHudThumbnailSize->value();
    group.writeEntry(QStringLiteral("BadgeHudThumbnailSize"), hudThumbSize);
    group.writeEntry(QStringLiteral("BadgeHudThumbnailWidth"), hudThumbSize);
    group.writeEntry(QStringLiteral("BadgeHudThumbnailHeight"), hudThumbSize);
    group.writeEntry(QStringLiteral("BadgeHudIncludeAll"), m_ui->kcfg_BadgeHudIncludeAll->isChecked());
    group.writeEntry(QStringLiteral("BadgeHudGroupWindows"), m_ui->kcfg_BadgeHudGroupWindows->isChecked());
    group.writeEntry(QStringLiteral("BadgeHudGroupBorderWidth"), m_ui->kcfg_BadgeHudGroupBorderWidth->value());
    group.writeEntry(QStringLiteral("BadgeHudGroupFontSize"), m_ui->kcfg_BadgeHudGroupFontSize->value());
    group.writeEntry(QStringLiteral("BadgeHudGroupCornerRadius"), m_ui->kcfg_BadgeHudGroupCornerRadius->value());
    group.writeEntry(QStringLiteral("BadgeHudGroupShaderEnabled"), m_ui->kcfg_BadgeHudGroupShaderEnabled->isChecked());
    group.writeEntry(QStringLiteral("BadgeLivePreviews"), m_ui->kcfg_BadgeLivePreviews->isChecked());
    group.writeEntry(QStringLiteral("BadgeTimeout"), m_ui->kcfg_BadgeTimeout->value());
    group.writeEntry(QStringLiteral("JumpRecentKey"), m_ui->kcfg_JumpRecentKey->keySequence().toString());
    group.deleteEntry(QStringLiteral("BadgeShowIcon"));
    group.deleteEntry(QStringLiteral("BadgeShowThumbnail"));
    saveShaderParameterValues();
    saveThemeParameterValues();
    group.writeEntry(QStringLiteral("ActiveThemeId"), m_activeThemeId);
    group.writeEntry(QStringLiteral("ActiveThemeParams"), QString::fromUtf8(QJsonDocument(m_themeUserOverrides).toJson(QJsonDocument::Compact)));
    const bool customOn = m_ui->kcfg_CustomShaderEnabled->isChecked();
    group.writeEntry(QStringLiteral("CustomShaderEnabled"), customOn);
    group.writeEntry(QStringLiteral("ActiveCustomShaderId"), m_activeShaderId);
    group.writeEntry(QStringLiteral("BreatheEnabled"), false);
    group.writeEntry(QStringLiteral("GlitchEnabled"), false);
    group.writeEntry(QStringLiteral("NeonFlickerEnabled"), false);
    group.writeEntry(QStringLiteral("FlashEnabled"), false);
    group.sync();

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("reconfigureEffect"));
    message << QStringLiteral("leapk");
    QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);

    const bool trayEnabled = m_ui->kcfg_TrayIconEnabled->isChecked();
    const QString autostartDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
    const QString desktopPath = autostartDir + QStringLiteral("/leapk-tray.desktop");
    const QString oldDesktopPath = autostartDir + QStringLiteral("/active-window-outline-tray.desktop");
    QFile::remove(oldDesktopPath);
    if (trayEnabled) {
        if (!QFile::exists(desktopPath)) {
            QString systemDesktop = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, QStringLiteral("leapk-tray.desktop"));
            if (systemDesktop.isEmpty()) {
                systemDesktop = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, QStringLiteral("active-window-outline-tray.desktop"));
            }
            if (!systemDesktop.isEmpty()) {
                QDir().mkpath(autostartDir);
                QFile::copy(systemDesktop, desktopPath);
            }
        }
        QProcess::startDetached(QStringLiteral("leapk-tray"), {});
    } else {
        QFile::remove(desktopPath);
        QProcess::execute(QStringLiteral("killall"), {QStringLiteral("leapk-tray"), QStringLiteral("kleap-tray"), QStringLiteral("active-window-outline-tray")});
    }
}

void ActiveWindowOutlineConfig::initShadersSection()
{
    m_ui->tableShaders->setColumnCount(6);
    m_ui->tableShaders->setHorizontalHeaderLabels({
        QStringLiteral("Aktywny"),
        QStringLiteral("Nazwa / Name"),
        QStringLiteral("Cel / Target"),
        QStringLiteral("Autor"),
        QStringLiteral("Wersja"),
        QStringLiteral("Opis / Description")
    });
    m_ui->tableShaders->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui->tableShaders->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ui->tableShaders->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui->tableShaders->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_ui->tableShaders->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_ui->tableShaders->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_ui->tableShaders->verticalHeader()->setVisible(false);
    m_ui->tableShaders->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->tableShaders->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ui->tableShaders->setContextMenuPolicy(Qt::CustomContextMenu);

    // Resizable splitter between table and parameters panel
    m_ui->splitterShaders->setSizes({260, 200});
    m_ui->splitterShaders->setHandleWidth(6);

    connect(m_ui->tableShaders, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_updatingTable || !item || item->column() != 0) {
            return;
        }
        const int row = item->row();
        if (row < 0 || row >= m_shaders.size()) {
            return;
        }
        const bool shouldBeActive = (item->checkState() == Qt::Checked);
        setShaderActive(row, shouldBeActive);
    });

    connect(m_ui->tableShaders, &QTableWidget::cellDoubleClicked, this, [this](int row, int col) {
        Q_UNUSED(col);
        if (row >= 0 && row < m_shaders.size()) {
            toggleShaderAt(row);
        }
    });

    connect(m_ui->tableShaders, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const int row = m_ui->tableShaders->rowAt(pos.y());
        if (row < 0 || row >= m_shaders.size()) {
            return;
        }
        m_ui->tableShaders->selectRow(row);
        const auto &effect = m_shaders[row];
        const bool isActive = m_ui->kcfg_CustomShaderEnabled->isChecked() && (m_activeShaderId == effect.id);

        QMenu menu(widget());
        menu.addAction(isActive ? QIcon::fromTheme(QStringLiteral("dialog-cancel")) : QIcon::fromTheme(QStringLiteral("dialog-ok")),
                       isActive ? QStringLiteral("Disable this effect") : QStringLiteral("Set as active"),
                       this, &ActiveWindowOutlineConfig::toggleSelectedShader);
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme(QStringLiteral("document-edit")), QStringLiteral("Edit Effect…"), this, &ActiveWindowOutlineConfig::editSelectedShader);
        menu.addAction(QIcon::fromTheme(QStringLiteral("document-export")), QStringLiteral("Export Effect…"), this, &ActiveWindowOutlineConfig::exportSelectedShader);
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), QStringLiteral("Delete Effect"), this, &ActiveWindowOutlineConfig::deleteSelectedShader);
        menu.exec(m_ui->tableShaders->viewport()->mapToGlobal(pos));
    });

    connect(m_ui->tableShaders, &QTableWidget::itemSelectionChanged, this, [this]() {
        if (m_updatingTable) {
            return;
        }
        const int row = m_ui->tableShaders->currentRow();
        if (row >= 0 && row < m_shaders.size()) {
            const QString id = m_shaders[row].id;
            const bool globalEnabled = m_ui->kcfg_CustomShaderEnabled->isChecked();
            const bool isCurActive = globalEnabled && (id == m_activeShaderId);
            m_ui->btnToggleShader->setText(isCurActive ? QStringLiteral("Disable this effect") : QStringLiteral("Enable this effect"));
            m_ui->btnToggleShader->setIcon(isCurActive ? QIcon::fromTheme(QStringLiteral("dialog-cancel"))
                                                       : QIcon::fromTheme(QStringLiteral("dialog-ok")));
            updateShaderParametersUIFor(id);
        }
    });

    connect(m_ui->kcfg_CustomShaderEnabled, &QCheckBox::toggled, this, [this](bool) {
        refreshShadersTable();
        markAsChanged();
    });

    refreshShadersTable();
}

void ActiveWindowOutlineConfig::toggleSelectedShader()
{
    const int row = m_ui->tableShaders->currentRow();
    if (row >= 0 && row < m_shaders.size()) {
        toggleShaderAt(row);
    }
}

void ActiveWindowOutlineConfig::toggleShaderAt(int row)
{
    if (row < 0 || row >= m_shaders.size()) return;
    const QString id = m_shaders[row].id;
    const bool isCurActive = m_ui->kcfg_CustomShaderEnabled->isChecked() && (m_activeShaderId == id);
    setShaderActive(row, !isCurActive);
}

void ActiveWindowOutlineConfig::setShaderActive(int row, bool active)
{
    if (row < 0 || row >= m_shaders.size()) return;
    const QString id = m_shaders[row].id;

    if (active) {
        m_activeShaderId = id;
        m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
        m_ui->kcfg_CustomShaderEnabled->setChecked(true);
    } else {
        if (m_activeShaderId == id) {
            m_ui->kcfg_CustomShaderEnabled->setChecked(false);
        }
    }

    refreshShadersTable();
    markAsChanged();
}

void ActiveWindowOutlineConfig::refreshShadersTable()
{
    m_updatingTable = true;
    m_shaders = LeapKShader::loadAllShaders();

    // Preserve table scroll position
    const int tableScrollVal = m_ui->tableShaders->verticalScrollBar()->value();

    if (m_ui->tableShaders->rowCount() != m_shaders.size()) {
        m_ui->tableShaders->setRowCount(m_shaders.size());
    }

    const bool globalEnabled = m_ui->kcfg_CustomShaderEnabled->isChecked();
    int activeRow = -1;
    const int previousRow = m_ui->tableShaders->currentRow();

    for (int i = 0; i < m_shaders.size(); ++i) {
        const auto &effect = m_shaders[i];
        const bool isActive = globalEnabled && (!m_activeShaderId.isEmpty() && effect.id == m_activeShaderId);
        if (isActive) {
            activeRow = i;
        }

        // Col 0: Standard KDE Checkbox
        auto *statusItem = m_ui->tableShaders->item(i, 0);
        if (!statusItem) {
            statusItem = new QTableWidgetItem();
            statusItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            statusItem->setTextAlignment(Qt::AlignCenter);
            m_ui->tableShaders->setItem(i, 0, statusItem);
        }
        statusItem->setCheckState(isActive ? Qt::Checked : Qt::Unchecked);
        statusItem->setText(QString());
        statusItem->setToolTip(isActive ? QStringLiteral("Effect is active. Click to disable.")
                                        : QStringLiteral("Click to activate this effect."));

        // Col 1: Name
        auto *nameItem = m_ui->tableShaders->item(i, 1);
        if (!nameItem) {
            nameItem = new QTableWidgetItem(effect.name);
            m_ui->tableShaders->setItem(i, 1, nameItem);
        } else {
            nameItem->setText(effect.name);
        }
        if (effect.isBuiltIn) {
            nameItem->setIcon(QIcon::fromTheme(QStringLiteral("favorite")));
        } else {
            nameItem->setIcon(QIcon());
        }
        QFont nf = nameItem->font();
        nf.setBold(isActive);
        nameItem->setFont(nf);

        // Col 2: Target
        QString targetStr = effect.target;
        if (effect.target == QStringLiteral("glow")) {
            targetStr = QStringLiteral("Glow");
        } else if (effect.target == QStringLiteral("outline")) {
            targetStr = QStringLiteral("Outline");
        } else if (effect.target == QStringLiteral("both")) {
            targetStr = QStringLiteral("Glow + Outline");
        }
        auto *targetItem = m_ui->tableShaders->item(i, 2);
        if (!targetItem) {
            targetItem = new QTableWidgetItem(targetStr);
            m_ui->tableShaders->setItem(i, 2, targetItem);
        } else {
            targetItem->setText(targetStr);
        }

        // Col 3: Author
        auto *authorItem = m_ui->tableShaders->item(i, 3);
        if (!authorItem) {
            authorItem = new QTableWidgetItem(effect.author);
            m_ui->tableShaders->setItem(i, 3, authorItem);
        } else {
            authorItem->setText(effect.author);
        }

        // Col 4: Version
        auto *versionItem = m_ui->tableShaders->item(i, 4);
        if (!versionItem) {
            versionItem = new QTableWidgetItem(effect.version);
            m_ui->tableShaders->setItem(i, 4, versionItem);
        } else {
            versionItem->setText(effect.version);
        }

        // Col 5: Description
        auto *descItem = m_ui->tableShaders->item(i, 5);
        if (!descItem) {
            descItem = new QTableWidgetItem(effect.description);
            m_ui->tableShaders->setItem(i, 5, descItem);
        } else {
            descItem->setText(effect.description);
        }
    }

    if (previousRow >= 0 && previousRow < m_shaders.size()) {
        m_ui->tableShaders->selectRow(previousRow);
    } else if (activeRow >= 0) {
        m_ui->tableShaders->selectRow(activeRow);
    } else if (!m_shaders.isEmpty()) {
        m_ui->tableShaders->selectRow(0);
    }

    // Restore table vertical scroll position so it never jumps
    m_ui->tableShaders->verticalScrollBar()->setValue(tableScrollVal);

    m_updatingTable = false;

    const int curRow = m_ui->tableShaders->currentRow();
    QString currentId;
    if (curRow >= 0 && curRow < m_shaders.size()) {
        currentId = m_shaders[curRow].id;
        const bool isCurActive = globalEnabled && (currentId == m_activeShaderId);
        m_ui->btnToggleShader->setText(isCurActive ? QStringLiteral("Disable this effect") : QStringLiteral("Enable this effect"));
        m_ui->btnToggleShader->setIcon(isCurActive ? QIcon::fromTheme(QStringLiteral("dialog-cancel"))
                                                   : QIcon::fromTheme(QStringLiteral("dialog-ok")));
    } else {
        m_ui->btnToggleShader->setText(QStringLiteral("Enable / Disable Effect"));
    }

    updateShaderParametersUIFor(currentId.isEmpty() ? m_activeShaderId : currentId);
}

void ActiveWindowOutlineConfig::updateShaderParametersUI()
{
    const int row = m_ui->tableShaders->currentRow();
    const QString id = (row >= 0 && row < m_shaders.size()) ? m_shaders[row].id : m_activeShaderId;
    updateShaderParametersUIFor(id);
}

void ActiveWindowOutlineConfig::updateShaderParametersUIFor(const QString &id)
{
    const bool idChanged = (m_currentDisplayedShaderId != id);
    m_currentDisplayedShaderId = id;

    int effectIdx = -1;
    for (int i = 0; i < m_shaders.size(); ++i) {
        if (m_shaders[i].id == id) {
            effectIdx = i;
            break;
        }
    }
    if (effectIdx < 0 && !m_shaders.isEmpty()) {
        effectIdx = 0;
    }
    if (effectIdx < 0) {
        m_ui->groupActiveShaderParams->setTitle(QStringLiteral("Effect Parameters"));
        auto *lbl = new QLabel(QStringLiteral("Select a shader effect from the list above."), m_ui->groupActiveShaderParams);
        m_ui->formLayoutShaderParams->addRow(lbl);
        return;
    }

    auto &effect = m_shaders[effectIdx];
    const bool isGlobalOn = m_ui->kcfg_CustomShaderEnabled->isChecked();
    const bool isActive = isGlobalOn && (effect.id == m_activeShaderId);

    m_ui->groupActiveShaderParams->setTitle(QStringLiteral("Effect Parameters: %1 (%2)")
        .arg(effect.name, isActive ? QStringLiteral("Active") : QStringLiteral("Inactive")));

    // If the displayed effect hasn't changed, do not rebuild widgets!
    if (!idChanged) {
        return;
    }

    // Preserve scroll position
    const int tableScrollVal = m_ui->tableShaders->verticalScrollBar()->value();

    // Prevent layout collapse during rebuild
    m_ui->groupActiveShaderParams->setMinimumHeight(qMax(60, m_ui->groupActiveShaderParams->height()));

    QLayoutItem *item;
    while ((item = m_ui->formLayoutShaderParams->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (effect.parameters.isEmpty()) {
        auto *lbl = new QLabel(QStringLiteral("This shader effect has no configurable parameters."), m_ui->groupActiveShaderParams);
        lbl->setStyleSheet(QStringLiteral("color: palette(placeholder-text); font-style: italic;"));
        m_ui->formLayoutShaderParams->addRow(lbl);
        m_ui->groupActiveShaderParams->setMinimumHeight(0);
        m_ui->tableShaders->verticalScrollBar()->setValue(tableScrollVal);
        return;
    }

    for (int pIdx = 0; pIdx < effect.parameters.size(); ++pIdx) {
        auto &param = effect.parameters[pIdx];
        if (param.type == QStringLiteral("float")) {
            auto *spin = new QDoubleSpinBox(m_ui->groupActiveShaderParams);
            spin->setRange(param.minValue, param.maxValue);
            spin->setSingleStep(param.step);
            spin->setDecimals(2);
            spin->setValue(param.currentValue);
            if (!param.suffix.isEmpty()) {
                spin->setSuffix(param.suffix);
            }
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [&param, this](double val) {
                param.currentValue = val;
                markAsChanged();
            });
            m_ui->formLayoutShaderParams->addRow(param.name + QStringLiteral(":"), spin);
        } else if (param.type == QStringLiteral("int")) {
            auto *spin = new QSpinBox(m_ui->groupActiveShaderParams);
            spin->setRange(qRound(param.minValue), qRound(param.maxValue));
            spin->setSingleStep(qMax(1, qRound(param.step)));
            spin->setValue(qRound(param.currentValue));
            if (!param.suffix.isEmpty()) {
                spin->setSuffix(param.suffix);
            }
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [&param, this](int val) {
                param.currentValue = val;
                markAsChanged();
            });
            m_ui->formLayoutShaderParams->addRow(param.name + QStringLiteral(":"), spin);
        } else if (param.type == QStringLiteral("bool")) {
            auto *cb = new QCheckBox(param.name, m_ui->groupActiveShaderParams);
            cb->setChecked(param.currentValue > 0.5);
            connect(cb, &QCheckBox::toggled, this, [&param, this](bool checked) {
                param.currentValue = checked ? 1.0 : 0.0;
                markAsChanged();
            });
            m_ui->formLayoutShaderParams->addRow(QString(), cb);
        } else if (param.type == QStringLiteral("color")) {
            auto *btn = new QPushButton(param.colorCurrent, m_ui->groupActiveShaderParams);
            QColor initialColor(param.colorCurrent);
            btn->setStyleSheet(QStringLiteral("background-color: %1; color: %2; font-weight: bold; border-radius: 4px; padding: 4px;")
                .arg(initialColor.name(), (initialColor.lightness() > 128) ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
            connect(btn, &QPushButton::clicked, this, [&param, btn, this]() {
                const QColor c = QColorDialog::getColor(QColor(param.colorCurrent), btn, QStringLiteral("Select Color"));
                if (c.isValid()) {
                    param.colorCurrent = c.name();
                    btn->setText(c.name());
                    btn->setStyleSheet(QStringLiteral("background-color: %1; color: %2; font-weight: bold; border-radius: 4px; padding: 4px;")
                        .arg(c.name(), (c.lightness() > 128) ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
                    markAsChanged();
                }
            });
            m_ui->formLayoutShaderParams->addRow(param.name + QStringLiteral(":"), btn);
        }
    }

    m_ui->groupActiveShaderParams->setMinimumHeight(0);
    m_ui->tableShaders->verticalScrollBar()->setValue(tableScrollVal);
}

void ActiveWindowOutlineConfig::saveShaderParameterValues()
{
    for (const auto &effect : std::as_const(m_shaders)) {
        if (!effect.filePath.isEmpty()) {
            LeapKShader::saveShaderEffectToFile(effect, effect.filePath);
        } else {
            const QString path = LeapKShader::defaultShadersDirectory() + QStringLiteral("/%1.json").arg(effect.id);
            LeapKShader::saveShaderEffectToFile(effect, path);
        }
    }
}

void ActiveWindowOutlineConfig::createNewShader()
{
    LeapK::ShaderEditorDialog dlg(widget(), nullptr);
    if (dlg.exec() == QDialog::Accepted) {
        const auto effect = dlg.getEffect();
        const QString dirPath = LeapKShader::defaultShadersDirectory();
        const QString filePath = dirPath + QStringLiteral("/%1.json").arg(effect.id);
        LeapKShader::saveShaderEffectToFile(effect, filePath);
        m_activeShaderId = effect.id;
        m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
        refreshShadersTable();
        markAsChanged();
    }
}

void ActiveWindowOutlineConfig::editSelectedShader()
{
    const int row = m_ui->tableShaders->currentRow();
    if (row < 0 || row >= m_shaders.size()) {
        return;
    }
    const auto &effect = m_shaders[row];
    LeapK::ShaderEditorDialog dlg(widget(), &effect);
    if (dlg.exec() == QDialog::Accepted) {
        const auto updated = dlg.getEffect();
        const QString filePath = !effect.filePath.isEmpty()
            ? effect.filePath
            : LeapKShader::defaultShadersDirectory() + QStringLiteral("/%1.json").arg(updated.id);
        LeapKShader::saveShaderEffectToFile(updated, filePath);
        m_activeShaderId = updated.id;
        m_ui->kcfg_ActiveCustomShaderId->setText(m_activeShaderId);
        refreshShadersTable();
        markAsChanged();
    }
}

void ActiveWindowOutlineConfig::deleteSelectedShader()
{
    const int row = m_ui->tableShaders->currentRow();
    if (row < 0 || row >= m_shaders.size()) {
        return;
    }
    const auto &effect = m_shaders[row];
    if (effect.isBuiltIn) {
        QMessageBox::warning(widget(), QStringLiteral("Protected Built-in Effect"),
                             QStringLiteral("Built-in LeapK effects cannot be deleted from disk."));
        return;
    }
    if (QMessageBox::question(widget(), QStringLiteral("Delete Effect"),
                              QStringLiteral("Are you sure you want to delete shader effect '%1'?").arg(effect.name),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    if (!effect.filePath.isEmpty()) {
        QFile::remove(effect.filePath);
    }
    m_activeShaderId.clear();
    refreshShadersTable();
    markAsChanged();
}

void ActiveWindowOutlineConfig::exportSelectedShader()
{
    const int row = m_ui->tableShaders->currentRow();
    if (row < 0 || row >= m_shaders.size()) {
        return;
    }
    const auto &effect = m_shaders[row];
    const QString suggestedName = effect.id + QStringLiteral(".leapk-shader");
    const QString destPath = QFileDialog::getSaveFileName(
        widget(),
        QStringLiteral("Export Shader Effect"),
        QDir::homePath() + QStringLiteral("/") + suggestedName,
        QStringLiteral("Shader Effects (*.leapk-shader *.json);;All Files (*)")
    );
    if (destPath.isEmpty()) {
        return;
    }
    if (LeapKShader::saveShaderEffectToFile(effect, destPath)) {
        QMessageBox::information(widget(), QStringLiteral("Export Complete"),
                                 QStringLiteral("Shader effect '%1' successfully exported to file:\n%2").arg(effect.name, destPath));
    } else {
        QMessageBox::critical(widget(), QStringLiteral("Save Error"),
                              QStringLiteral("Failed to save shader file to specified path."));
    }
}

void ActiveWindowOutlineConfig::importShader()
{
    const QString srcPath = QFileDialog::getOpenFileName(
        widget(),
        QStringLiteral("Import Shader Effect"),
        QDir::homePath(),
        QStringLiteral("Shader Effects (*.leapk-shader *.json);;All Files (*)")
    );
    if (srcPath.isEmpty()) {
        return;
    }

    LeapKShader::CustomShaderEffect effect;
    QString err;
    if (!LeapKShader::loadShaderEffectFromFile(srcPath, effect, &err)) {
        QMessageBox::critical(widget(), QStringLiteral("Import Error"),
                              QStringLiteral("Failed to import shader file:\n%1").arg(err));
        return;
    }

    // Save to user shaders directory
    const QString destPath = LeapKShader::defaultShadersDirectory() + QStringLiteral("/%1.json").arg(effect.id);
    effect.filePath = destPath;
    LeapKShader::saveShaderEffectToFile(effect, destPath);

    refreshShadersTable();
    QMessageBox::information(widget(), QStringLiteral("Import Complete"),
                             QStringLiteral("Shader effect '%1' was successfully imported and added to the list.").arg(effect.name));
}

void ActiveWindowOutlineConfig::initPersistentMarksSection()
{
    auto *table = m_ui->tablePersistentMarks;
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels({
        tr("Letter"),
        tr("Color"),
        tr("Label"),
        tr("Application (Class)"),
        tr("Title / Process Pattern"),
        tr("Desktop")
    });
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    connect(m_ui->btnDeletePersistentMark, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::deleteSelectedPersistentMark);
    connect(m_ui->btnClearAllPersistentMarks, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::clearAllPersistentMarks);
    connect(m_ui->btnReloadPersistentMarks, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::refreshPersistentMarksTable);

    auto *btnOpenMgr = new QPushButton(tr("🏷️ Marker Manager…"), m_ui->tablePersistentMarks->parentWidget());
    btnOpenMgr->setToolTip(tr("Open the graphical Marker & Group Manager"));
    connect(btnOpenMgr, &QPushButton::clicked, this, [this]() {
        QProcess::startDetached(QStringLiteral("leapk-marker-manager"));
    });
    m_ui->layoutPersistentMarksButtons->addWidget(btnOpenMgr);

    refreshPersistentMarksTable();
}

void ActiveWindowOutlineConfig::refreshPersistentMarksTable()
{
    m_persistentMarks = LeapKMarks::loadPersistentMarks();

    auto *table = m_ui->tablePersistentMarks;
    table->setRowCount(m_persistentMarks.size());

    for (int i = 0; i < m_persistentMarks.size(); ++i) {
        const auto &rule = m_persistentMarks.at(i);

        // Letter
        auto *itemLetter = new QTableWidgetItem(rule.letter.isNull() ? QStringLiteral("-") : QString(rule.letter));
        itemLetter->setTextAlignment(Qt::AlignCenter);
        QFont f = itemLetter->font();
        f.setBold(true);
        itemLetter->setFont(f);
        table->setItem(i, 0, itemLetter);

        // Color
        QPixmap pix(16, 16);
        pix.fill(rule.color.isValid() ? rule.color : Qt::transparent);
        auto *itemColor = new QTableWidgetItem(QIcon(pix), rule.color.isValid() ? rule.color.name(QColor::HexArgb) : QStringLiteral("-"));
        table->setItem(i, 1, itemColor);

        // Label
        auto *itemLabel = new QTableWidgetItem(rule.label.isEmpty() ? QStringLiteral("-") : rule.label);
        table->setItem(i, 2, itemLabel);

        // App
        auto *itemApp = new QTableWidgetItem(rule.matchApp);
        table->setItem(i, 3, itemApp);

        // Caption / Process
        auto *itemCaption = new QTableWidgetItem(rule.matchCaption.isEmpty() ? tr("(any)") : rule.matchCaption);
        table->setItem(i, 4, itemCaption);

        // Desktop
        QString deskStr;
        if (rule.matchDesktop > 0) {
            deskStr = tr("Desktop %1").arg(rule.matchDesktop);
        } else {
            deskStr = tr("All");
        }
        auto *itemDesk = new QTableWidgetItem(deskStr);
        itemDesk->setTextAlignment(Qt::AlignCenter);
        table->setItem(i, 5, itemDesk);
    }
}

void ActiveWindowOutlineConfig::deleteSelectedPersistentMark()
{
    auto *table = m_ui->tablePersistentMarks;
    const int row = table->currentRow();
    if (row < 0 || row >= m_persistentMarks.size()) {
        return;
    }
    m_persistentMarks.removeAt(row);
    LeapKMarks::savePersistentMarks(m_persistentMarks);
    refreshPersistentMarksTable();

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("reconfigureEffect")
    );
    msg << QStringLiteral("leapk");
    QDBusConnection::sessionBus().send(msg);
}

void ActiveWindowOutlineConfig::clearAllPersistentMarks()
{
    auto res = QMessageBox::question(
        widget(),
        tr("Clear Rules"),
        tr("Are you sure you want to delete all persistent mark rules?"),
        QMessageBox::Yes | QMessageBox::No
    );
    if (res != QMessageBox::Yes) {
        return;
    }
    m_persistentMarks.clear();
    LeapKMarks::savePersistentMarks(m_persistentMarks);
    refreshPersistentMarksTable();

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("reconfigureEffect")
    );
    msg << QStringLiteral("leapk");
    QDBusConnection::sessionBus().send(msg);
}

void ActiveWindowOutlineConfig::initThemesTab()
{
    m_ui->tableThemes->setColumnCount(5);
    m_ui->tableThemes->setHorizontalHeaderLabels({
        QStringLiteral("Status"),
        QStringLiteral("Theme Name"),
        QStringLiteral("Author"),
        QStringLiteral("Version"),
        QStringLiteral("Description")
    });
    m_ui->tableThemes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_ui->tableThemes->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_ui->tableThemes->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_ui->tableThemes->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_ui->tableThemes->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_ui->tableThemes->verticalHeader()->setVisible(false);
    m_ui->tableThemes->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ui->tableThemes->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ui->tableThemes->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(m_ui->btnApplyTheme, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::applySelectedTheme);
    connect(m_ui->btnImportTheme, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::importTheme);
    connect(m_ui->btnExportTheme, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::exportSelectedTheme);
    connect(m_ui->btnResetThemeParams, &QPushButton::clicked, this, &ActiveWindowOutlineConfig::resetThemeParameters);

    connect(m_ui->tableThemes, &QTableWidget::itemSelectionChanged, this, &ActiveWindowOutlineConfig::updateThemeParametersUI);
    connect(m_ui->tableThemes, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        applySelectedTheme();
    });

    refreshThemesTable();
}

void ActiveWindowOutlineConfig::refreshThemesTable()
{
    m_themes = LeapKTheme::discoverAllThemes();
    if (m_activeThemeId.isEmpty()) {
        m_activeThemeId = QStringLiteral("default-breeze");
    }

    m_ui->tableThemes->setRowCount(0);
    int activeRow = 0;

    for (int r = 0; r < m_themes.size(); ++r) {
        auto &theme = m_themes[r];
        if (theme.id == m_activeThemeId && !m_themeUserOverrides.isEmpty()) {
            theme.applyParameterValues(m_themeUserOverrides);
        }

        m_ui->tableThemes->insertRow(r);
        const bool isActive = (theme.id == m_activeThemeId);
        if (isActive) {
            activeRow = r;
        }

        auto *itemStatus = new QTableWidgetItem(isActive ? QStringLiteral("● Active") : QStringLiteral("○ Inactive"));
        if (isActive) {
            itemStatus->setForeground(QColor(46, 204, 113));
            QFont f = itemStatus->font();
            f.setBold(true);
            itemStatus->setFont(f);
        } else {
            itemStatus->setForeground(QColor(127, 140, 141));
        }

        auto *itemName = new QTableWidgetItem(theme.name);
        auto *itemAuthor = new QTableWidgetItem(theme.author);
        auto *itemVer = new QTableWidgetItem(theme.version + (theme.isBuiltIn ? QStringLiteral(" (Built-in)") : QStringLiteral(" (Custom)")));
        auto *itemDesc = new QTableWidgetItem(theme.description);

        m_ui->tableThemes->setItem(r, 0, itemStatus);
        m_ui->tableThemes->setItem(r, 1, itemName);
        m_ui->tableThemes->setItem(r, 2, itemAuthor);
        m_ui->tableThemes->setItem(r, 3, itemVer);
        m_ui->tableThemes->setItem(r, 4, itemDesc);
    }

    if (m_ui->tableThemes->rowCount() > 0) {
        m_ui->tableThemes->selectRow(activeRow);
    }
}

void ActiveWindowOutlineConfig::applySelectedTheme()
{
    const int r = m_ui->tableThemes->currentRow();
    if (r < 0 || r >= m_themes.size()) {
        return;
    }
    m_activeThemeId = m_themes[r].id;
    m_currentDisplayedThemeId.clear();
    refreshThemesTable();
    updateThemeParametersUI();
    markAsChanged();
}

void ActiveWindowOutlineConfig::updateThemeParametersUI()
{
    const int r = m_ui->tableThemes->currentRow();
    if (r >= 0 && r < m_themes.size()) {
        updateThemeParametersUIFor(m_themes[r].id);
    } else {
        updateThemeParametersUIFor(m_activeThemeId);
    }
}

void ActiveWindowOutlineConfig::updateThemeParametersUIFor(const QString &id)
{
    if (m_currentDisplayedThemeId == id && !m_currentDisplayedThemeId.isEmpty()) {
        return;
    }
    m_currentDisplayedThemeId = id;

    int themeIdx = -1;
    for (int i = 0; i < m_themes.size(); ++i) {
        if (m_themes[i].id == id) {
            themeIdx = i;
            break;
        }
    }
    if (themeIdx < 0 && !m_themes.isEmpty()) {
        themeIdx = 0;
    }
    if (themeIdx < 0) {
        return;
    }

    auto &theme = m_themes[themeIdx];
    const bool isActive = (theme.id == m_activeThemeId);
    m_ui->groupActiveThemeParams->setTitle(QStringLiteral("Theme Customizer & Tweaks: %1 (%2)")
        .arg(theme.name, isActive ? QStringLiteral("Active") : QStringLiteral("Selected")));

    // Clear layout
    QLayoutItem *item;
    while ((item = m_ui->formLayoutThemeParams->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    if (theme.parameters.isEmpty()) {
        auto *lbl = new QLabel(QStringLiteral("This visual theme does not expose any custom tweakable parameters."), m_ui->groupActiveThemeParams);
        lbl->setStyleSheet(QStringLiteral("color: palette(placeholder-text); font-style: italic;"));
        m_ui->formLayoutThemeParams->addRow(lbl);
        return;
    }

    for (int pIdx = 0; pIdx < theme.parameters.size(); ++pIdx) {
        auto &param = theme.parameters[pIdx];
        if (param.type == QStringLiteral("float")) {
            auto *spin = new QDoubleSpinBox(m_ui->groupActiveThemeParams);
            spin->setRange(param.minValue, param.maxValue);
            spin->setSingleStep(param.step);
            spin->setDecimals(2);
            spin->setValue(param.currentValue);
            if (!param.suffix.isEmpty()) {
                spin->setSuffix(param.suffix);
            }
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [&param, this](double val) {
                param.currentValue = val;
                markAsChanged();
            });
            m_ui->formLayoutThemeParams->addRow(param.name + QStringLiteral(":"), spin);
        } else if (param.type == QStringLiteral("int")) {
            auto *spin = new QSpinBox(m_ui->groupActiveThemeParams);
            spin->setRange(qRound(param.minValue), qRound(param.maxValue));
            spin->setSingleStep(qMax(1, qRound(param.step)));
            spin->setValue(qRound(param.currentValue));
            if (!param.suffix.isEmpty()) {
                spin->setSuffix(param.suffix);
            }
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [&param, this](int val) {
                param.currentValue = val;
                markAsChanged();
            });
            m_ui->formLayoutThemeParams->addRow(param.name + QStringLiteral(":"), spin);
        } else if (param.type == QStringLiteral("bool")) {
            auto *cb = new QCheckBox(param.name, m_ui->groupActiveThemeParams);
            cb->setChecked(param.currentValue > 0.5);
            connect(cb, &QCheckBox::toggled, this, [&param, this](bool checked) {
                param.currentValue = checked ? 1.0 : 0.0;
                markAsChanged();
            });
            m_ui->formLayoutThemeParams->addRow(QString(), cb);
        } else if (param.type == QStringLiteral("color")) {
            auto *btn = new KColorButton(m_ui->groupActiveThemeParams);
            btn->setColor(QColor(param.colorCurrent));
            connect(btn, &KColorButton::changed, this, [&param, this](const QColor &col) {
                param.colorCurrent = col.name(QColor::HexRgb);
                markAsChanged();
            });
            m_ui->formLayoutThemeParams->addRow(param.name + QStringLiteral(":"), btn);
        } else if (param.type == QStringLiteral("enum")) {
            auto *combo = new QComboBox(m_ui->groupActiveThemeParams);
            combo->addItems(param.enumOptions);
            const int idx = param.enumOptions.indexOf(param.enumCurrent.isEmpty() ? param.enumDefault : param.enumCurrent);
            if (idx >= 0) {
                combo->setCurrentIndex(idx);
            }
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&param, combo, this](int) {
                param.enumCurrent = combo->currentText();
                markAsChanged();
            });
            m_ui->formLayoutThemeParams->addRow(param.name + QStringLiteral(":"), combo);
        }
    }
}

void ActiveWindowOutlineConfig::saveThemeParameterValues()
{
    for (const auto &theme : m_themes) {
        if (theme.id == m_activeThemeId) {
            m_themeUserOverrides = theme.exportParameterValues();
            break;
        }
    }
}

void ActiveWindowOutlineConfig::resetThemeParameters()
{
    const int r = m_ui->tableThemes->currentRow();
    if (r < 0 || r >= m_themes.size()) {
        return;
    }
    auto &theme = m_themes[r];
    for (auto &p : theme.parameters) {
        p.currentValue = p.defaultValue;
        p.colorCurrent = p.colorDefault;
        p.enumCurrent = p.enumDefault;
    }
    if (theme.id == m_activeThemeId) {
        m_themeUserOverrides = QJsonObject();
    }
    m_currentDisplayedThemeId.clear();
    updateThemeParametersUIFor(theme.id);
    markAsChanged();
}

void ActiveWindowOutlineConfig::importTheme()
{
    const QString filePath = QFileDialog::getOpenFileName(
        widget(),
        QStringLiteral("Import LeapK Theme"),
        QString(),
        QStringLiteral("LeapK Themes (*.leapk-theme *.json);;All Files (*)")
    );
    if (filePath.isEmpty()) {
        return;
    }

    LeapKTheme::Theme imported;
    QString err;
    if (!LeapKTheme::loadThemeFromFile(filePath, imported, &err)) {
        QMessageBox::critical(widget(), QStringLiteral("Import Error"),
            QStringLiteral("Failed to parse theme file:\n%1").arg(err));
        return;
    }

    const QString destDir = LeapKTheme::defaultThemesDirectory();
    QDir().mkpath(destDir);
    const QString destFile = destDir + QStringLiteral("/") + QFileInfo(filePath).fileName();
    if (!LeapKTheme::saveThemeToFile(imported, destFile)) {
        QMessageBox::critical(widget(), QStringLiteral("Save Error"),
            QStringLiteral("Could not save imported theme to:\n%1").arg(destFile));
        return;
    }

    refreshThemesTable();
    QMessageBox::information(widget(), QStringLiteral("Theme Imported"),
        QStringLiteral("Theme '%1' was successfully imported!").arg(imported.name));
}

void ActiveWindowOutlineConfig::exportSelectedTheme()
{
    const int r = m_ui->tableThemes->currentRow();
    if (r < 0 || r >= m_themes.size()) {
        return;
    }
    const auto &theme = m_themes[r];

    const QString filePath = QFileDialog::getSaveFileName(
        widget(),
        QStringLiteral("Export LeapK Theme"),
        QDir::homePath() + QStringLiteral("/%1.leapk-theme").arg(theme.id),
        QStringLiteral("LeapK Themes (*.leapk-theme);;JSON Files (*.json)")
    );
    if (filePath.isEmpty()) {
        return;
    }

    if (!LeapKTheme::saveThemeToFile(theme, filePath)) {
        QMessageBox::critical(widget(), QStringLiteral("Export Error"),
            QStringLiteral("Could not write theme to file:\n%1").arg(filePath));
        return;
    }

    QMessageBox::information(widget(), QStringLiteral("Theme Exported"),
        QStringLiteral("Theme '%1' exported successfully to:\n%2").arg(theme.name, filePath));
}

#include "leapk_config.moc"
