#include "themehelper.h"
#include <cassert>
#include <iostream>

using namespace LeapKTheme;

static void testThemeParameter()
{
    ThemeParameter p;
    p.id = QStringLiteral("glowRadius");
    p.name = QStringLiteral("Glow Radius");
    p.type = QStringLiteral("float");
    p.minValue = 2.0;
    p.maxValue = 20.0;
    p.step = 0.5;
    p.defaultValue = 8.0;
    p.currentValue = 12.5;
    p.suffix = QStringLiteral(" px");

    const QJsonObject json = p.toJson();
    const ThemeParameter parsed = ThemeParameter::fromJson(json);

    assert(parsed.id == QStringLiteral("glowRadius"));
    assert(parsed.name == QStringLiteral("Glow Radius"));
    assert(parsed.type == QStringLiteral("float"));
    assert(std::abs(parsed.currentValue - 12.5) < 0.001);
    assert(parsed.suffix == QStringLiteral(" px"));

    // Color param
    ThemeParameter pCol;
    pCol.id = QStringLiteral("neon");
    pCol.type = QStringLiteral("color");
    pCol.colorDefault = QStringLiteral("#00f0ff");
    pCol.colorCurrent = QStringLiteral("#ff0055");
    const QJsonObject colJson = pCol.toJson();
    const ThemeParameter colParsed = ThemeParameter::fromJson(colJson);
    assert(colParsed.colorCurrent == QStringLiteral("#ff0055"));

    // Enum param
    ThemeParameter pEnum;
    pEnum.id = QStringLiteral("cardStyle");
    pEnum.type = QStringLiteral("enum");
    pEnum.enumOptions = {QStringLiteral("chip"), QStringLiteral("tile"), QStringLiteral("card")};
    pEnum.enumDefault = QStringLiteral("chip");
    pEnum.enumCurrent = QStringLiteral("card");
    const QJsonObject enumJson = pEnum.toJson();
    const ThemeParameter enumParsed = ThemeParameter::fromJson(enumJson);
    assert(enumParsed.enumOptions.size() == 3);
    assert(enumParsed.enumCurrent == QStringLiteral("card"));
}

static void testBuiltInThemes()
{
    const QList<Theme> builtins = builtInThemes();
    assert(builtins.size() >= 4);

    // 1. Breeze Dark
    const Theme breeze = builtins[0];
    assert(breeze.id == QStringLiteral("default-breeze"));
    assert(breeze.isBuiltIn);
    assert(!breeze.parameters.isEmpty());
    assert(breeze.hudContainer.shape == QStringLiteral("rounded_rect"));
    assert(breeze.windowBadge.shape == QStringLiteral("rounded_rect"));

    // 2. Cyberpunk Neon
    const Theme cyber = builtins[1];
    assert(cyber.id == QStringLiteral("cyberpunk-neon"));
    assert(cyber.hudContainer.shape == QStringLiteral("chamfered"));
    assert(cyber.hudContainer.glowEnabled);
    assert(cyber.searchHeader.promptPrefix == QStringLiteral("❯ "));

    // 3. Retro Terminal
    const Theme term = builtins[2];
    assert(term.id == QStringLiteral("retro-terminal"));
    assert(term.hudContainer.shape == QStringLiteral("sharp"));
    assert(term.hintsBar.keycapStyle == QStringLiteral("bracket"));

    // 4. Nordic Frost
    const Theme nordic = builtins[3];
    assert(nordic.id == QStringLiteral("nordic-frost"));
    assert(nordic.hudContainer.shape == QStringLiteral("pill"));
    assert(nordic.hintsBar.keycapStyle == QStringLiteral("pill"));
}

static void testVariableResolutionAndOverrides()
{
    Theme cyber = builtInThemes()[1]; // Cyberpunk
    assert(cyber.id == QStringLiteral("cyberpunk-neon"));

    // Default neonCyan is #00f0ff
    const QColor initialBorder = cyber.resolveColor(cyber.hudContainer.borderColor);
    if (initialBorder != QColor(QStringLiteral("#00f0ff"))) {
        std::cerr << "initialBorder mismatch!" << std::endl;
        return;
    }

    // User overrides neonCyan to #ffff00
    QJsonObject userOverrides;
    userOverrides[QStringLiteral("neonCyan")] = QStringLiteral("#ffff00");
    userOverrides[QStringLiteral("chamferSize")] = 14;
    cyber.applyParameterValues(userOverrides);

    const QColor updatedBorder = cyber.resolveColor(cyber.hudContainer.borderColor);
    if (updatedBorder != QColor(QStringLiteral("#ffff00"))) {
        std::cerr << "updatedBorder mismatch!" << std::endl;
        return;
    }

    const int updatedChamfer = cyber.resolveInt(QStringLiteral("$chamferSize"));
    if (updatedChamfer != 14) {
        std::cerr << "updatedChamfer mismatch!" << std::endl;
        return;
    }

    // Export user overrides
    const QJsonObject exported = cyber.exportParameterValues();
    assert(exported.value(QStringLiteral("neonCyan")).toString() == QStringLiteral("#ffff00"));
    assert(exported.value(QStringLiteral("chamferSize")).toInt() == 14);
}

static void testSerializationAndParsing()
{
    const Theme original = builtInThemes()[0];
    const QByteArray bytes = serializeTheme(original);
    assert(!bytes.isEmpty());

    Theme loaded;
    QString err;
    const bool ok = parseTheme(bytes, loaded, &err);
    if (!ok || !err.isEmpty()) {
        std::cerr << "parseTheme failed: " << err.toStdString() << std::endl;
        return;
    }
    assert(loaded.id == original.id);
    assert(loaded.name == original.name);
    assert(loaded.parameters.size() == original.parameters.size());
    assert(loaded.hudContainer.shape == original.hudContainer.shape);
    assert(loaded.windowBadge.shape == original.windowBadge.shape);
}

static void testThemeDiscoveryAndLookup()
{
    const QList<Theme> all = discoverAllThemes();
    assert(!all.isEmpty());

    // Known theme lookup
    const Theme cyber = findThemeById(QStringLiteral("cyberpunk-neon"));
    assert(cyber.id == QStringLiteral("cyberpunk-neon"));

    // Unknown ID fallback
    const Theme fallback = findThemeById(QStringLiteral("non-existent-theme-xyz"));
    assert(fallback.id == QStringLiteral("default-breeze"));
}

int main(int, char **)
{
    testThemeParameter();
    testBuiltInThemes();
    testVariableResolutionAndOverrides();
    testSerializationAndParsing();
    testThemeDiscoveryAndLookup();

    std::cout << "All theme tests passed successfully." << std::endl;
    return 0;
}
