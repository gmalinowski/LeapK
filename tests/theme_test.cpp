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
    assert(breeze.hudNav.standardIcon == QStringLiteral("💻"));
    assert(breeze.hudNav.tabShape == QStringLiteral("pill"));

    // 2. Cyberpunk Neon
    const Theme cyber = builtins[1];
    assert(cyber.id == QStringLiteral("cyberpunk-neon"));
    assert(cyber.hudContainer.shape == QStringLiteral("chamfered"));
    assert(cyber.hudContainer.glowEnabled);
    assert(cyber.searchHeader.promptPrefix == QStringLiteral("❯ "));
    assert(cyber.hudCard.audioIndicatorStyle == QStringLiteral("soundwave_pulse"));
    assert(cyber.hudCard.audioIndicatorColor == QStringLiteral("$neonPink"));
    assert(cyber.hudCard.audioIndicatorScale > 1.2);
    assert(cyber.windowBadge.audioIndicatorStyle == QStringLiteral("soundwave_pulse"));
    assert(cyber.windowBadge.audioPulseBadge);
    assert(cyber.hudNav.standardIcon == QStringLiteral("⚡"));
    assert(cyber.hudNav.workspaceIcon == QStringLiteral("◈"));
    assert(cyber.hudNav.allIcon == QStringLiteral("⬡"));
    assert(cyber.hudNav.stashIcon == QStringLiteral("💾"));
    assert(cyber.hudNav.tabShape == QStringLiteral("chamfered"));

    // 3. Retro Terminal
    const Theme term = builtins[2];
    assert(term.id == QStringLiteral("retro-terminal"));
    assert(term.hudContainer.shape == QStringLiteral("sharp"));
    assert(term.hintsBar.keycapStyle == QStringLiteral("bracket"));
    assert(term.hudNav.standardIcon == QStringLiteral("[SYS]"));
    assert(term.hudNav.workspaceIcon == QStringLiteral("[DIR]"));
    assert(term.hudNav.allIcon == QStringLiteral("[NET]"));
    assert(term.hudNav.stashIcon == QStringLiteral("[MEM]"));
    assert(term.hudNav.tabShape == QStringLiteral("sharp"));

    // 4. Nordic Frost
    const Theme nordic = builtins[3];
    assert(nordic.id == QStringLiteral("nordic-frost"));
    assert(nordic.hudContainer.shape == QStringLiteral("pill"));
    assert(nordic.hudCard.shape == QStringLiteral("rounded_rect"));
    assert(nordic.windowBadge.shape == QStringLiteral("pill"));
    assert(nordic.hintsBar.keycapStyle == QStringLiteral("pill"));
    assert(nordic.windowBadge.audioIndicatorScale > 1.1);
    assert(nordic.windowBadge.audioPulseBadge);
    assert(nordic.hudNav.standardIcon == QStringLiteral("❄"));
    assert(nordic.hudNav.workspaceIcon == QStringLiteral("◇"));
    assert(nordic.hudNav.allIcon == QStringLiteral("○"));
    assert(nordic.hudNav.stashIcon == QStringLiteral("📥"));
    assert(nordic.hudNav.tabShape == QStringLiteral("pill"));
}

static void testHudNavThemeSettings()
{
    HudNavTheme hnt;
    assert(hnt.standardIcon == QStringLiteral("💻"));
    assert(hnt.tabShape == QStringLiteral("pill"));
    assert(std::abs(hnt.iconScale - 1.0) < 0.001);

    // Test JSON roundtrip
    hnt.standardIcon = QStringLiteral("⚡");
    hnt.workspaceIcon = QStringLiteral("◈");
    hnt.allIcon = QStringLiteral("⬡");
    hnt.stashIcon = QStringLiteral("💾");
    hnt.newWsIcon = QStringLiteral("＋");
    hnt.arrowLeft = QStringLiteral("« ,");
    hnt.arrowRight = QStringLiteral(". »");
    hnt.tabShape = QStringLiteral("chamfered");
    hnt.tabChamferSize = 7;
    hnt.iconScale = 1.35;

    const QJsonObject json = hnt.toJson();
    const HudNavTheme parsed = HudNavTheme::fromJson(json);

    assert(parsed.standardIcon == QStringLiteral("⚡"));
    assert(parsed.workspaceIcon == QStringLiteral("◈"));
    assert(parsed.allIcon == QStringLiteral("⬡"));
    assert(parsed.stashIcon == QStringLiteral("💾"));
    assert(parsed.newWsIcon == QStringLiteral("＋"));
    assert(parsed.arrowLeft == QStringLiteral("« ,"));
    assert(parsed.arrowRight == QStringLiteral(". »"));
    assert(parsed.tabShape == QStringLiteral("chamfered"));
    assert(parsed.tabChamferSize == 7);
    assert(std::abs(parsed.iconScale - 1.35) < 0.001);
}

static void testAudioThemeSettings()
{
    // Test defaults
    WindowBadgeTheme wbt;
    assert(wbt.audioIndicatorStyle == QStringLiteral("speaker_icon"));
    assert(wbt.audioIndicatorColor == QStringLiteral("$audioColor"));
    assert(std::abs(wbt.audioIndicatorScale - 1.0) < 0.001);
    assert(wbt.audioPulseBadge == true);
    assert(std::abs(wbt.audioPulseSpeed - 1.0) < 0.001);
    assert(std::abs(wbt.audioPulseIntensity - 1.0) < 0.001);

    HudCardTheme hct;
    assert(hct.audioIndicatorStyle == QStringLiteral("speaker_icon"));
    assert(hct.audioIndicatorColor == QStringLiteral("$audioColor"));
    assert(std::abs(hct.audioIndicatorScale - 1.0) < 0.001);
    assert(hct.audioPulseBadge == true);
    assert(std::abs(hct.audioPulseSpeed - 1.0) < 0.001);
    assert(std::abs(hct.audioPulseIntensity - 1.0) < 0.001);

    // Test JSON round-trip for WindowBadgeTheme
    wbt.audioIndicatorStyle = QStringLiteral("soundwave_pulse");
    wbt.audioIndicatorColor = QStringLiteral("#ff007f");
    wbt.audioIndicatorScale = 1.45;
    wbt.audioPulseBadge = false;
    wbt.audioPulseSpeed = 1.75;
    wbt.audioPulseIntensity = 0.5;

    const QJsonObject wbJson = wbt.toJson();
    const WindowBadgeTheme wbParsed = WindowBadgeTheme::fromJson(wbJson);
    assert(wbParsed.audioIndicatorStyle == QStringLiteral("soundwave_pulse"));
    assert(wbParsed.audioIndicatorColor == QStringLiteral("#ff007f"));
    assert(std::abs(wbParsed.audioIndicatorScale - 1.45) < 0.001);
    assert(wbParsed.audioPulseBadge == false);
    assert(std::abs(wbParsed.audioPulseSpeed - 1.75) < 0.001);
    assert(std::abs(wbParsed.audioPulseIntensity - 0.5) < 0.001);

    // Test JSON round-trip for HudCardTheme
    hct.audioIndicatorStyle = QStringLiteral("minimal_dot");
    hct.audioIndicatorColor = QStringLiteral("#00f0ff");
    hct.audioIndicatorScale = 1.8;
    hct.audioPulseBadge = true;
    hct.audioPulseSpeed = 2.0;
    hct.audioPulseIntensity = 1.25;

    const QJsonObject hcJson = hct.toJson();
    const HudCardTheme hcParsed = HudCardTheme::fromJson(hcJson);
    assert(hcParsed.audioIndicatorStyle == QStringLiteral("minimal_dot"));
    assert(hcParsed.audioIndicatorColor == QStringLiteral("#00f0ff"));
    assert(std::abs(hcParsed.audioIndicatorScale - 1.8) < 0.001);
    assert(hcParsed.audioPulseBadge == true);
    assert(std::abs(hcParsed.audioPulseSpeed - 2.0) < 0.001);
    assert(std::abs(hcParsed.audioPulseIntensity - 1.25) < 0.001);

    // Test parameter resolution
    Theme breeze = builtInThemes()[0];
    assert(std::abs(breeze.resolveDouble(QStringLiteral("$audioPulseSpeed"), 1.0) - 1.0) < 0.001);
    assert(std::abs(breeze.resolveDouble(QStringLiteral("$audioPulseIntensity"), 1.0) - 1.0) < 0.001);
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

static void testSoundThemeSettings()
{
    SoundTheme st;
    assert(st.enabled == true);
    assert(std::abs(st.volume - 1.0) < 0.001);
    assert(st.profile == QStringLiteral("crisp_click"));
    assert(st.enterSound.isEmpty());

    st.enabled = true;
    st.volume = 0.8;
    st.profile = QStringLiteral("soft_pop");
    st.workspaceSound = QStringLiteral("/path/to/sound.wav");

    const QJsonObject json = st.toJson();
    const SoundTheme parsed = SoundTheme::fromJson(json);
    assert(parsed.enabled == true);
    assert(std::abs(parsed.volume - 0.8) < 0.001);
    assert(parsed.profile == QStringLiteral("soft_pop"));
    assert(parsed.workspaceSound == QStringLiteral("/path/to/sound.wav"));

    // Check profiles across builtins
    const auto builtins = builtInThemes();
    assert(builtins[0].sound.profile == QStringLiteral("crisp_click"));
    assert(builtins[1].sound.profile == QStringLiteral("scifi_blip"));
    assert(builtins[2].sound.profile == QStringLiteral("mechanical"));
    assert(builtins[3].sound.profile == QStringLiteral("soft_pop"));

    // Test parameter resolution
    assert(builtins[1].resolveVar(QStringLiteral("$soundProfile")) == QStringLiteral("scifi_blip"));
    assert(std::abs(builtins[1].resolveDouble(QStringLiteral("$soundVolume"), 1.0) - 1.0) < 0.001);
}

int main(int, char **)
{
    testThemeParameter();
    testBuiltInThemes();
    testAudioThemeSettings();
    testSoundThemeSettings();
    testHudNavThemeSettings();
    testVariableResolutionAndOverrides();
    testSerializationAndParsing();
    testThemeDiscoveryAndLookup();

    std::cout << "All theme tests passed successfully." << std::endl;
    return 0;
}
