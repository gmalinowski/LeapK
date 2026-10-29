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
#include <QPointF>
#include <QRectF>
#include <QRegularExpression>
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

        t.hudCard.shape = QStringLiteral("pill");
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

} // namespace LeapKTheme
