#pragma once

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <cmath>

namespace LeapKTheme {

// ============================================================================
// 1. Theme Parameter (Knob exposed to LeapK Settings UI)
// ============================================================================

struct ThemeParameter {
    QString id;              // Identifier used in token references (e.g. "accentColor")
    QString name;            // Human-readable label displayed in settings (e.g. "Neon Accent Color")
    QString type;            // "color", "float", "int", "bool", "enum"
    double minValue = 0.0;
    double maxValue = 1.0;
    double step = 0.05;
    double defaultValue = 0.5;
    double currentValue = 0.5;
    QString suffix;          // e.g. " px", "%", " ms"
    QString colorDefault = QStringLiteral("#3498db");
    QString colorCurrent = QStringLiteral("#3498db");
    QStringList enumOptions; // List of choices for type == "enum"
    QString enumDefault;
    QString enumCurrent;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj[QStringLiteral("id")] = id;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("type")] = type;
        if (type == QStringLiteral("float") || type == QStringLiteral("int")) {
            obj[QStringLiteral("min")] = minValue;
            obj[QStringLiteral("max")] = maxValue;
            obj[QStringLiteral("step")] = step;
            obj[QStringLiteral("default")] = defaultValue;
            obj[QStringLiteral("value")] = currentValue;
            obj[QStringLiteral("suffix")] = suffix;
        } else if (type == QStringLiteral("bool")) {
            obj[QStringLiteral("default")] = defaultValue > 0.5;
            obj[QStringLiteral("value")] = currentValue > 0.5;
        } else if (type == QStringLiteral("color")) {
            obj[QStringLiteral("colorDefault")] = colorDefault;
            obj[QStringLiteral("colorValue")] = colorCurrent;
        } else if (type == QStringLiteral("enum")) {
            QJsonArray arr;
            for (const auto &opt : enumOptions) {
                arr.append(opt);
            }
            obj[QStringLiteral("options")] = arr;
            obj[QStringLiteral("default")] = enumDefault;
            obj[QStringLiteral("value")] = enumCurrent.isEmpty() ? enumDefault : enumCurrent;
        }
        return obj;
    }

    static ThemeParameter fromJson(const QJsonObject &obj)
    {
        ThemeParameter p;
        p.id = obj.value(QStringLiteral("id")).toString();
        p.name = obj.value(QStringLiteral("name")).toString(p.id);
        p.type = obj.value(QStringLiteral("type")).toString(QStringLiteral("float"));
        p.minValue = obj.value(QStringLiteral("min")).toDouble(0.0);
        p.maxValue = obj.value(QStringLiteral("max")).toDouble(1.0);
        p.step = obj.value(QStringLiteral("step")).toDouble(0.05);
        p.defaultValue = obj.value(QStringLiteral("default")).toDouble(0.5);
        p.currentValue = obj.contains(QStringLiteral("value"))
            ? obj.value(QStringLiteral("value")).toDouble(p.defaultValue)
            : p.defaultValue;
        p.suffix = obj.value(QStringLiteral("suffix")).toString();
        p.colorDefault = obj.value(QStringLiteral("colorDefault")).toString(QStringLiteral("#3498db"));
        p.colorCurrent = obj.contains(QStringLiteral("colorValue"))
            ? obj.value(QStringLiteral("colorValue")).toString(p.colorDefault)
            : p.colorDefault;

        if (obj.contains(QStringLiteral("options"))) {
            const QJsonArray arr = obj.value(QStringLiteral("options")).toArray();
            for (const auto &val : arr) {
                p.enumOptions.append(val.toString());
            }
        }
        p.enumDefault = obj.value(QStringLiteral("default")).toString();
        p.enumCurrent = obj.contains(QStringLiteral("value"))
            ? obj.value(QStringLiteral("value")).toString(p.enumDefault)
            : p.enumDefault;

        if (p.type == QStringLiteral("bool") && obj.contains(QStringLiteral("default"))) {
            p.defaultValue = obj.value(QStringLiteral("default")).toBool() ? 1.0 : 0.0;
            p.currentValue = obj.contains(QStringLiteral("value"))
                ? (obj.value(QStringLiteral("value")).toBool() ? 1.0 : 0.0)
                : p.defaultValue;
        }
        return p;
    }
};

// ============================================================================
// 2. Component Style Structures
// ============================================================================

struct WindowBadgeTheme {
    QString shape = QStringLiteral("pill"); // "pill", "rounded_rect", "sharp", "chamfered", "circle"
    int cornerRadius = 6;
    int chamferSize = 6;
    double borderWidth = 1.0;
    QString borderColor = QStringLiteral("$borderNormal");
    QString bgNormal = QStringLiteral("$bgCard");
    bool shadowEnabled = true;
    QString shadowColor = QStringLiteral("#00000066");
    double shadowRadius = 4.0;
    QString audioIndicatorStyle = QStringLiteral("speaker_icon"); // "speaker_icon", "soundwave_pulse", "minimal_dot"
    QString audioIndicatorColor = QStringLiteral("$audioColor");
    double audioIndicatorScale = 1.0;
    bool audioPulseBadge = true;
    double audioPulseSpeed = 1.0;
    double audioPulseIntensity = 1.0;

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QStringLiteral("shape")] = shape;
        o[QStringLiteral("cornerRadius")] = cornerRadius;
        o[QStringLiteral("chamferSize")] = chamferSize;
        o[QStringLiteral("borderWidth")] = borderWidth;
        o[QStringLiteral("borderColor")] = borderColor;
        o[QStringLiteral("bgNormal")] = bgNormal;
        o[QStringLiteral("shadowEnabled")] = shadowEnabled;
        o[QStringLiteral("shadowColor")] = shadowColor;
        o[QStringLiteral("shadowRadius")] = shadowRadius;
        o[QStringLiteral("audioIndicatorStyle")] = audioIndicatorStyle;
        o[QStringLiteral("audioIndicatorColor")] = audioIndicatorColor;
        o[QStringLiteral("audioIndicatorScale")] = audioIndicatorScale;
        o[QStringLiteral("audioPulseBadge")] = audioPulseBadge;
        o[QStringLiteral("audioPulseSpeed")] = audioPulseSpeed;
        o[QStringLiteral("audioPulseIntensity")] = audioPulseIntensity;
        return o;
    }

    static WindowBadgeTheme fromJson(const QJsonObject &o)
    {
        WindowBadgeTheme w;
        w.shape = o.value(QStringLiteral("shape")).toString(w.shape);
        w.cornerRadius = o.value(QStringLiteral("cornerRadius")).toInt(w.cornerRadius);
        w.chamferSize = o.value(QStringLiteral("chamferSize")).toInt(w.chamferSize);
        w.borderWidth = o.value(QStringLiteral("borderWidth")).toDouble(w.borderWidth);
        w.borderColor = o.value(QStringLiteral("borderColor")).toString(w.borderColor);
        w.bgNormal = o.value(QStringLiteral("bgNormal")).toString(w.bgNormal);
        w.shadowEnabled = o.value(QStringLiteral("shadowEnabled")).toBool(w.shadowEnabled);
        w.shadowColor = o.value(QStringLiteral("shadowColor")).toString(w.shadowColor);
        w.shadowRadius = o.value(QStringLiteral("shadowRadius")).toDouble(w.shadowRadius);
        w.audioIndicatorStyle = o.value(QStringLiteral("audioIndicatorStyle")).toString(w.audioIndicatorStyle);
        w.audioIndicatorColor = o.value(QStringLiteral("audioIndicatorColor")).toString(w.audioIndicatorColor);
        w.audioIndicatorScale = o.value(QStringLiteral("audioIndicatorScale")).toDouble(w.audioIndicatorScale);
        w.audioPulseBadge = o.value(QStringLiteral("audioPulseBadge")).toBool(w.audioPulseBadge);
        w.audioPulseSpeed = o.value(QStringLiteral("audioPulseSpeed")).toDouble(w.audioPulseSpeed);
        w.audioPulseIntensity = o.value(QStringLiteral("audioPulseIntensity")).toDouble(w.audioPulseIntensity);
        return w;
    }
};

struct HudContainerTheme {
    QString dockStyle = QStringLiteral("floating_dock"); // "floating_dock", "screen_edge"
    QString backgroundStyle = QStringLiteral("glassmorphism"); // "solid", "gradient", "glassmorphism", "wireframe"
    double bgOpacity = 0.92;
    QString shape = QStringLiteral("rounded_rect"); // "rounded_rect", "chamfered", "sharp", "pill"
    int cornerRadius = 8;
    int chamferSize = 8;
    double borderWidth = 1.5;
    QString borderColor = QStringLiteral("$borderColor");
    bool glowEnabled = false;
    QString glowColor = QStringLiteral("$accentColor");
    double glowRadius = 8.0;
    bool shadowEnabled = true;
    QString shadowColor = QStringLiteral("#00000088");
    double shadowRadius = 12.0;

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QStringLiteral("dockStyle")] = dockStyle;
        o[QStringLiteral("backgroundStyle")] = backgroundStyle;
        o[QStringLiteral("bgOpacity")] = bgOpacity;
        o[QStringLiteral("shape")] = shape;
        o[QStringLiteral("cornerRadius")] = cornerRadius;
        o[QStringLiteral("chamferSize")] = chamferSize;
        o[QStringLiteral("borderWidth")] = borderWidth;
        o[QStringLiteral("borderColor")] = borderColor;
        o[QStringLiteral("glowEnabled")] = glowEnabled;
        o[QStringLiteral("glowColor")] = glowColor;
        o[QStringLiteral("glowRadius")] = glowRadius;
        o[QStringLiteral("shadowEnabled")] = shadowEnabled;
        o[QStringLiteral("shadowColor")] = shadowColor;
        o[QStringLiteral("shadowRadius")] = shadowRadius;
        return o;
    }

    static HudContainerTheme fromJson(const QJsonObject &o)
    {
        HudContainerTheme c;
        c.dockStyle = o.value(QStringLiteral("dockStyle")).toString(c.dockStyle);
        c.backgroundStyle = o.value(QStringLiteral("backgroundStyle")).toString(c.backgroundStyle);
        c.bgOpacity = o.value(QStringLiteral("bgOpacity")).toDouble(c.bgOpacity);
        c.shape = o.value(QStringLiteral("shape")).toString(c.shape);
        c.cornerRadius = o.value(QStringLiteral("cornerRadius")).toInt(c.cornerRadius);
        c.chamferSize = o.value(QStringLiteral("chamferSize")).toInt(c.chamferSize);
        c.borderWidth = o.value(QStringLiteral("borderWidth")).toDouble(c.borderWidth);
        c.borderColor = o.value(QStringLiteral("borderColor")).toString(c.borderColor);
        c.glowEnabled = o.value(QStringLiteral("glowEnabled")).toBool(c.glowEnabled);
        c.glowColor = o.value(QStringLiteral("glowColor")).toString(c.glowColor);
        c.glowRadius = o.value(QStringLiteral("glowRadius")).toDouble(c.glowRadius);
        c.shadowEnabled = o.value(QStringLiteral("shadowEnabled")).toBool(c.shadowEnabled);
        c.shadowColor = o.value(QStringLiteral("shadowColor")).toString(c.shadowColor);
        c.shadowRadius = o.value(QStringLiteral("shadowRadius")).toDouble(c.shadowRadius);
        return c;
    }
};

struct HudCardTheme {
    QString layout = QStringLiteral("horizontal_chip"); // "horizontal_chip", "vertical_card", "compact_tile"
    QString shape = QStringLiteral("rounded_rect");      // "rounded_rect", "chamfered", "sharp", "pill"
    int cornerRadius = 6;
    int chamferSize = 6;
    QString bgNormal = QStringLiteral("$bgCard");
    QString bgActive = QStringLiteral("$bgCardActive");
    QString borderNormal = QStringLiteral("#3a404a");
    QString borderActive = QStringLiteral("$accentColor");
    double borderWidth = 1.5;
    bool activeGlow = true;
    QString audioIndicatorStyle = QStringLiteral("speaker_icon"); // "speaker_icon", "soundwave_pulse", "minimal_dot"
    QString audioIndicatorColor = QStringLiteral("$audioColor");
    double audioIndicatorScale = 1.0;
    bool audioPulseBadge = true;
    double audioPulseSpeed = 1.0;
    double audioPulseIntensity = 1.0;

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QStringLiteral("layout")] = layout;
        o[QStringLiteral("shape")] = shape;
        o[QStringLiteral("cornerRadius")] = cornerRadius;
        o[QStringLiteral("chamferSize")] = chamferSize;
        o[QStringLiteral("bgNormal")] = bgNormal;
        o[QStringLiteral("bgActive")] = bgActive;
        o[QStringLiteral("borderNormal")] = borderNormal;
        o[QStringLiteral("borderActive")] = borderActive;
        o[QStringLiteral("borderWidth")] = borderWidth;
        o[QStringLiteral("activeGlow")] = activeGlow;
        o[QStringLiteral("audioIndicatorStyle")] = audioIndicatorStyle;
        o[QStringLiteral("audioIndicatorColor")] = audioIndicatorColor;
        o[QStringLiteral("audioIndicatorScale")] = audioIndicatorScale;
        o[QStringLiteral("audioPulseBadge")] = audioPulseBadge;
        o[QStringLiteral("audioPulseSpeed")] = audioPulseSpeed;
        o[QStringLiteral("audioPulseIntensity")] = audioPulseIntensity;
        return o;
    }

    static HudCardTheme fromJson(const QJsonObject &o)
    {
        HudCardTheme c;
        c.layout = o.value(QStringLiteral("layout")).toString(c.layout);
        c.shape = o.value(QStringLiteral("shape")).toString(c.shape);
        c.cornerRadius = o.value(QStringLiteral("cornerRadius")).toInt(c.cornerRadius);
        c.chamferSize = o.value(QStringLiteral("chamferSize")).toInt(c.chamferSize);
        c.bgNormal = o.value(QStringLiteral("bgNormal")).toString(c.bgNormal);
        c.bgActive = o.value(QStringLiteral("bgActive")).toString(c.bgActive);
        c.borderNormal = o.value(QStringLiteral("borderNormal")).toString(c.borderNormal);
        c.borderActive = o.value(QStringLiteral("borderActive")).toString(c.borderActive);
        c.borderWidth = o.value(QStringLiteral("borderWidth")).toDouble(c.borderWidth);
        c.activeGlow = o.value(QStringLiteral("activeGlow")).toBool(c.activeGlow);
        c.audioIndicatorStyle = o.value(QStringLiteral("audioIndicatorStyle")).toString(c.audioIndicatorStyle);
        c.audioIndicatorColor = o.value(QStringLiteral("audioIndicatorColor")).toString(c.audioIndicatorColor);
        c.audioIndicatorScale = o.value(QStringLiteral("audioIndicatorScale")).toDouble(c.audioIndicatorScale);
        c.audioPulseBadge = o.value(QStringLiteral("audioPulseBadge")).toBool(c.audioPulseBadge);
        c.audioPulseSpeed = o.value(QStringLiteral("audioPulseSpeed")).toDouble(c.audioPulseSpeed);
        c.audioPulseIntensity = o.value(QStringLiteral("audioPulseIntensity")).toDouble(c.audioPulseIntensity);
        return c;
    }
};

struct SearchHeaderTheme {
    QString style = QStringLiteral("spotlight_capsule"); // "spotlight_capsule", "minimal_underline", "terminal_box"
    QString promptPrefix = QStringLiteral("❯ ");
    QString cursorStyle = QStringLiteral("line");        // "line", "block_blink", "underline"
    QString activeScopeBg = QStringLiteral("$accentColor");
    QString inactiveScopeBg = QStringLiteral("#2e3440");

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QStringLiteral("style")] = style;
        o[QStringLiteral("promptPrefix")] = promptPrefix;
        o[QStringLiteral("cursorStyle")] = cursorStyle;
        o[QStringLiteral("activeScopeBg")] = activeScopeBg;
        o[QStringLiteral("inactiveScopeBg")] = inactiveScopeBg;
        return o;
    }

    static SearchHeaderTheme fromJson(const QJsonObject &o)
    {
        SearchHeaderTheme s;
        s.style = o.value(QStringLiteral("style")).toString(s.style);
        s.promptPrefix = o.value(QStringLiteral("promptPrefix")).toString(s.promptPrefix);
        s.cursorStyle = o.value(QStringLiteral("cursorStyle")).toString(s.cursorStyle);
        s.activeScopeBg = o.value(QStringLiteral("activeScopeBg")).toString(s.activeScopeBg);
        s.inactiveScopeBg = o.value(QStringLiteral("inactiveScopeBg")).toString(s.inactiveScopeBg);
        return s;
    }
};

struct HintsBarTheme {
    QString keycapStyle = QStringLiteral("retro_keycap"); // "retro_keycap", "pill", "bracket", "flat_chip", "neon_outline"
    QString separator = QStringLiteral(" • ");
    QString keyColor = QStringLiteral("#ffffff");
    QString keyBgColor = QStringLiteral("#34495e");
    QString labelColor = QStringLiteral("#bdc3c7");
    QString bgMode = QStringLiteral("pill");              // "pill", "full_bar", "transparent"

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QStringLiteral("keycapStyle")] = keycapStyle;
        o[QStringLiteral("separator")] = separator;
        o[QStringLiteral("keyColor")] = keyColor;
        o[QStringLiteral("keyBgColor")] = keyBgColor;
        o[QStringLiteral("labelColor")] = labelColor;
        o[QStringLiteral("bgMode")] = bgMode;
        return o;
    }

    static HintsBarTheme fromJson(const QJsonObject &o)
    {
        HintsBarTheme h;
        h.keycapStyle = o.value(QStringLiteral("keycapStyle")).toString(h.keycapStyle);
        h.separator = o.value(QStringLiteral("separator")).toString(h.separator);
        h.keyColor = o.value(QStringLiteral("keyColor")).toString(h.keyColor);
        h.keyBgColor = o.value(QStringLiteral("keyBgColor")).toString(h.keyBgColor);
        h.labelColor = o.value(QStringLiteral("labelColor")).toString(h.labelColor);
        h.bgMode = o.value(QStringLiteral("bgMode")).toString(h.bgMode);
        return h;
    }
};

struct HudNavTheme {
    QString style = QStringLiteral("capsule_tabs");      // "capsule_tabs", "retro_tabs", "minimal", "dock_tiles"
    QString standardIcon = QStringLiteral("💻");
    QString workspaceIcon = QStringLiteral("📁");
    QString allIcon = QStringLiteral("🌐");
    QString stashIcon = QStringLiteral("📦");
    QString newWsIcon = QStringLiteral("+");
    QString arrowLeft = QStringLiteral("◂ ,");
    QString arrowRight = QStringLiteral(". ▸");
    QString standardText = QStringLiteral("Standard");
    QString allText = QStringLiteral("All");
    QString stashText = QStringLiteral("Stash");
    QString newWsText = QStringLiteral("New");
    QString tabShape = QStringLiteral("pill");          // "pill", "rounded_rect", "chamfered", "sharp"
    int tabCornerRadius = 12;
    int tabChamferSize = 6;
    double iconScale = 1.0;

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QStringLiteral("style")] = style;
        o[QStringLiteral("standardIcon")] = standardIcon;
        o[QStringLiteral("workspaceIcon")] = workspaceIcon;
        o[QStringLiteral("allIcon")] = allIcon;
        o[QStringLiteral("stashIcon")] = stashIcon;
        o[QStringLiteral("newWsIcon")] = newWsIcon;
        o[QStringLiteral("arrowLeft")] = arrowLeft;
        o[QStringLiteral("arrowRight")] = arrowRight;
        o[QStringLiteral("standardText")] = standardText;
        o[QStringLiteral("allText")] = allText;
        o[QStringLiteral("stashText")] = stashText;
        o[QStringLiteral("newWsText")] = newWsText;
        o[QStringLiteral("tabShape")] = tabShape;
        o[QStringLiteral("tabCornerRadius")] = tabCornerRadius;
        o[QStringLiteral("tabChamferSize")] = tabChamferSize;
        o[QStringLiteral("iconScale")] = iconScale;
        return o;
    }

    static HudNavTheme fromJson(const QJsonObject &o)
    {
        HudNavTheme n;
        n.style = o.value(QStringLiteral("style")).toString(n.style);
        n.standardIcon = o.value(QStringLiteral("standardIcon")).toString(n.standardIcon);
        n.workspaceIcon = o.value(QStringLiteral("workspaceIcon")).toString(n.workspaceIcon);
        n.allIcon = o.value(QStringLiteral("allIcon")).toString(n.allIcon);
        n.stashIcon = o.value(QStringLiteral("stashIcon")).toString(n.stashIcon);
        n.newWsIcon = o.value(QStringLiteral("newWsIcon")).toString(n.newWsIcon);
        n.arrowLeft = o.value(QStringLiteral("arrowLeft")).toString(n.arrowLeft);
        n.arrowRight = o.value(QStringLiteral("arrowRight")).toString(n.arrowRight);
        n.standardText = o.value(QStringLiteral("standardText")).toString(n.standardText);
        n.allText = o.value(QStringLiteral("allText")).toString(n.allText);
        n.stashText = o.value(QStringLiteral("stashText")).toString(n.stashText);
        n.newWsText = o.value(QStringLiteral("newWsText")).toString(n.newWsText);
        n.tabShape = o.value(QStringLiteral("tabShape")).toString(n.tabShape);
        n.tabCornerRadius = o.value(QStringLiteral("tabCornerRadius")).toInt(n.tabCornerRadius);
        n.tabChamferSize = o.value(QStringLiteral("tabChamferSize")).toInt(n.tabChamferSize);
        n.iconScale = o.value(QStringLiteral("iconScale")).toDouble(n.iconScale);
        return n;
    }
};

struct SoundTheme {
    bool enabled = true;
    double volume = 1.0;
    QString profile = QStringLiteral("crisp_click"); // "crisp_click", "soft_pop", "scifi_blip", "mechanical"
    QString enterSound;
    QString landSound;
    QString workspaceSound;
    QString navSound;
    QString dismissSound;
    QString stashSound;

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QStringLiteral("enabled")] = enabled;
        o[QStringLiteral("volume")] = volume;
        o[QStringLiteral("profile")] = profile;
        if (!enterSound.isEmpty()) o[QStringLiteral("enterSound")] = enterSound;
        if (!landSound.isEmpty()) o[QStringLiteral("landSound")] = landSound;
        if (!workspaceSound.isEmpty()) o[QStringLiteral("workspaceSound")] = workspaceSound;
        if (!navSound.isEmpty()) o[QStringLiteral("navSound")] = navSound;
        if (!dismissSound.isEmpty()) o[QStringLiteral("dismissSound")] = dismissSound;
        if (!stashSound.isEmpty()) o[QStringLiteral("stashSound")] = stashSound;
        return o;
    }

    static SoundTheme fromJson(const QJsonObject &o)
    {
        SoundTheme s;
        s.enabled = o.value(QStringLiteral("enabled")).toBool(s.enabled);
        s.volume = o.value(QStringLiteral("volume")).toDouble(s.volume);
        s.profile = o.value(QStringLiteral("profile")).toString(s.profile);
        s.enterSound = o.value(QStringLiteral("enterSound")).toString(s.enterSound);
        s.landSound = o.value(QStringLiteral("landSound")).toString(s.landSound);
        s.workspaceSound = o.value(QStringLiteral("workspaceSound")).toString(s.workspaceSound);
        s.navSound = o.value(QStringLiteral("navSound")).toString(s.navSound);
        s.dismissSound = o.value(QStringLiteral("dismissSound")).toString(s.dismissSound);
        s.stashSound = o.value(QStringLiteral("stashSound")).toString(s.stashSound);
        return s;
    }
};

struct SpatialKeyboardTheme {
    QString fontFamily;
    QString keycapShape = QStringLiteral("rounded_rect"); // "rounded_rect", "pill", "chamfered", "sharp"
    int keycapCornerRadius = 6;
    QString keycapBg = QStringLiteral("$bgCard");
    QString keycapBorder = QStringLiteral("$borderNormal");
    QString keycapTextColor = QStringLiteral("$textPrimary");
    QString panelShape = QStringLiteral("rounded_rect");
    int panelCornerRadius = 12;
    QString panelBg = QStringLiteral("$bgSurface");
    QString panelBorder = QStringLiteral("$borderColor");
    bool laserGlow = true;
    double laserGlowIntensity = 1.0;
    QString laserDashStyle = QStringLiteral("solid"); // "solid", "dashed", "dotted"
    QString targetBadgeShape = QStringLiteral("circle"); // "circle", "rounded_rect", "pill", "chamfered", "ring"
    int targetBadgeCornerRadius = 8;
    QString targetBadgeBg = QStringLiteral("$bgCard");
    QString targetBadgeBorder = QStringLiteral("$borderColor");
    double targetBadgeBorderWidth = 2.0;
    bool targetLabelVisible = true;
    int targetLabelFontSize = 9;
    QString targetLabelBg = QStringLiteral("#0c0f16eb");
    QString targetLabelTextColor = QStringLiteral("$textPrimary");
    double stackScaleReduction = 0.35;   // 0.0 .. 0.70 (degree of badge size reduction for stacked/occluded windows)
    double stackOpacityReduction = 0.40; // 0.0 .. 0.70 (degree of opacity reduction for lower stacked levels)
    double stackBadgeGap = 8.0;

    QJsonObject toJson() const
    {
        QJsonObject o;
        if (!fontFamily.isEmpty()) o[QStringLiteral("fontFamily")] = fontFamily;
        o[QStringLiteral("keycapShape")] = keycapShape;
        o[QStringLiteral("keycapCornerRadius")] = keycapCornerRadius;
        o[QStringLiteral("keycapBg")] = keycapBg;
        o[QStringLiteral("keycapBorder")] = keycapBorder;
        o[QStringLiteral("keycapTextColor")] = keycapTextColor;
        o[QStringLiteral("panelShape")] = panelShape;
        o[QStringLiteral("panelCornerRadius")] = panelCornerRadius;
        o[QStringLiteral("panelBg")] = panelBg;
        o[QStringLiteral("panelBorder")] = panelBorder;
        o[QStringLiteral("laserGlow")] = laserGlow;
        o[QStringLiteral("laserGlowIntensity")] = laserGlowIntensity;
        o[QStringLiteral("laserDashStyle")] = laserDashStyle;
        o[QStringLiteral("targetBadgeShape")] = targetBadgeShape;
        o[QStringLiteral("targetBadgeCornerRadius")] = targetBadgeCornerRadius;
        o[QStringLiteral("targetBadgeBg")] = targetBadgeBg;
        o[QStringLiteral("targetBadgeBorder")] = targetBadgeBorder;
        o[QStringLiteral("targetBadgeBorderWidth")] = targetBadgeBorderWidth;
        o[QStringLiteral("targetLabelVisible")] = targetLabelVisible;
        o[QStringLiteral("targetLabelFontSize")] = targetLabelFontSize;
        o[QStringLiteral("targetLabelBg")] = targetLabelBg;
        o[QStringLiteral("targetLabelTextColor")] = targetLabelTextColor;
        o[QStringLiteral("stackScaleReduction")] = stackScaleReduction;
        o[QStringLiteral("stackOpacityReduction")] = stackOpacityReduction;
        o[QStringLiteral("stackBadgeGap")] = stackBadgeGap;
        return o;
    }

    static SpatialKeyboardTheme fromJson(const QJsonObject &o)
    {
        SpatialKeyboardTheme k;
        k.fontFamily = o.value(QStringLiteral("fontFamily")).toString(k.fontFamily);
        k.keycapShape = o.value(QStringLiteral("keycapShape")).toString(k.keycapShape);
        k.keycapCornerRadius = o.value(QStringLiteral("keycapCornerRadius")).toInt(k.keycapCornerRadius);
        k.keycapBg = o.value(QStringLiteral("keycapBg")).toString(k.keycapBg);
        k.keycapBorder = o.value(QStringLiteral("keycapBorder")).toString(k.keycapBorder);
        k.keycapTextColor = o.value(QStringLiteral("keycapTextColor")).toString(k.keycapTextColor);
        k.panelShape = o.value(QStringLiteral("panelShape")).toString(k.panelShape);
        k.panelCornerRadius = o.value(QStringLiteral("panelCornerRadius")).toInt(k.panelCornerRadius);
        k.panelBg = o.value(QStringLiteral("panelBg")).toString(k.panelBg);
        k.panelBorder = o.value(QStringLiteral("panelBorder")).toString(k.panelBorder);
        k.laserGlow = o.value(QStringLiteral("laserGlow")).toBool(k.laserGlow);
        k.laserGlowIntensity = o.value(QStringLiteral("laserGlowIntensity")).toDouble(k.laserGlowIntensity);
        k.laserDashStyle = o.value(QStringLiteral("laserDashStyle")).toString(k.laserDashStyle);
        k.targetBadgeShape = o.value(QStringLiteral("targetBadgeShape")).toString(k.targetBadgeShape);
        k.targetBadgeCornerRadius = o.value(QStringLiteral("targetBadgeCornerRadius")).toInt(k.targetBadgeCornerRadius);
        k.targetBadgeBg = o.value(QStringLiteral("targetBadgeBg")).toString(k.targetBadgeBg);
        k.targetBadgeBorder = o.value(QStringLiteral("targetBadgeBorder")).toString(k.targetBadgeBorder);
        k.targetBadgeBorderWidth = o.value(QStringLiteral("targetBadgeBorderWidth")).toDouble(k.targetBadgeBorderWidth);
        k.targetLabelVisible = o.value(QStringLiteral("targetLabelVisible")).toBool(k.targetLabelVisible);
        k.targetLabelFontSize = o.value(QStringLiteral("targetLabelFontSize")).toInt(k.targetLabelFontSize);
        k.targetLabelBg = o.value(QStringLiteral("targetLabelBg")).toString(k.targetLabelBg);
        k.targetLabelTextColor = o.value(QStringLiteral("targetLabelTextColor")).toString(k.targetLabelTextColor);
        k.stackScaleReduction = o.value(QStringLiteral("stackScaleReduction")).toDouble(k.stackScaleReduction);
        k.stackOpacityReduction = o.value(QStringLiteral("stackOpacityReduction")).toDouble(k.stackOpacityReduction);
        k.stackBadgeGap = o.value(QStringLiteral("stackBadgeGap")).toDouble(k.stackBadgeGap);
        return k;
    }
};

// ============================================================================
// 3. Complete LeapK Theme Structure
// ============================================================================

struct Theme {
    QString id = QStringLiteral("default-breeze");
    QString name = QStringLiteral("Breeze Dark (Default)");
    QString description = QStringLiteral("Clean and balanced KDE Plasma 6 Breeze Dark styling.");
    QString author = QStringLiteral("LeapK Team");
    QString version = QStringLiteral("1.0");
    bool isBuiltIn = true;
    QString filePath;

    QList<ThemeParameter> parameters;
    QMap<QString, QString> tokens;

    WindowBadgeTheme windowBadge;
    HudContainerTheme hudContainer;
    HudCardTheme hudCard;
    SearchHeaderTheme searchHeader;
    HintsBarTheme hintsBar;
    HudNavTheme hudNav;
    SoundTheme sound;
    SpatialKeyboardTheme spatialKeyboard;

    // Helper: resolves a value that may reference a parameter ($id) or token
    QString resolveVar(const QString &val) const
    {
        if (!val.startsWith(QLatin1Char('$'))) {
            return val;
        }
        const QString varId = val.mid(1);

        // 1. Look in parameters
        for (const auto &p : parameters) {
            if (p.id == varId) {
                if (p.type == QStringLiteral("color")) {
                    return p.colorCurrent;
                }
                if (p.type == QStringLiteral("enum")) {
                    return p.enumCurrent.isEmpty() ? p.enumDefault : p.enumCurrent;
                }
                if (p.type == QStringLiteral("bool")) {
                    return (p.currentValue > 0.5) ? QStringLiteral("true") : QStringLiteral("false");
                }
                return QString::number(p.currentValue);
            }
        }

        // 2. Look in tokens
        if (tokens.contains(varId)) {
            return tokens.value(varId);
        }

        return val;
    }

    QColor resolveColor(const QString &spec, const QColor &fallback = QColor()) const
    {
        const QString resolved = resolveVar(spec);
        const QColor c(resolved);
        return c.isValid() ? c : fallback;
    }

    int resolveInt(const QString &spec, int fallback = 0) const
    {
        const QString resolved = resolveVar(spec);
        bool ok = false;
        const int val = resolved.toInt(&ok);
        return ok ? val : fallback;
    }

    double resolveDouble(const QString &spec, double fallback = 0.0) const
    {
        const QString resolved = resolveVar(spec);
        bool ok = false;
        const double val = resolved.toDouble(&ok);
        return ok ? val : fallback;
    }

    bool resolveBool(const QString &spec, bool fallback = false) const
    {
        const QString resolved = resolveVar(spec);
        if (resolved.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || resolved == QStringLiteral("1")) {
            return true;
        }
        if (resolved.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0 || resolved == QStringLiteral("0")) {
            return false;
        }
        return fallback;
    }

    QColor primaryAccentColor() const
    {
        QColor c = resolveColor(tokens.value(QStringLiteral("borderColor")));
        if (c.isValid()) return c;
        c = resolveColor(QStringLiteral("$accentColor"));
        if (c.isValid()) return c;
        c = resolveColor(QStringLiteral("$phosphorColor"));
        if (c.isValid()) return c;
        c = resolveColor(QStringLiteral("$frostColor"));
        if (c.isValid()) return c;
        c = resolveColor(QStringLiteral("$neonCyan"));
        if (c.isValid()) return c;
        return QColor(QStringLiteral("#3daee9"));
    }

    QColor secondaryAccentColor() const
    {
        QColor c = resolveColor(QStringLiteral("$neonMagenta"));
        if (c.isValid()) return c;
        c = resolveColor(tokens.value(QStringLiteral("textSecondary")));
        if (c.isValid()) return c;
        return primaryAccentColor();
    }

    QColor surfaceBackgroundColor() const
    {
        QColor c = resolveColor(tokens.value(QStringLiteral("bgSurface")));
        if (!c.isValid()) {
            c = QColor(18, 21, 26);
        }
        const int alpha = qBound(50, int(hudContainer.bgOpacity * 255.0), 255);
        c.setAlpha(alpha);
        return c;
    }

    QColor cardBackgroundColor(bool active = false) const
    {
        const QString spec = active ? hudCard.bgActive : hudCard.bgNormal;
        QColor c = resolveColor(spec);
        if (!c.isValid()) {
            c = active ? QColor(40, 46, 54) : QColor(30, 34, 39);
        }
        return c;
    }

    QColor textPrimaryColor() const
    {
        QColor c = resolveColor(tokens.value(QStringLiteral("textPrimary")));
        return c.isValid() ? c : QColor(255, 255, 255);
    }

    QColor textSecondaryColor() const
    {
        QColor c = resolveColor(tokens.value(QStringLiteral("textSecondary")));
        return c.isValid() ? c : QColor(180, 185, 190);
    }

    QColor frameBorderColor() const
    {
        QColor c = resolveColor(hudContainer.borderColor);
        if (!c.isValid()) {
            c = primaryAccentColor();
        }
        return c;
    }

    QColor cardBorderColor(bool active = false) const
    {
        const QString spec = active ? hudCard.borderActive : hudCard.borderNormal;
        QColor c = resolveColor(spec);
        if (!c.isValid()) {
            c = active ? primaryAccentColor() : QColor(60, 66, 75);
        }
        return c;
    }

    int cornerRadius() const
    {
        return qBound(0, hudContainer.cornerRadius, 32);
    }

    int cardCornerRadius() const
    {
        return qBound(0, hudCard.cornerRadius, 24);
    }

    void applyParameterValues(const QJsonObject &userValues)
    {
        for (auto &p : parameters) {
            if (userValues.contains(p.id)) {
                const QJsonValue val = userValues.value(p.id);
                if (p.type == QStringLiteral("color")) {
                    p.colorCurrent = val.toString(p.colorDefault);
                } else if (p.type == QStringLiteral("enum")) {
                    p.enumCurrent = val.toString(p.enumDefault);
                } else if (p.type == QStringLiteral("bool")) {
                    p.currentValue = val.toBool(p.defaultValue > 0.5) ? 1.0 : 0.0;
                } else {
                    p.currentValue = val.toDouble(p.defaultValue);
                }
            }
        }
    }

    QJsonObject exportParameterValues() const
    {
        QJsonObject root;
        for (const auto &p : parameters) {
            if (p.type == QStringLiteral("color")) {
                root[p.id] = p.colorCurrent;
            } else if (p.type == QStringLiteral("enum")) {
                root[p.id] = p.enumCurrent;
            } else if (p.type == QStringLiteral("bool")) {
                root[p.id] = (p.currentValue > 0.5);
            } else {
                root[p.id] = p.currentValue;
            }
        }
        return root;
    }

    QJsonObject toJson() const
    {
        QJsonObject root;
        root[QStringLiteral("format")] = QStringLiteral("leapk-theme");
        root[QStringLiteral("version")] = version;
        root[QStringLiteral("id")] = id;
        root[QStringLiteral("name")] = name;
        root[QStringLiteral("description")] = description;
        root[QStringLiteral("author")] = author;
        root[QStringLiteral("isBuiltIn")] = isBuiltIn;

        QJsonArray paramArray;
        for (const auto &p : parameters) {
            paramArray.append(p.toJson());
        }
        root[QStringLiteral("parameters")] = paramArray;

        QJsonObject tokenObj;
        for (auto it = tokens.begin(); it != tokens.end(); ++it) {
            tokenObj[it.key()] = it.value();
        }
        root[QStringLiteral("tokens")] = tokenObj;

        QJsonObject comps;
        comps[QStringLiteral("windowBadge")] = windowBadge.toJson();
        comps[QStringLiteral("hudContainer")] = hudContainer.toJson();
        comps[QStringLiteral("hudCard")] = hudCard.toJson();
        comps[QStringLiteral("searchHeader")] = searchHeader.toJson();
        comps[QStringLiteral("hintsBar")] = hintsBar.toJson();
        comps[QStringLiteral("hudNav")] = hudNav.toJson();
        comps[QStringLiteral("sound")] = sound.toJson();
        comps[QStringLiteral("spatialKeyboard")] = spatialKeyboard.toJson();
        root[QStringLiteral("components")] = comps;

        return root;
    }

    static Theme fromJson(const QJsonObject &root)
    {
        Theme t;
        t.id = root.value(QStringLiteral("id")).toString(t.id);
        t.name = root.value(QStringLiteral("name")).toString(t.id);
        t.description = root.value(QStringLiteral("description")).toString();
        t.author = root.value(QStringLiteral("author")).toString();
        t.version = root.value(QStringLiteral("version")).toString(QStringLiteral("1.0"));
        t.isBuiltIn = root.value(QStringLiteral("isBuiltIn")).toBool(false);

        const QJsonArray paramArray = root.value(QStringLiteral("parameters")).toArray();
        for (const auto &v : paramArray) {
            if (v.isObject()) {
                t.parameters.append(ThemeParameter::fromJson(v.toObject()));
            }
        }

        const QJsonObject tokenObj = root.value(QStringLiteral("tokens")).toObject();
        for (auto it = tokenObj.begin(); it != tokenObj.end(); ++it) {
            t.tokens[it.key()] = it.value().toString();
        }

        const QJsonObject comps = root.value(QStringLiteral("components")).toObject();
        if (comps.contains(QStringLiteral("windowBadge"))) {
            t.windowBadge = WindowBadgeTheme::fromJson(comps.value(QStringLiteral("windowBadge")).toObject());
        }
        if (comps.contains(QStringLiteral("hudContainer"))) {
            t.hudContainer = HudContainerTheme::fromJson(comps.value(QStringLiteral("hudContainer")).toObject());
        }
        if (comps.contains(QStringLiteral("hudCard"))) {
            t.hudCard = HudCardTheme::fromJson(comps.value(QStringLiteral("hudCard")).toObject());
        }
        if (comps.contains(QStringLiteral("searchHeader"))) {
            t.searchHeader = SearchHeaderTheme::fromJson(comps.value(QStringLiteral("searchHeader")).toObject());
        }
        if (comps.contains(QStringLiteral("hintsBar"))) {
            t.hintsBar = HintsBarTheme::fromJson(comps.value(QStringLiteral("hintsBar")).toObject());
        }
        if (comps.contains(QStringLiteral("hudNav"))) {
            t.hudNav = HudNavTheme::fromJson(comps.value(QStringLiteral("hudNav")).toObject());
        }
        if (comps.contains(QStringLiteral("sound"))) {
            t.sound = SoundTheme::fromJson(comps.value(QStringLiteral("sound")).toObject());
        }
        if (comps.contains(QStringLiteral("spatialKeyboard"))) {
            t.spatialKeyboard = SpatialKeyboardTheme::fromJson(comps.value(QStringLiteral("spatialKeyboard")).toObject());
        }

        return t;
    }
};

// ============================================================================
// 4. Built-in Themes Library
// ============================================================================

inline QList<Theme> builtInThemes()
{
    QList<Theme> list;

    // 1. Breeze Dark (Default)
    {
        Theme t;
        t.id = QStringLiteral("default-breeze");
        t.name = QStringLiteral("Breeze Dark (Default)");
        t.description = QStringLiteral("Balanced, modern KDE Plasma 6 Breeze aesthetic with smooth rounded edges.");
        t.author = QStringLiteral("LeapK Core Team");
        t.isBuiltIn = true;

        ThemeParameter pAccent;
        pAccent.id = QStringLiteral("accentColor");
        pAccent.name = QStringLiteral("Accent Color");
        pAccent.type = QStringLiteral("color");
        pAccent.colorDefault = QStringLiteral("#3daee9");
        pAccent.colorCurrent = pAccent.colorDefault;
        t.parameters.append(pAccent);

        ThemeParameter pOpacity;
        pOpacity.id = QStringLiteral("hudOpacity");
        pOpacity.name = QStringLiteral("HUD Background Opacity");
        pOpacity.type = QStringLiteral("float");
        pOpacity.minValue = 0.3;
        pOpacity.maxValue = 1.0;
        pOpacity.step = 0.05;
        pOpacity.defaultValue = 0.92;
        pOpacity.currentValue = 0.92;
        pOpacity.suffix = QStringLiteral("%");
        t.parameters.append(pOpacity);

        ThemeParameter pRadius;
        pRadius.id = QStringLiteral("cornerRadius");
        pRadius.name = QStringLiteral("Corner Radius");
        pRadius.type = QStringLiteral("int");
        pRadius.minValue = 2.0;
        pRadius.maxValue = 24.0;
        pRadius.step = 1.0;
        pRadius.defaultValue = 6.0;
        pRadius.currentValue = 6.0;
        pRadius.suffix = QStringLiteral(" px");
        t.parameters.append(pRadius);

        ThemeParameter pAudioSpeed;
        pAudioSpeed.id = QStringLiteral("audioPulseSpeed");
        pAudioSpeed.name = QStringLiteral("Audio Pulse Speed");
        pAudioSpeed.type = QStringLiteral("float");
        pAudioSpeed.minValue = 0.2;
        pAudioSpeed.maxValue = 3.0;
        pAudioSpeed.step = 0.1;
        pAudioSpeed.defaultValue = 1.0;
        pAudioSpeed.currentValue = 1.0;
        pAudioSpeed.suffix = QStringLiteral("x");
        t.parameters.append(pAudioSpeed);

        ThemeParameter pAudioIntensity;
        pAudioIntensity.id = QStringLiteral("audioPulseIntensity");
        pAudioIntensity.name = QStringLiteral("Audio Pulse Intensity");
        pAudioIntensity.type = QStringLiteral("float");
        pAudioIntensity.minValue = 0.0;
        pAudioIntensity.maxValue = 2.5;
        pAudioIntensity.step = 0.1;
        pAudioIntensity.defaultValue = 1.0;
        pAudioIntensity.currentValue = 1.0;
        pAudioIntensity.suffix = QStringLiteral("x");
        t.parameters.append(pAudioIntensity);

        ThemeParameter pSoundProf;
        pSoundProf.id = QStringLiteral("soundProfile");
        pSoundProf.name = QStringLiteral("Sound Feedback Profile");
        pSoundProf.type = QStringLiteral("enum");
        pSoundProf.enumOptions = {QStringLiteral("crisp_click"), QStringLiteral("soft_pop"), QStringLiteral("scifi_blip"), QStringLiteral("mechanical")};
        pSoundProf.enumDefault = QStringLiteral("crisp_click");
        pSoundProf.enumCurrent = pSoundProf.enumDefault;
        t.parameters.append(pSoundProf);

        ThemeParameter pSoundVol;
        pSoundVol.id = QStringLiteral("soundVolume");
        pSoundVol.name = QStringLiteral("Theme Sound Volume");
        pSoundVol.type = QStringLiteral("float");
        pSoundVol.minValue = 0.0;
        pSoundVol.maxValue = 1.0;
        pSoundVol.step = 0.05;
        pSoundVol.defaultValue = 1.0;
        pSoundVol.currentValue = 1.0;
        pSoundVol.suffix = QStringLiteral("%");
        t.parameters.append(pSoundVol);

        t.sound.profile = QStringLiteral("crisp_click");

        t.tokens[QStringLiteral("bgSurface")] = QStringLiteral("#16191d");
        t.tokens[QStringLiteral("bgCard")] = QStringLiteral("#1e2227");
        t.tokens[QStringLiteral("bgCardActive")] = QStringLiteral("#282e36");
        t.tokens[QStringLiteral("textPrimary")] = QStringLiteral("#eff0f1");
        t.tokens[QStringLiteral("textSecondary")] = QStringLiteral("#9ba1a6");
        t.tokens[QStringLiteral("borderColor")] = QStringLiteral("$accentColor");
        t.tokens[QStringLiteral("borderNormal")] = QStringLiteral("#363c46");
        t.tokens[QStringLiteral("audioColor")] = QStringLiteral("#f1c40f");

        t.hudContainer.shape = QStringLiteral("rounded_rect");
        t.hudContainer.cornerRadius = 8;
        t.hudContainer.borderWidth = 1.2;
        t.hudContainer.borderColor = QStringLiteral("$borderColor");
        t.hudContainer.bgOpacity = 0.92;

        t.hudCard.shape = QStringLiteral("rounded_rect");
        t.hudCard.cornerRadius = 6;
        t.hudCard.bgNormal = QStringLiteral("$bgCard");
        t.hudCard.bgActive = QStringLiteral("$bgCardActive");
        t.hudCard.borderNormal = QStringLiteral("$borderNormal");
        t.hudCard.borderActive = QStringLiteral("$accentColor");
        t.hudCard.audioIndicatorColor = QStringLiteral("$audioColor");

        t.windowBadge.shape = QStringLiteral("rounded_rect");
        t.windowBadge.cornerRadius = 6;
        t.windowBadge.bgNormal = QStringLiteral("$bgCard");
        t.windowBadge.borderColor = QStringLiteral("$borderNormal");

        t.searchHeader.promptPrefix = QStringLiteral("Search: ");
        t.searchHeader.cursorStyle = QStringLiteral("bar");
        t.hintsBar.keycapStyle = QStringLiteral("retro_keycap");
        t.hintsBar.separator = QStringLiteral(" • ");
        t.hintsBar.keyColor = QStringLiteral("$accentColor");
        t.hintsBar.keyBgColor = QStringLiteral("#2e3440");
        t.hintsBar.labelColor = QStringLiteral("$textSecondary");

        t.hudNav.standardIcon = QStringLiteral("💻");
        t.hudNav.workspaceIcon = QStringLiteral("📁");
        t.hudNav.allIcon = QStringLiteral("🌐");
        t.hudNav.stashIcon = QStringLiteral("📦");
        t.hudNav.newWsIcon = QStringLiteral("+");
        t.hudNav.arrowLeft = QStringLiteral("◂ ,");
        t.hudNav.arrowRight = QStringLiteral(". ▸");
        t.hudNav.tabShape = QStringLiteral("pill");
        t.hudNav.tabCornerRadius = 12;

        t.spatialKeyboard.keycapShape = QStringLiteral("rounded_rect");
        t.spatialKeyboard.keycapCornerRadius = 6;
        t.spatialKeyboard.targetBadgeShape = QStringLiteral("circle");
        t.spatialKeyboard.laserGlow = true;
        t.spatialKeyboard.laserGlowIntensity = 1.0;
        t.spatialKeyboard.stackScaleReduction = 0.35;
        t.spatialKeyboard.stackOpacityReduction = 0.40;

        list.append(t);
    }

    // 2. Cyberpunk Neon
    {
        Theme t;
        t.id = QStringLiteral("cyberpunk-neon");
        t.name = QStringLiteral("Cyberpunk Neon");
        t.description = QStringLiteral("Futuristic sci-fi HUD with chamfered 45° cut corners, glowing cyan & magenta neon accents, and digital prompt.");
        t.author = QStringLiteral("LeapK Core Team");
        t.isBuiltIn = true;

        ThemeParameter pCyan;
        pCyan.id = QStringLiteral("neonCyan");
        pCyan.name = QStringLiteral("Primary Neon Accent");
        pCyan.type = QStringLiteral("color");
        pCyan.colorDefault = QStringLiteral("#00f0ff");
        pCyan.colorCurrent = pCyan.colorDefault;
        t.parameters.append(pCyan);

        ThemeParameter pPink;
        pPink.id = QStringLiteral("neonPink");
        pPink.name = QStringLiteral("Secondary Neon Accent");
        pPink.type = QStringLiteral("color");
        pPink.colorDefault = QStringLiteral("#ff0055");
        pPink.colorCurrent = pPink.colorDefault;
        t.parameters.append(pPink);

        ThemeParameter pGlow;
        pGlow.id = QStringLiteral("glowRadius");
        pGlow.name = QStringLiteral("Neon Glow Radius");
        pGlow.type = QStringLiteral("float");
        pGlow.minValue = 4.0;
        pGlow.maxValue = 24.0;
        pGlow.step = 2.0;
        pGlow.defaultValue = 12.0;
        pGlow.currentValue = 12.0;
        pGlow.suffix = QStringLiteral(" px");
        t.parameters.append(pGlow);

        ThemeParameter pCut;
        pCut.id = QStringLiteral("chamferSize");
        pCut.name = QStringLiteral("Chamfer Cut Corner Size");
        pCut.type = QStringLiteral("int");
        pCut.minValue = 4.0;
        pCut.maxValue = 16.0;
        pCut.step = 1.0;
        pCut.defaultValue = 8.0;
        pCut.currentValue = 8.0;
        pCut.suffix = QStringLiteral(" px");
        t.parameters.append(pCut);

        ThemeParameter pAudioSpeed;
        pAudioSpeed.id = QStringLiteral("audioPulseSpeed");
        pAudioSpeed.name = QStringLiteral("Audio Pulse Speed");
        pAudioSpeed.type = QStringLiteral("float");
        pAudioSpeed.minValue = 0.2;
        pAudioSpeed.maxValue = 3.0;
        pAudioSpeed.step = 0.1;
        pAudioSpeed.defaultValue = 1.0;
        pAudioSpeed.currentValue = 1.0;
        pAudioSpeed.suffix = QStringLiteral("x");
        t.parameters.append(pAudioSpeed);

        ThemeParameter pAudioIntensity;
        pAudioIntensity.id = QStringLiteral("audioPulseIntensity");
        pAudioIntensity.name = QStringLiteral("Audio Pulse Intensity");
        pAudioIntensity.type = QStringLiteral("float");
        pAudioIntensity.minValue = 0.0;
        pAudioIntensity.maxValue = 2.5;
        pAudioIntensity.step = 0.1;
        pAudioIntensity.defaultValue = 1.0;
        pAudioIntensity.currentValue = 1.0;
        pAudioIntensity.suffix = QStringLiteral("x");
        t.parameters.append(pAudioIntensity);

        ThemeParameter pSoundProf;
        pSoundProf.id = QStringLiteral("soundProfile");
        pSoundProf.name = QStringLiteral("Sound Feedback Profile");
        pSoundProf.type = QStringLiteral("enum");
        pSoundProf.enumOptions = {QStringLiteral("crisp_click"), QStringLiteral("soft_pop"), QStringLiteral("scifi_blip"), QStringLiteral("mechanical")};
        pSoundProf.enumDefault = QStringLiteral("scifi_blip");
        pSoundProf.enumCurrent = pSoundProf.enumDefault;
        t.parameters.append(pSoundProf);

        ThemeParameter pSoundVol;
        pSoundVol.id = QStringLiteral("soundVolume");
        pSoundVol.name = QStringLiteral("Theme Sound Volume");
        pSoundVol.type = QStringLiteral("float");
        pSoundVol.minValue = 0.0;
        pSoundVol.maxValue = 1.0;
        pSoundVol.step = 0.05;
        pSoundVol.defaultValue = 1.0;
        pSoundVol.currentValue = 1.0;
        pSoundVol.suffix = QStringLiteral("%");
        t.parameters.append(pSoundVol);

        t.sound.profile = QStringLiteral("scifi_blip");

        t.tokens[QStringLiteral("bgSurface")] = QStringLiteral("#0a0618");
        t.tokens[QStringLiteral("bgCard")] = QStringLiteral("#160d33");
        t.tokens[QStringLiteral("bgCardActive")] = QStringLiteral("#261456");
        t.tokens[QStringLiteral("textPrimary")] = QStringLiteral("#00f0ff");
        t.tokens[QStringLiteral("textSecondary")] = QStringLiteral("#ff007f");
        t.tokens[QStringLiteral("borderColor")] = QStringLiteral("$neonCyan");
        t.tokens[QStringLiteral("borderNormal")] = QStringLiteral("#ff007f");
        t.tokens[QStringLiteral("audioColor")] = QStringLiteral("#ffff00");

        t.hudContainer.shape = QStringLiteral("chamfered");
        t.hudContainer.chamferSize = 8;
        t.hudContainer.borderWidth = 2.0;
        t.hudContainer.borderColor = QStringLiteral("$neonCyan");
        t.hudContainer.glowEnabled = true;
        t.hudContainer.glowColor = QStringLiteral("$neonCyan");
        t.hudContainer.glowRadius = 12.0;
        t.hudContainer.bgOpacity = 0.95;

        t.hudCard.shape = QStringLiteral("chamfered");
        t.hudCard.chamferSize = 6;
        t.hudCard.bgNormal = QStringLiteral("$bgCard");
        t.hudCard.bgActive = QStringLiteral("$bgCardActive");
        t.hudCard.borderNormal = QStringLiteral("$borderNormal");
        t.hudCard.borderActive = QStringLiteral("$neonCyan");
        t.hudCard.activeGlow = true;
        t.hudCard.audioIndicatorStyle = QStringLiteral("soundwave_pulse");
        t.hudCard.audioIndicatorColor = QStringLiteral("$neonPink");
        t.hudCard.audioIndicatorScale = 1.25;

        t.windowBadge.shape = QStringLiteral("chamfered");
        t.windowBadge.chamferSize = 5;
        t.windowBadge.bgNormal = QStringLiteral("$bgCard");
        t.windowBadge.borderColor = QStringLiteral("$neonCyan");
        t.windowBadge.audioIndicatorStyle = QStringLiteral("soundwave_pulse");
        t.windowBadge.audioIndicatorColor = QStringLiteral("$neonPink");
        t.windowBadge.audioIndicatorScale = 1.25;

        t.searchHeader.promptPrefix = QStringLiteral("❯ ");
        t.searchHeader.cursorStyle = QStringLiteral("block_blink");
        t.hintsBar.keycapStyle = QStringLiteral("neon_outline");
        t.hintsBar.separator = QStringLiteral(" // ");
        t.hintsBar.keyColor = QStringLiteral("$neonCyan");
        t.hintsBar.keyBgColor = QStringLiteral("#0e182e");
        t.hintsBar.labelColor = QStringLiteral("#8ab4f8");

        t.hudNav.standardIcon = QStringLiteral("⚡");
        t.hudNav.workspaceIcon = QStringLiteral("◈");
        t.hudNav.allIcon = QStringLiteral("⬡");
        t.hudNav.stashIcon = QStringLiteral("💾");
        t.hudNav.newWsIcon = QStringLiteral("＋");
        t.hudNav.arrowLeft = QStringLiteral("« ,");
        t.hudNav.arrowRight = QStringLiteral(". »");
        t.hudNav.tabShape = QStringLiteral("chamfered");
        t.hudNav.tabChamferSize = 5;

        t.spatialKeyboard.keycapShape = QStringLiteral("chamfered");
        t.spatialKeyboard.targetBadgeShape = QStringLiteral("ring");
        t.spatialKeyboard.laserGlow = true;
        t.spatialKeyboard.laserGlowIntensity = 1.7;
        t.spatialKeyboard.laserDashStyle = QStringLiteral("solid");
        t.spatialKeyboard.stackScaleReduction = 0.30;
        t.spatialKeyboard.stackOpacityReduction = 0.45;

        list.append(t);
    }

    // 3. Matrix Terminal
    {
        Theme t;
        t.id = QStringLiteral("retro-terminal");
        t.name = QStringLiteral("Retro Matrix Terminal");
        t.description = QStringLiteral("Sharp rectangular phosphor CRT terminal aesthetic with bracketed keycaps [A] and wireframe cards.");
        t.author = QStringLiteral("LeapK Core Team");
        t.isBuiltIn = true;

        ThemeParameter pPhosphor;
        pPhosphor.id = QStringLiteral("phosphorColor");
        pPhosphor.name = QStringLiteral("CRT Phosphor Color");
        pPhosphor.type = QStringLiteral("color");
        pPhosphor.colorDefault = QStringLiteral("#00ff66");
        pPhosphor.colorCurrent = pPhosphor.colorDefault;
        t.parameters.append(pPhosphor);

        ThemeParameter pWireframe;
        pWireframe.id = QStringLiteral("wireframeBg");
        pWireframe.name = QStringLiteral("Transparent Wireframe Background");
        pWireframe.type = QStringLiteral("bool");
        pWireframe.defaultValue = 1.0;
        pWireframe.currentValue = 1.0;
        t.parameters.append(pWireframe);

        ThemeParameter pAudioSpeed;
        pAudioSpeed.id = QStringLiteral("audioPulseSpeed");
        pAudioSpeed.name = QStringLiteral("Audio Pulse Speed");
        pAudioSpeed.type = QStringLiteral("float");
        pAudioSpeed.minValue = 0.2;
        pAudioSpeed.maxValue = 3.0;
        pAudioSpeed.step = 0.1;
        pAudioSpeed.defaultValue = 1.0;
        pAudioSpeed.currentValue = 1.0;
        pAudioSpeed.suffix = QStringLiteral("x");
        t.parameters.append(pAudioSpeed);

        ThemeParameter pAudioIntensity;
        pAudioIntensity.id = QStringLiteral("audioPulseIntensity");
        pAudioIntensity.name = QStringLiteral("Audio Pulse Intensity");
        pAudioIntensity.type = QStringLiteral("float");
        pAudioIntensity.minValue = 0.0;
        pAudioIntensity.maxValue = 2.5;
        pAudioIntensity.step = 0.1;
        pAudioIntensity.defaultValue = 1.0;
        pAudioIntensity.currentValue = 1.0;
        pAudioIntensity.suffix = QStringLiteral("x");
        t.parameters.append(pAudioIntensity);

        ThemeParameter pSoundProf;
        pSoundProf.id = QStringLiteral("soundProfile");
        pSoundProf.name = QStringLiteral("Sound Feedback Profile");
        pSoundProf.type = QStringLiteral("enum");
        pSoundProf.enumOptions = {QStringLiteral("crisp_click"), QStringLiteral("soft_pop"), QStringLiteral("scifi_blip"), QStringLiteral("mechanical")};
        pSoundProf.enumDefault = QStringLiteral("mechanical");
        pSoundProf.enumCurrent = pSoundProf.enumDefault;
        t.parameters.append(pSoundProf);

        ThemeParameter pSoundVol;
        pSoundVol.id = QStringLiteral("soundVolume");
        pSoundVol.name = QStringLiteral("Theme Sound Volume");
        pSoundVol.type = QStringLiteral("float");
        pSoundVol.minValue = 0.0;
        pSoundVol.maxValue = 1.0;
        pSoundVol.step = 0.05;
        pSoundVol.defaultValue = 1.0;
        pSoundVol.currentValue = 1.0;
        pSoundVol.suffix = QStringLiteral("%");
        t.parameters.append(pSoundVol);

        t.sound.profile = QStringLiteral("mechanical");

        t.tokens[QStringLiteral("bgSurface")] = QStringLiteral("#031006");
        t.tokens[QStringLiteral("bgCard")] = QStringLiteral("#08200d");
        t.tokens[QStringLiteral("bgCardActive")] = QStringLiteral("#103d19");
        t.tokens[QStringLiteral("textPrimary")] = QStringLiteral("#33ff77");
        t.tokens[QStringLiteral("textSecondary")] = QStringLiteral("#1ea843");
        t.tokens[QStringLiteral("borderColor")] = QStringLiteral("$phosphorColor");
        t.tokens[QStringLiteral("borderNormal")] = QStringLiteral("#0d5520");
        t.tokens[QStringLiteral("audioColor")] = QStringLiteral("#33ff77");

        t.hudContainer.shape = QStringLiteral("sharp");
        t.hudContainer.cornerRadius = 0;
        t.hudContainer.borderWidth = 1.5;
        t.hudContainer.borderColor = QStringLiteral("$phosphorColor");
        t.hudContainer.bgOpacity = 0.96;

        t.hudCard.shape = QStringLiteral("sharp");
        t.hudCard.cornerRadius = 0;
        t.hudCard.bgNormal = QStringLiteral("$bgCard");
        t.hudCard.bgActive = QStringLiteral("$bgCardActive");
        t.hudCard.borderNormal = QStringLiteral("$borderNormal");
        t.hudCard.borderActive = QStringLiteral("$phosphorColor");
        t.hudCard.audioIndicatorColor = QStringLiteral("$audioColor");

        t.windowBadge.shape = QStringLiteral("sharp");
        t.windowBadge.cornerRadius = 0;
        t.windowBadge.bgNormal = QStringLiteral("$bgCard");
        t.windowBadge.borderColor = QStringLiteral("$phosphorColor");

        t.searchHeader.promptPrefix = QStringLiteral("> ");
        t.searchHeader.cursorStyle = QStringLiteral("block_blink");
        t.hintsBar.keycapStyle = QStringLiteral("bracket");
        t.hintsBar.separator = QStringLiteral(" | ");
        t.hintsBar.keyColor = QStringLiteral("$phosphorColor");
        t.hintsBar.keyBgColor = QStringLiteral("#000000");
        t.hintsBar.labelColor = QStringLiteral("#00aa44");

        t.hudNav.standardIcon = QStringLiteral("[SYS]");
        t.hudNav.workspaceIcon = QStringLiteral("[DIR]");
        t.hudNav.allIcon = QStringLiteral("[NET]");
        t.hudNav.stashIcon = QStringLiteral("[MEM]");
        t.hudNav.newWsIcon = QStringLiteral("[+]");
        t.hudNav.arrowLeft = QStringLiteral("< ,");
        t.hudNav.arrowRight = QStringLiteral(". >");
        t.hudNav.tabShape = QStringLiteral("sharp");
        t.hudNav.tabCornerRadius = 0;

        t.spatialKeyboard.fontFamily = QStringLiteral("monospace");
        t.spatialKeyboard.keycapShape = QStringLiteral("sharp");
        t.spatialKeyboard.targetBadgeShape = QStringLiteral("sharp");
        t.spatialKeyboard.laserGlow = false;
        t.spatialKeyboard.laserDashStyle = QStringLiteral("dashed");
        t.spatialKeyboard.stackScaleReduction = 0.40;
        t.spatialKeyboard.stackOpacityReduction = 0.50;

        list.append(t);
    }

    // 4. Nordic Frost
    {
        Theme t;
        t.id = QStringLiteral("nordic-frost");
        t.name = QStringLiteral("Nordic Frost Glass (Light)");
        t.description = QStringLiteral("Airy light frosted-glass aesthetic with translucent snow cards, dark arctic charcoal text, and fluid pill capsules.");
        t.author = QStringLiteral("LeapK Core Team");
        t.isBuiltIn = true;

        ThemeParameter pFrost;
        pFrost.id = QStringLiteral("frostColor");
        pFrost.name = QStringLiteral("Frost Pastel Accent");
        pFrost.type = QStringLiteral("color");
        pFrost.colorDefault = QStringLiteral("#5e81ac");
        pFrost.colorCurrent = pFrost.colorDefault;
        t.parameters.append(pFrost);

        ThemeParameter pOpacity;
        pOpacity.id = QStringLiteral("frostOpacity");
        pOpacity.name = QStringLiteral("Glass Transparency");
        pOpacity.type = QStringLiteral("float");
        pOpacity.minValue = 0.3;
        pOpacity.maxValue = 0.95;
        pOpacity.step = 0.05;
        pOpacity.defaultValue = 0.88;
        pOpacity.currentValue = 0.88;
        pOpacity.suffix = QStringLiteral("%");
        t.parameters.append(pOpacity);

        ThemeParameter pAudioSpeed;
        pAudioSpeed.id = QStringLiteral("audioPulseSpeed");
        pAudioSpeed.name = QStringLiteral("Audio Pulse Speed");
        pAudioSpeed.type = QStringLiteral("float");
        pAudioSpeed.minValue = 0.2;
        pAudioSpeed.maxValue = 3.0;
        pAudioSpeed.step = 0.1;
        pAudioSpeed.defaultValue = 1.0;
        pAudioSpeed.currentValue = 1.0;
        pAudioSpeed.suffix = QStringLiteral("x");
        t.parameters.append(pAudioSpeed);

        ThemeParameter pAudioIntensity;
        pAudioIntensity.id = QStringLiteral("audioPulseIntensity");
        pAudioIntensity.name = QStringLiteral("Audio Pulse Intensity");
        pAudioIntensity.type = QStringLiteral("float");
        pAudioIntensity.minValue = 0.0;
        pAudioIntensity.maxValue = 2.5;
        pAudioIntensity.step = 0.1;
        pAudioIntensity.defaultValue = 1.0;
        pAudioIntensity.currentValue = 1.0;
        pAudioIntensity.suffix = QStringLiteral("x");
        t.parameters.append(pAudioIntensity);

        ThemeParameter pSoundProf;
        pSoundProf.id = QStringLiteral("soundProfile");
        pSoundProf.name = QStringLiteral("Sound Feedback Profile");
        pSoundProf.type = QStringLiteral("enum");
        pSoundProf.enumOptions = {QStringLiteral("crisp_click"), QStringLiteral("soft_pop"), QStringLiteral("scifi_blip"), QStringLiteral("mechanical")};
        pSoundProf.enumDefault = QStringLiteral("soft_pop");
        pSoundProf.enumCurrent = pSoundProf.enumDefault;
        t.parameters.append(pSoundProf);

        ThemeParameter pSoundVol;
        pSoundVol.id = QStringLiteral("soundVolume");
        pSoundVol.name = QStringLiteral("Theme Sound Volume");
        pSoundVol.type = QStringLiteral("float");
        pSoundVol.minValue = 0.0;
        pSoundVol.maxValue = 1.0;
        pSoundVol.step = 0.05;
        pSoundVol.defaultValue = 1.0;
        pSoundVol.currentValue = 1.0;
        pSoundVol.suffix = QStringLiteral("%");
        t.parameters.append(pSoundVol);

        t.sound.profile = QStringLiteral("soft_pop");

        t.tokens[QStringLiteral("bgSurface")] = QStringLiteral("#e5e9f0");
        t.tokens[QStringLiteral("bgCard")] = QStringLiteral("#ffffff");
        t.tokens[QStringLiteral("bgCardActive")] = QStringLiteral("#eceff4");
        t.tokens[QStringLiteral("textPrimary")] = QStringLiteral("#2e3440");
        t.tokens[QStringLiteral("textSecondary")] = QStringLiteral("#4c566a");
        t.tokens[QStringLiteral("borderColor")] = QStringLiteral("$frostColor");
        t.tokens[QStringLiteral("borderNormal")] = QStringLiteral("#d8dee9");
        t.tokens[QStringLiteral("audioColor")] = QStringLiteral("#d08770");

        t.hudContainer.shape = QStringLiteral("pill");
        t.hudContainer.cornerRadius = 18;
        t.hudContainer.borderWidth = 1.5;
        t.hudContainer.bgOpacity = 0.88;
        t.hudContainer.borderColor = QStringLiteral("$frostColor");

        t.hudCard.shape = QStringLiteral("rounded_rect");
        t.hudCard.cornerRadius = 14;
        t.hudCard.bgNormal = QStringLiteral("$bgCard");
        t.hudCard.bgActive = QStringLiteral("$bgCardActive");
        t.hudCard.borderNormal = QStringLiteral("$borderNormal");
        t.hudCard.borderActive = QStringLiteral("$frostColor");
        t.hudCard.audioIndicatorColor = QStringLiteral("$audioColor");
        t.hudCard.audioIndicatorScale = 1.15;

        t.windowBadge.shape = QStringLiteral("pill");
        t.windowBadge.cornerRadius = 12;
        t.windowBadge.bgNormal = QStringLiteral("$bgCard");
        t.windowBadge.borderColor = QStringLiteral("$frostColor");
        t.windowBadge.audioIndicatorColor = QStringLiteral("$audioColor");
        t.windowBadge.audioIndicatorScale = 1.15;

        t.searchHeader.promptPrefix = QStringLiteral("❄ ");
        t.hintsBar.keycapStyle = QStringLiteral("pill");
        t.hintsBar.separator = QStringLiteral(" • ");
        t.hintsBar.keyColor = QStringLiteral("$frostColor");
        t.hintsBar.keyBgColor = QStringLiteral("#e5e9f0");
        t.hintsBar.labelColor = QStringLiteral("$textSecondary");

        t.hudNav.standardIcon = QStringLiteral("❄");
        t.hudNav.workspaceIcon = QStringLiteral("◇");
        t.hudNav.allIcon = QStringLiteral("○");
        t.hudNav.stashIcon = QStringLiteral("📥");
        t.hudNav.newWsIcon = QStringLiteral("+");
        t.hudNav.arrowLeft = QStringLiteral("‹ ,");
        t.hudNav.arrowRight = QStringLiteral(". ›");
        t.hudNav.tabShape = QStringLiteral("pill");
        t.hudNav.tabCornerRadius = 14;

        t.spatialKeyboard.keycapShape = QStringLiteral("pill");
        t.spatialKeyboard.targetBadgeShape = QStringLiteral("pill");
        t.spatialKeyboard.laserGlow = true;
        t.spatialKeyboard.laserGlowIntensity = 1.2;
        t.spatialKeyboard.stackScaleReduction = 0.35;
        t.spatialKeyboard.stackOpacityReduction = 0.35;

        list.append(t);
    }

    return list;
}

// ============================================================================
// 5. File System & Theme Discovery
// ============================================================================

inline QString defaultThemesDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString path = base + QStringLiteral("/kwin-active-window-outline/themes");
    QDir().mkpath(path);
    return path;
}

inline QByteArray serializeTheme(const Theme &theme)
{
    const QJsonObject json = theme.toJson();
    const QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Indented);
}

inline bool parseTheme(const QByteArray &data, Theme &outTheme, QString *errorMessage = nullptr)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = doc.object();
    if (!root.contains(QStringLiteral("name")) && !root.contains(QStringLiteral("id"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Missing 'name' or 'id' field in theme JSON");
        }
        return false;
    }

    outTheme = Theme::fromJson(root);
    return true;
}

inline bool saveThemeToFile(const Theme &theme, const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray data = serializeTheme(theme);
    file.write(data);
    file.close();
    return true;
}

inline bool loadThemeFromFile(const QString &path, Theme &outTheme, QString *errorMessage = nullptr)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not open file: %1").arg(path);
        }
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    const bool ok = parseTheme(data, outTheme, errorMessage);
    if (ok) {
        outTheme.filePath = path;
    }
    return ok;
}

inline QList<Theme> discoverAllThemes()
{
    QList<Theme> all = builtInThemes();

    const QString userDir = defaultThemesDirectory();
    QDir dir(userDir);
    const QStringList files = dir.entryList({QStringLiteral("*.json"), QStringLiteral("*.leapk-theme")}, QDir::Files | QDir::Readable);

    for (const QString &file : files) {
        Theme userTheme;
        if (loadThemeFromFile(dir.filePath(file), userTheme)) {
            userTheme.isBuiltIn = false;
            // Overwrite built-in if user has overridden with same ID, or append
            bool replaced = false;
            for (int i = 0; i < all.size(); ++i) {
                if (all[i].id == userTheme.id) {
                    all[i] = userTheme;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                all.append(userTheme);
            }
        }
    }
    return all;
}

inline Theme findThemeById(const QString &id, const QJsonObject &userOverrides = QJsonObject())
{
    const QList<Theme> all = discoverAllThemes();
    for (Theme t : all) {
        if (t.id == id) {
            if (!userOverrides.isEmpty()) {
                t.applyParameterValues(userOverrides);
            }
            return t;
        }
    }
    // Fallback: Breeze Dark
    Theme fallback = builtInThemes().first();
    if (!userOverrides.isEmpty()) {
        fallback.applyParameterValues(userOverrides);
    }
    return fallback;
}

// ============================================================================
// 6. Shape Drawing & Window Theming Helpers
// ============================================================================

inline void addThemedShapePath(QPainterPath &path, const QRectF &rect, const QString &shape, int cornerRadius, int chamferSize = 6)
{
    if (shape == QStringLiteral("chamfered")) {
        const qreal c = qBound(2.0, (qreal)chamferSize, qMin(rect.width() / 2.0, rect.height() / 2.0));
        path.moveTo(rect.left() + c, rect.top());
        path.lineTo(rect.right() - c, rect.top());
        path.lineTo(rect.right(), rect.top() + c);
        path.lineTo(rect.right(), rect.bottom() - c);
        path.lineTo(rect.right() - c, rect.bottom());
        path.lineTo(rect.left() + c, rect.bottom());
        path.lineTo(rect.left(), rect.bottom() - c);
        path.lineTo(rect.left(), rect.top() + c);
        path.closeSubpath();
    } else if (shape == QStringLiteral("sharp")) {
        path.addRect(rect);
    } else if (shape == QStringLiteral("pill")) {
        const qreal maxR = (cornerRadius > 0) ? (qreal)cornerRadius : 14.0;
        const qreal r = (rect.height() > 44.0) ? qMin(rect.height() / 2.0, maxR) : (rect.height() / 2.0);
        path.addRoundedRect(rect, r, r);
    } else if (shape == QStringLiteral("circle")) {
        path.addEllipse(rect);
    } else {
        path.addRoundedRect(rect, cornerRadius, cornerRadius);
    }
}

inline void drawThemedShape(QPainter &p, const QRectF &rect, const QString &shape, int cornerRadius, int chamferSize = 6)
{
    if (shape == QStringLiteral("sharp")) {
        p.drawRect(rect);
    } else if (shape == QStringLiteral("pill")) {
        const qreal maxR = (cornerRadius > 0) ? (qreal)cornerRadius : 14.0;
        const qreal r = (rect.height() > 44.0) ? qMin(rect.height() / 2.0, maxR) : (rect.height() / 2.0);
        p.drawRoundedRect(rect, r, r);
    } else if (shape == QStringLiteral("circle")) {
        p.drawEllipse(rect);
    } else if (shape == QStringLiteral("chamfered")) {
        QPainterPath path;
        addThemedShapePath(path, rect, shape, cornerRadius, chamferSize);
        p.drawPath(path);
    } else {
        p.drawRoundedRect(rect, cornerRadius, cornerRadius);
    }
}

inline void paintThemedWindowFrame(QPainter *p, const QRectF &rect, const Theme &theme, const QColor &customAccent = QColor())
{
    p->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = rect.adjusted(1, 1, -1, -1);

    const QColor accent = customAccent.isValid() ? customAccent : theme.primaryAccentColor();
    const QColor bg = theme.surfaceBackgroundColor();
    const QColor border = theme.frameBorderColor();
    const double bWidth = theme.hudContainer.borderWidth > 0 ? theme.hudContainer.borderWidth : 1.5;
    const int radius = theme.cornerRadius();
    const QString shape = theme.hudContainer.shape;
    const int chamfer = theme.hudContainer.chamferSize > 0 ? theme.hudContainer.chamferSize : 8;

    // 1. Soft glow if enabled in theme
    if (theme.hudContainer.glowEnabled) {
        const QColor glowCol = theme.resolveColor(theme.hudContainer.glowColor, accent);
        for (int i = 3; i >= 1; --i) {
            QPen glowPen(QColor(glowCol.red(), glowCol.green(), glowCol.blue(), 25 / i), bWidth + i * 2.0);
            p->setPen(glowPen);
            p->setBrush(Qt::NoBrush);
            drawThemedShape(*p, r, shape, radius, chamfer);
        }
    }

    // 2. Main frame body
    p->setPen(QPen(border, bWidth));
    p->setBrush(bg);
    drawThemedShape(*p, r, shape, radius, chamfer);

    // 3. Top accent stripe
    p->setPen(Qt::NoPen);
    p->setBrush(accent);
    const qreal stripeMargin = (shape == QStringLiteral("sharp") || radius == 0) ? 0.0 : 24.0;
    const qreal stripeRadius = (shape == QStringLiteral("sharp") || radius == 0) ? 0.0 : 1.5;
    const QRectF stripeRect(r.left() + stripeMargin, r.top(), r.width() - (stripeMargin * 2.0), 3.0);
    if (stripeRadius > 0.0) {
        p->drawRoundedRect(stripeRect, stripeRadius, stripeRadius);
    } else {
        p->drawRect(stripeRect);
    }
}

inline QString generateDialogStyleSheet(const Theme &theme)
{
    const QColor accent = theme.primaryAccentColor();
    const QColor textPrimary = theme.textPrimaryColor();
    const QColor cardBg = theme.cardBackgroundColor(false);
    const QColor cardBorder = theme.cardBorderColor(false);
    const int cRadius = theme.cardCornerRadius();

    return QStringLiteral(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical {"
        "  background: rgba(255, 255, 255, 0.05); width: 8px; border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %1; border-radius: 4px; min-height: 24px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QLineEdit {"
        "  background: %2;"
        "  border: 1px solid %3;"
        "  border-radius: %4px;"
        "  color: %5;"
        "  font-size: 13px; padding: 6px 12px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1.5px solid %6;"
        "}"
    ).arg(
        theme.cardBorderColor(true).name(QColor::HexRgb),
        cardBg.name(QColor::HexArgb),
        cardBorder.name(QColor::HexArgb),
        QString::number(qBound(0, cRadius, 10)),
        textPrimary.name(QColor::HexRgb),
        accent.name(QColor::HexRgb)
    );
}

inline Theme loadActiveTheme(const QString &overrideThemeId = QString())
{
    QString themeId = overrideThemeId.trimmed();
    QJsonObject userOverrides;

    const QString kwinrcPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kwinrc");
    if (QFile::exists(kwinrcPath)) {
        QSettings settings(kwinrcPath, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("Effect-leapk"));
        if (!settings.contains(QStringLiteral("ActiveThemeId")) && !settings.contains(QStringLiteral("ActiveThemeParams"))) {
            settings.endGroup();
            settings.beginGroup(QStringLiteral("Effect-active-window-outline"));
        }
        if (themeId.isEmpty()) {
            themeId = settings.value(QStringLiteral("ActiveThemeId"), QStringLiteral("default-breeze")).toString();
        }
        const QString paramsStr = settings.value(QStringLiteral("ActiveThemeParams"), QString()).toString();
        if (!paramsStr.isEmpty()) {
            userOverrides = QJsonDocument::fromJson(paramsStr.toUtf8()).object();
        }
        settings.endGroup();
    }

    if (themeId.isEmpty()) {
        themeId = QStringLiteral("default-breeze");
    }

    return findThemeById(themeId, userOverrides);
}

} // namespace LeapKTheme
