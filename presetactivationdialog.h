/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "presethelper.h"

#include <QDialog>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QLocale>
#include <QPushButton>
#include <QSet>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace LeapK
{

struct PresetSettingItem {
    QString key;
    QString labelPl;
    QString labelEn;
};

struct PresetCategory {
    QString titlePl;
    QString titleEn;
    QString iconTheme;
    QList<PresetSettingItem> items;
};

class PresetActivationDialog
{
public:
    static QList<PresetCategory> getCategories()
    {
        return QList<PresetCategory>{
            // 1. Geometry & Border
            {
                QStringLiteral("📐 Geometria i styl ramki"),
                QStringLiteral("📐 Geometry & Border Style"),
                QStringLiteral("draw-rectangle"),
                {
                    {QStringLiteral("BorderWidth"), QStringLiteral("Szerokość obrysu"), QStringLiteral("Border Width")},
                    {QStringLiteral("OutlineGap"), QStringLiteral("Odstęp obrysu od okna"), QStringLiteral("Outline Gap")},
                    {QStringLiteral("BorderRadius"), QStringLiteral("Zaokrąglenie narożników"), QStringLiteral("Corner Radius")},
                    {QStringLiteral("FadeCorners"), QStringLiteral("Wygładzanie narożników"), QStringLiteral("Fade Corners")},
                    {QStringLiteral("DrawInside"), QStringLiteral("Rysuj wewnątrz okna"), QStringLiteral("Draw Inside Window")},
                    {QStringLiteral("SmoothEdges"), QStringLiteral("Wygładzanie krawędzi (AA)"), QStringLiteral("Smooth Edges")},
                    {QStringLiteral("GlowEnabled"), QStringLiteral("Efekt poświaty (Glow)"), QStringLiteral("Outer Glow")},
                    {QStringLiteral("GlowRadius"), QStringLiteral("Promień poświaty"), QStringLiteral("Glow Radius")}
                }
            },
            // 2. Colors & Opacity
            {
                QStringLiteral("🎨 Kolorystyka i przezroczystość"),
                QStringLiteral("🎨 Colors & Opacity"),
                QStringLiteral("color-management"),
                {
                    {QStringLiteral("UseAccentColor"), QStringLiteral("Użyj koloru akcentu KDE"), QStringLiteral("Use KDE Accent Color")},
                    {QStringLiteral("CustomColor"), QStringLiteral("Własny kolor obrysu"), QStringLiteral("Custom Outline Color")},
                    {QStringLiteral("AccentOpacity"), QStringLiteral("Przezroczystość ramki"), QStringLiteral("Outline Opacity")},
                    {QStringLiteral("EnsurePerimeterContrast"), QStringLiteral("Ochrona kontrastu obrysu"), QStringLiteral("Perimeter Contrast Safeguard")}
                }
            },
            // 3. Dynamic Ambilight
            {
                QStringLiteral("🌈 Dynamiczny Ambilight (Próbkowanie barw)"),
                QStringLiteral("🌈 Dynamic Ambilight (Color Sampling)"),
                QStringLiteral("video-display"),
                {
                    {QStringLiteral("AmbilightEnabled"), QStringLiteral("Włącz dynamiczny Ambilight"), QStringLiteral("Enable Ambilight")},
                    {QStringLiteral("AmbilightMode"), QStringLiteral("Tryb próbkowania kolorów"), QStringLiteral("Color Sampling Mode")},
                    {QStringLiteral("AmbilightOpacity"), QStringLiteral("Przezroczystość Ambilight"), QStringLiteral("Ambilight Opacity")},
                    {QStringLiteral("AmbilightFrequency"), QStringLiteral("Częstotliwość próbkowania"), QStringLiteral("Sampling Frequency")},
                    {QStringLiteral("AmbilightDuration"), QStringLiteral("Czas płynnego przejścia"), QStringLiteral("Transition Duration")},
                    {QStringLiteral("AmbilightSaturationLeveling"), QStringLiteral("Korekta nasycenia barw"), QStringLiteral("Saturation Leveling")},
                    {QStringLiteral("AmbilightFallbackStrength"), QStringLiteral("Siła koloru zapasowego"), QStringLiteral("Fallback Strength")},
                    {QStringLiteral("AmbilightMinBrightness"), QStringLiteral("Minimalna jasność"), QStringLiteral("Minimum Brightness")},
                    {QStringLiteral("AmbilightMaxBrightness"), QStringLiteral("Maksymalna jasność"), QStringLiteral("Maximum Brightness")},
                    {QStringLiteral("AmbilightEnsureContrast"), QStringLiteral("Ochrona kontrastu Ambilight"), QStringLiteral("Ambilight Contrast Safeguard")}
                }
            },
            // 4. Shader Effects & Animations
            {
                QStringLiteral("⚡ Efekty shaderowe i animacje"),
                QStringLiteral("⚡ Shader Effects & Animations"),
                QStringLiteral("preferences-desktop-effects"),
                {
                    {QStringLiteral("BreatheEnabled"), QStringLiteral("Efekt pulsowania / oddychania"), QStringLiteral("Breathing Pulse")},
                    {QStringLiteral("BreatheSpeed"), QStringLiteral("Szybkość oddychania"), QStringLiteral("Breathing Speed")},
                    {QStringLiteral("BreatheIntensity"), QStringLiteral("Głębokość oddychania"), QStringLiteral("Breathing Intensity")},
                    {QStringLiteral("GlitchEnabled"), QStringLiteral("Efekt zakłóceń (Glitch)"), QStringLiteral("Glitch Effect")},
                    {QStringLiteral("GlitchSpeed"), QStringLiteral("Szybkość glitcha"), QStringLiteral("Glitch Speed")},
                    {QStringLiteral("GlitchIntensity"), QStringLiteral("Intensywność glitcha"), QStringLiteral("Glitch Intensity")},
                    {QStringLiteral("GlitchAnomalyEnabled"), QStringLiteral("Anomalia glitcha"), QStringLiteral("Glitch Anomaly")},
                    {QStringLiteral("GlitchFrequency"), QStringLiteral("Częstotliwość anomalii glitcha"), QStringLiteral("Anomaly Frequency")},
                    {QStringLiteral("NeonFlickerEnabled"), QStringLiteral("Migotanie neonu (Neon Flicker)"), QStringLiteral("Neon Flicker")},
                    {QStringLiteral("NeonFlickerInterval"), QStringLiteral("Odstęp migotania neonu"), QStringLiteral("Neon Flicker Interval")},
                    {QStringLiteral("NeonFlickerDuration"), QStringLiteral("Czas trwania migotania"), QStringLiteral("Neon Flicker Duration")},
                    {QStringLiteral("NeonFlickerIntensity"), QStringLiteral("Intensywność migotania"), QStringLiteral("Neon Flicker Intensity")},
                    {QStringLiteral("FlashEnabled"), QStringLiteral("Efekt błysku (Flash)"), QStringLiteral("Flash Pulse Effect")},
                    {QStringLiteral("FlashInterval"), QStringLiteral("Odstęp błysku"), QStringLiteral("Flash Interval")},
                    {QStringLiteral("FlashDuration"), QStringLiteral("Czas trwania błysku"), QStringLiteral("Flash Duration")},
                    {QStringLiteral("ColorCycleEnabled"), QStringLiteral("Pętla sekwencji barw"), QStringLiteral("Color Cycling")},
                    {QStringLiteral("ColorTransitionDuration"), QStringLiteral("Czas przejścia barw sekwencji"), QStringLiteral("Cycle Duration")},
                    {QStringLiteral("ColorCycleOrder"), QStringLiteral("Kolejność sekwencji"), QStringLiteral("Cycle Order")},
                    {QStringLiteral("ColorCycleEasing"), QStringLiteral("Krzywa animacji sekwencji"), QStringLiteral("Cycle Easing")},
                    {QStringLiteral("ColorList"), QStringLiteral("Paleta kolorów sekwencji"), QStringLiteral("Color Palette")},
                    {QStringLiteral("CustomShaderEnabled"), QStringLiteral("Niestandardowy efekt shadera GLSL"), QStringLiteral("Custom GLSL Shader")},
                    {QStringLiteral("ActiveCustomShaderId"), QStringLiteral("Aktywny shader GLSL"), QStringLiteral("Active Shader ID")}
                }
            },
            // 5. Leap Mode & Badges
            {
                QStringLiteral("🚀 Nawigacja Leap i plakietki"),
                QStringLiteral("🚀 Leap Mode & Badges"),
                QStringLiteral("input-keyboard"),
                {
                    {QStringLiteral("BadgeDisplayMode"), QStringLiteral("Tryb wyświetlania plakietek"), QStringLiteral("Badge Display Mode")},
                    {QStringLiteral("JumpTargets"), QStringLiteral("Zasięg celów nawigacji Leap"), QStringLiteral("Jump Targets Scope")},
                    {QStringLiteral("BadgePosition"), QStringLiteral("Pozycja plakietki na oknie"), QStringLiteral("Window Badge Position")},
                    {QStringLiteral("BadgeHudPosition"), QStringLiteral("Pozycja paska kart HUD"), QStringLiteral("Screen HUD Position")},
                    {QStringLiteral("BadgeSize"), QStringLiteral("Rozmiar plakietek literowych"), QStringLiteral("Badge Size")},
                    {QStringLiteral("BadgeMargin"), QStringLiteral("Margines od krawędzi okna"), QStringLiteral("Badge Margin")},
                    {QStringLiteral("MarkShowLetterBadges"), QStringLiteral("Pokaż plakietki literowe"), QStringLiteral("Show Letter Badges")},
                    {QStringLiteral("BadgesAlwaysVisible"), QStringLiteral("Plakietki stale widoczne w tle"), QStringLiteral("Badges Always Visible")}
                }
            },
            // 6. Window Marks
            {
                QStringLiteral("🏷️ Oznaczanie okien (Marks)"),
                QStringLiteral("🏷️ Window Marks"),
                QStringLiteral("bookmark-new"),
                {
                    {QStringLiteral("MarkDefaultColor"), QStringLiteral("Domyślny kolor oznaczenia"), QStringLiteral("Default Mark Color")},
                    {QStringLiteral("MarkSmartAdaptive"), QStringLiteral("Inteligentna barwa adaptacyjna"), QStringLiteral("Smart Adaptive Color")},
                    {QStringLiteral("MarkRandomColor"), QStringLiteral("Losowy kolor oznaczenia"), QStringLiteral("Random Mark Color")},
                    {QStringLiteral("StylePrecedence"), QStringLiteral("Kolejność priorytetów stylów"), QStringLiteral("Style Precedence Order")},
                    {QStringLiteral("MarkInactiveWidthReduction"), QStringLiteral("Zwężenie ramki nieaktywnych"), QStringLiteral("Inactive Width Reduction")},
                    {QStringLiteral("MarkInactiveDashed"), QStringLiteral("Przerywana linia nieaktywnych"), QStringLiteral("Dashed Inactive Outline")},
                    {QStringLiteral("MarkInactiveDashGap"), QStringLiteral("Odstęp myślników nieaktywnych"), QStringLiteral("Inactive Dash Gap")},
                    {QStringLiteral("MarkInactiveDashSmooth"), QStringLiteral("Wygładzone myślniki nieaktywnych"), QStringLiteral("Smooth Inactive Dashes")}
                }
            },
            // 7. Focus History & Behavior
            {
                QStringLiteral("🕒 Historia fokusu i zachowanie"),
                QStringLiteral("🕒 Focus History & Behavior"),
                QStringLiteral("preferences-system-windows"),
                {
                    {QStringLiteral("HighlightRecentWindow"), QStringLiteral("Podświetlanie poprzednich okien"), QStringLiteral("Highlight Recent Windows")},
                    {QStringLiteral("RecentWindowsCount"), QStringLiteral("Liczba śledzonych okien"), QStringLiteral("Recent Windows Count")},
                    {QStringLiteral("RecentWindowColorMode"), QStringLiteral("Tryb koloru historii"), QStringLiteral("Recent Windows Color Mode")},
                    {QStringLiteral("RecentWindowCustomColor"), QStringLiteral("Własny kolor historii"), QStringLiteral("Custom Recent Color")},
                    {QStringLiteral("RecentWindowOpacity"), QStringLiteral("Krycie obrysu historii"), QStringLiteral("Recent Window Opacity")},
                    {QStringLiteral("RecentWindowHueShift"), QStringLiteral("Przesunięcie barwy w historii"), QStringLiteral("Recent Hue Shift")},
                    {QStringLiteral("RecentWindowSaturation"), QStringLiteral("Nasycenie barwy w historii"), QStringLiteral("Recent Saturation")},
                    {QStringLiteral("RecentWindowWidthReduction"), QStringLiteral("Zwężenie ramki w historii"), QStringLiteral("Recent Width Reduction")},
                    {QStringLiteral("RecentWindowDashed"), QStringLiteral("Przerywana ramka w historii"), QStringLiteral("Dashed Recent Outline")},
                    {QStringLiteral("RecentWindowGapGrowth"), QStringLiteral("Wzrost odstępu w historii"), QStringLiteral("Recent Gap Growth")},
                    {QStringLiteral("RecentWindowDashSmooth"), QStringLiteral("Wygładzone myślniki w historii"), QStringLiteral("Smooth Recent Dashes")},
                    {QStringLiteral("FadeDuration"), QStringLiteral("Czas zanikania obrysu"), QStringLiteral("Outline Fade Duration")},
                    {QStringLiteral("MenuHideDelay"), QStringLiteral("Opóźnienie ukrywania menu"), QStringLiteral("Menu Hide Delay")},
                    {QStringLiteral("MenuShowDelay"), QStringLiteral("Opóźnienie pokazywania menu"), QStringLiteral("Menu Show Delay")},
                    {QStringLiteral("WindowHideDelay"), QStringLiteral("Opóźnienie ukrywania okna"), QStringLiteral("Window Hide Delay")},
                    {QStringLiteral("WindowShowDelay"), QStringLiteral("Opóźnienie pokazywania okna"), QStringLiteral("Window Show Delay")},
                    {QStringLiteral("HideOnFullScreen"), QStringLiteral("Ukryj na pełnym ekranie"), QStringLiteral("Hide Outline on Full Screen")},
                    {QStringLiteral("TrayIconEnabled"), QStringLiteral("Ikona w zasobniku systemowym"), QStringLiteral("System Tray Icon")}
                }
            }
        };
    }

    static QString formatValue(const QString &key, const QJsonValue &val, bool isPolish)
    {
        if (val.isBool()) {
            return val.toBool()
                ? (isPolish ? QStringLiteral("Włączone") : QStringLiteral("Enabled"))
                : (isPolish ? QStringLiteral("Wyłączone") : QStringLiteral("Disabled"));
        }
        if (key == QStringLiteral("BadgeDisplayMode")) {
            const int m = val.toInt();
            if (m == 0) return isPolish ? QStringLiteral("Tylko w trybie skoku (Meta+Space)") : QStringLiteral("Only in Jump Mode");
            if (m == 1) return isPolish ? QStringLiteral("Zawsze widoczne w tle") : QStringLiteral("Always visible on background");
            return isPolish ? QStringLiteral("Wyłączone") : QStringLiteral("Disabled");
        }
        if (key == QStringLiteral("JumpTargets")) {
            const int t = val.toInt();
            if (t == 0) return isPolish ? QStringLiteral("Bieżący pulpit (widoczne) + oznaczone") : QStringLiteral("Current desktop (visible) + marked");
            if (t == 1) return isPolish ? QStringLiteral("Bieżący pulpit (widoczne i zmin.) + oznaczone") : QStringLiteral("Current desktop (visible & min) + marked");
            if (t == 2) return isPolish ? QStringLiteral("Wszystkie pulpity (widoczne i zmin.)") : QStringLiteral("All desktops (visible & min)");
            return isPolish ? QStringLiteral("Tylko oznaczone okna") : QStringLiteral("Marked windows only");
        }
        if (key == QStringLiteral("BadgeHudPosition")) {
            const int p = val.toInt();
            if (p == 0) return isPolish ? QStringLiteral("Dół ekranu (Bottom)") : QStringLiteral("Bottom");
            if (p == 1) return isPolish ? QStringLiteral("Góra ekranu (Top)") : QStringLiteral("Top");
            if (p == 2) return isPolish ? QStringLiteral("Lewa krawędź (Left)") : QStringLiteral("Left");
            return isPolish ? QStringLiteral("Prawa krawędź (Right)") : QStringLiteral("Right");
        }
        if (key == QStringLiteral("BadgePosition")) {
            const int p = val.toInt();
            const char *posPl[] = {"Góra-lewo", "Góra", "Góra-prawo", "Lewo", "Środek", "Prawo", "Dół-lewo", "Dół", "Dół-prawo"};
            const char *posEn[] = {"Top-Left", "Top", "Top-Right", "Left", "Center", "Right", "Bottom-Left", "Bottom", "Bottom-Right"};
            if (p >= 0 && p < 9) {
                return isPolish ? QString::fromUtf8(posPl[p]) : QString::fromUtf8(posEn[p]);
            }
        }
        if (key == QStringLiteral("RecentWindowColorMode")) {
            const int m = val.toInt();
            if (m == 0) return isPolish ? QStringLiteral("Przyciemniony kolor aktywny") : QStringLiteral("Dimmed active color");
            if (m == 1) return isPolish ? QStringLiteral("Własny kolor") : QStringLiteral("Custom color");
            return isPolish ? QStringLiteral("Skala szarości") : QStringLiteral("Grayscale");
        }
        if (key == QStringLiteral("AmbilightMode")) {
            const int m = val.toInt();
            if (m == 0) return isPolish ? QStringLiteral("Średni kolor (Dominant)") : QStringLiteral("Dominant average");
            return isPolish ? QStringLiteral("Częściowy obwód (Perimeter)") : QStringLiteral("Perimeter segments");
        }
        if (key.endsWith(QStringLiteral("Width")) || key.endsWith(QStringLiteral("Radius")) ||
            key == QStringLiteral("BadgeSize") || key == QStringLiteral("BadgeMargin") ||
            key == QStringLiteral("OutlineGap") || key == QStringLiteral("MarkInactiveDashGap") ||
            key == QStringLiteral("RecentWindowGapGrowth")) {
            return QStringLiteral("%1 px").arg(val.toInt());
        }
        if (key.endsWith(QStringLiteral("Delay")) || (key.endsWith(QStringLiteral("Duration")) && !val.isDouble())) {
            return QStringLiteral("%1 ms").arg(val.toInt());
        }
        if (val.isDouble()) {
            if (key.endsWith(QStringLiteral("Duration")) || key.endsWith(QStringLiteral("Interval")) || key == QStringLiteral("ColorTransitionDuration")) {
                return QStringLiteral("%1 s").arg(val.toDouble(), 0, 'f', 2);
            }
            if (key.endsWith(QStringLiteral("Frequency"))) {
                return QStringLiteral("%1 Hz").arg(val.toDouble(), 0, 'f', 1);
            }
            return QString::number(val.toDouble(), 'g', 3);
        }
        if (key.endsWith(QStringLiteral("Opacity")) || key.endsWith(QStringLiteral("Intensity")) ||
            key == QStringLiteral("AmbilightSaturationLeveling") || key == QStringLiteral("AmbilightFallbackStrength") ||
            key == QStringLiteral("AmbilightMinBrightness") || key == QStringLiteral("AmbilightMaxBrightness")) {
            return QStringLiteral("%1%").arg(val.toInt());
        }
        if (val.isString()) {
            const QString s = val.toString();
            if (s.length() > 26) {
                return s.left(23) + QStringLiteral("…");
            }
            return s;
        }
        return QString::number(val.toInt());
    }

    static bool areValuesEqual(const QJsonValue &v1, const QJsonValue &v2)
    {
        if (v1.type() != v2.type()) {
            if ((v1.isDouble() || v1.isDouble()) && (v2.isDouble() || v2.isDouble())) {
                return qFuzzyCompare(v1.toDouble() + 1.0, v2.toDouble() + 1.0);
            }
            return false;
        }
        if (v1.isBool()) return v1.toBool() == v2.toBool();
        if (v1.isDouble()) return qFuzzyCompare(v1.toDouble() + 1.0, v2.toDouble() + 1.0);
        if (v1.isString()) return v1.toString().compare(v2.toString(), Qt::CaseInsensitive) == 0;
        return v1.toInt() == v2.toInt();
    }

    static bool selectSettings(QWidget *parent,
                               const OutlinePreset::PresetInfo &preset,
                               const QJsonObject &currentSettings,
                               QJsonObject &outSettingsToApply,
                               bool isPolish)
    {
        QDialog dlg(parent);
        dlg.setWindowTitle(isPolish
            ? QStringLiteral("Wybierz elementy do aktywacji z presetu: %1").arg(preset.name)
            : QStringLiteral("Select Elements to Apply from Preset: %1").arg(preset.name));
        dlg.setWindowIcon(QIcon::fromTheme(QStringLiteral("leapk"), QIcon(QStringLiteral(":/icons/leapk.svg"))));
        dlg.resize(760, 600);

        auto *mainLayout = new QVBoxLayout(&dlg);
        mainLayout->setContentsMargins(16, 16, 16, 16);
        mainLayout->setSpacing(10);

        // Header
        auto *headerLabel = new QLabel(&dlg);
        headerLabel->setTextFormat(Qt::RichText);
        headerLabel->setWordWrap(true);
        const QString desc = preset.description.isEmpty()
            ? (isPolish ? QStringLiteral("Brak opisu.") : QStringLiteral("No description."))
            : preset.description;
        headerLabel->setText(isPolish
            ? QStringLiteral("<h3>Preset: <b>%1</b></h3>"
                             "<p style='color: #888;'>%2</p>"
                             "<p>Wybierz sekcje lub pojedyncze opcje, które chcesz zastosować. "
                             "Odznaczone pozycje zachowają Twoje bieżące wartości.</p>")
                .arg(preset.name, desc)
            : QStringLiteral("<h3>Preset: <b>%1</b></h3>"
                             "<p style='color: #888;'>%2</p>"
                             "<p>Select the categories or individual settings to apply. "
                             "Unchecked options will keep your current configuration intact.</p>")
                .arg(preset.name, desc));
        mainLayout->addWidget(headerLabel);

        // Quick toolbar & search filter
        auto *toolsLayout = new QHBoxLayout();
        auto *btnSelectAll = new QPushButton(isPolish ? QStringLiteral("☑ Zaznacz wszystko") : QStringLiteral("☑ Select All"), &dlg);
        auto *btnDeselectAll = new QPushButton(isPolish ? QStringLiteral("☐ Odznacz wszystko") : QStringLiteral("☐ Deselect All"), &dlg);
        auto *btnOnlyDiff = new QPushButton(&dlg);

        toolsLayout->addWidget(btnSelectAll);
        toolsLayout->addWidget(btnDeselectAll);
        toolsLayout->addWidget(btnOnlyDiff);
        toolsLayout->addSpacing(16);

        auto *searchBox = new QLineEdit(&dlg);
        searchBox->setPlaceholderText(isPolish ? QStringLiteral("🔍 Filtruj opcje…") : QStringLiteral("🔍 Filter options…"));
        searchBox->setClearButtonEnabled(true);
        toolsLayout->addWidget(searchBox);

        mainLayout->addLayout(toolsLayout);

        // Options Tree
        auto *tree = new QTreeWidget(&dlg);
        tree->setHeaderLabels(isPolish
            ? QStringList{QStringLiteral("Opcja / Kategoria"), QStringLiteral("Wartość w presecie"), QStringLiteral("Obecna wartość")}
            : QStringList{QStringLiteral("Setting / Category"), QStringLiteral("Preset Value"), QStringLiteral("Current Value")});
        tree->setAlternatingRowColors(true);
        tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        mainLayout->addWidget(tree);

        const auto categories = getCategories();
        int totalDiffCount = 0;

        for (const auto &cat : categories) {
            auto *catItem = new QTreeWidgetItem(tree);
            catItem->setText(0, isPolish ? cat.titlePl : cat.titleEn);
            catItem->setIcon(0, QIcon::fromTheme(cat.iconTheme));
            catItem->setFlags(catItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
            catItem->setCheckState(0, Qt::Checked);

            QFont f = catItem->font(0);
            f.setBold(true);
            catItem->setFont(0, f);

            for (const auto &it : cat.items) {
                auto *child = new QTreeWidgetItem(catItem);
                child->setText(0, isPolish ? it.labelPl : it.labelEn);
                child->setData(0, Qt::UserRole, it.key);
                child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
                child->setCheckState(0, Qt::Checked);

                const QJsonValue presetVal = preset.settings.value(it.key);
                const QJsonValue currentVal = currentSettings.value(it.key);
                const bool differs = !areValuesEqual(presetVal, currentVal);

                if (differs) {
                    totalDiffCount++;
                }
                child->setData(0, Qt::UserRole + 1, differs);

                const QString presetStr = formatValue(it.key, presetVal, isPolish);
                const QString currentStr = formatValue(it.key, currentVal, isPolish);

                child->setText(1, presetStr);
                child->setText(2, currentStr);

                if (differs) {
                    child->setForeground(1, QBrush(QColor(QStringLiteral("#2ecc71"))));
                    child->setToolTip(1, isPolish ? QStringLiteral("Wartość różni się od Twojej obecnej konfiguracji")
                                                  : QStringLiteral("Value differs from your current configuration"));
                } else {
                    child->setForeground(1, QBrush(QColor(QStringLiteral("#888888"))));
                }
            }
            catItem->setExpanded(true);
        }

        btnOnlyDiff->setText(isPolish
            ? QStringLiteral("⚡ Tylko różnice (%1)").arg(totalDiffCount)
            : QStringLiteral("⚡ Differences Only (%1)").arg(totalDiffCount));

        // Bottom action layout
        auto *bottomLayout = new QHBoxLayout();
        auto *lblCount = new QLabel(&dlg);
        bottomLayout->addWidget(lblCount);
        bottomLayout->addStretch();

        auto *btnApplySelected = new QPushButton(isPolish ? QStringLiteral("✓ Zastosuj zaznaczone") : QStringLiteral("✓ Apply Selected"), &dlg);
        btnApplySelected->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")));

        auto *btnApplyAll = new QPushButton(isPolish ? QStringLiteral("⚡ Zastosuj całość") : QStringLiteral("⚡ Apply All"), &dlg);
        btnApplyAll->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok")));

        auto *btnCancel = new QPushButton(isPolish ? QStringLiteral("Anuluj") : QStringLiteral("Cancel"), &dlg);
        btnCancel->setIcon(QIcon::fromTheme(QStringLiteral("dialog-cancel")));

        bottomLayout->addWidget(btnApplySelected);
        bottomLayout->addWidget(btnApplyAll);
        bottomLayout->addWidget(btnCancel);
        mainLayout->addLayout(bottomLayout);

        bool updatingChecks = false;

        auto updateCountLabel = [&]() {
            int checked = 0;
            int total = 0;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                for (int j = 0; j < catItem->childCount(); ++j) {
                    total++;
                    if (catItem->child(j)->checkState(0) == Qt::Checked) {
                        checked++;
                    }
                }
            }
            lblCount->setText(isPolish
                ? QStringLiteral("Wybrano: <b>%1</b> z %2 opcji").arg(checked).arg(total)
                : QStringLiteral("Selected: <b>%1</b> of %2 options").arg(checked).arg(total));
            btnApplySelected->setEnabled(checked > 0);
            btnApplySelected->setText(isPolish
                ? QStringLiteral("✓ Zastosuj zaznaczone (%1)").arg(checked)
                : QStringLiteral("✓ Apply Selected (%1)").arg(checked));
        };
        updateCountLabel();

        // Handle item check changes
        QObject::connect(tree, &QTreeWidget::itemChanged, [&](QTreeWidgetItem *item, int col) {
            if (updatingChecks || col != 0) return;
            updatingChecks = true;

            if (item->parent() == nullptr) {
                // Category parent clicked
                const Qt::CheckState state = item->checkState(0);
                if (state != Qt::PartiallyChecked) {
                    for (int i = 0; i < item->childCount(); ++i) {
                        item->child(i)->setCheckState(0, state);
                    }
                }
            } else {
                // Child clicked: update parent tristate
                auto *parentItem = item->parent();
                int checked = 0;
                for (int i = 0; i < parentItem->childCount(); ++i) {
                    if (parentItem->child(i)->checkState(0) == Qt::Checked) {
                        checked++;
                    }
                }
                if (checked == 0) {
                    parentItem->setCheckState(0, Qt::Unchecked);
                } else if (checked == parentItem->childCount()) {
                    parentItem->setCheckState(0, Qt::Checked);
                } else {
                    parentItem->setCheckState(0, Qt::PartiallyChecked);
                }
            }

            updatingChecks = false;
            updateCountLabel();
        });

        // Quick toolbar connects
        QObject::connect(btnSelectAll, &QPushButton::clicked, [&]() {
            updatingChecks = true;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                catItem->setCheckState(0, Qt::Checked);
                for (int j = 0; j < catItem->childCount(); ++j) {
                    catItem->child(j)->setCheckState(0, Qt::Checked);
                }
            }
            updatingChecks = false;
            updateCountLabel();
        });

        QObject::connect(btnDeselectAll, &QPushButton::clicked, [&]() {
            updatingChecks = true;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                catItem->setCheckState(0, Qt::Unchecked);
                for (int j = 0; j < catItem->childCount(); ++j) {
                    catItem->child(j)->setCheckState(0, Qt::Unchecked);
                }
            }
            updatingChecks = false;
            updateCountLabel();
        });

        QObject::connect(btnOnlyDiff, &QPushButton::clicked, [&]() {
            updatingChecks = true;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                int checked = 0;
                for (int j = 0; j < catItem->childCount(); ++j) {
                    auto *child = catItem->child(j);
                    const bool differs = child->data(0, Qt::UserRole + 1).toBool();
                    child->setCheckState(0, differs ? Qt::Checked : Qt::Unchecked);
                    if (differs) checked++;
                }
                if (checked == 0) {
                    catItem->setCheckState(0, Qt::Unchecked);
                } else if (checked == catItem->childCount()) {
                    catItem->setCheckState(0, Qt::Checked);
                } else {
                    catItem->setCheckState(0, Qt::PartiallyChecked);
                }
            }
            updatingChecks = false;
            updateCountLabel();
        });

        // Search filter connect
        QObject::connect(searchBox, &QLineEdit::textChanged, [&](const QString &text) {
            const QString query = text.trimmed();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                bool anyChildVisible = false;
                for (int j = 0; j < catItem->childCount(); ++j) {
                    auto *child = catItem->child(j);
                    const bool match = query.isEmpty() ||
                                       child->text(0).contains(query, Qt::CaseInsensitive) ||
                                       child->data(0, Qt::UserRole).toString().contains(query, Qt::CaseInsensitive);
                    child->setHidden(!match);
                    if (match) anyChildVisible = true;
                }
                const bool catMatch = catItem->text(0).contains(query, Qt::CaseInsensitive);
                catItem->setHidden(!anyChildVisible && !catMatch);
                if (!query.isEmpty() && (anyChildVisible || catMatch)) {
                    catItem->setExpanded(true);
                }
            }
        });

        bool confirmed = false;

        QObject::connect(btnApplyAll, &QPushButton::clicked, [&]() {
            outSettingsToApply = preset.settings;
            confirmed = true;
            dlg.accept();
        });

        QObject::connect(btnApplySelected, &QPushButton::clicked, [&]() {
            outSettingsToApply = QJsonObject();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                for (int j = 0; j < catItem->childCount(); ++j) {
                    auto *child = catItem->child(j);
                    if (child->checkState(0) == Qt::Checked) {
                        const QString key = child->data(0, Qt::UserRole).toString();
                        if (preset.settings.contains(key)) {
                            outSettingsToApply[key] = preset.settings[key];
                        }
                    }
                }
            }
            confirmed = true;
            dlg.accept();
        });

        QObject::connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

        dlg.exec();
        return confirmed;
    }
};

} // namespace LeapK
