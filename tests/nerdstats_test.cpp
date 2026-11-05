/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "nerdstatshelper.h"
#include "badgehelper.h"
#include <QGuiApplication>
#include <cmath>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testRollingAverage()
{
    LeapKNerdStats::RollingAverage avg(0.1);
    assert(!avg.isInitialized());
    assert(avg.average() == 0.0);

    // Initial sample sets the base average directly without lag
    avg.addSample(10.0);
    assert(avg.isInitialized());
    assert(std::abs(avg.average() - 10.0) < 1e-6);

    // Second sample applies exponential moving average smoothing
    // S_1 = 0.1 * 20.0 + 0.9 * 10.0 = 11.0
    avg.addSample(20.0);
    assert(std::abs(avg.average() - 11.0) < 1e-6);

    // Third sample: S_2 = 0.1 * 20.0 + 0.9 * 11.0 = 11.9
    avg.addSample(20.0);
    assert(std::abs(avg.average() - 11.9) < 1e-6);

    // Verify reset
    avg.reset();
    assert(!avg.isInitialized());
    assert(avg.average() == 0.0);

    std::cout << "testRollingAverage PASSED\n";
}

void testHardwareSensors()
{
    bool ok = false;
    const double watts = LeapKNerdStats::queryHardwarePowerWatts(&ok);
    assert(watts >= 0.0);

    const double temp = LeapKNerdStats::queryHardwareTemperatureC();
    assert(temp >= 0.0);

    std::cout << "testHardwareSensors PASSED (hwmon watts=" << watts << ", temp=" << temp << "C)\n";
}

void testRenderNerdStatsCard()
{
    LeapKNerdStats::NerdStatsSnapshot stats;
    stats.avgLeapkWatts = 0.85;
    stats.avgPackagePowerWatts = 14.5;
    stats.avgCpuPercent = 1.2;
    stats.avgFrameLatencyMs = 0.42;
    stats.refreshRateHz = 60;
    stats.activeShaderPasses = 3;
    stats.textureMemoryMb = 4.8;
    stats.hardwareTempC = 48.0;
    stats.hasHardwarePower = true;

    // 1. Render at 1.0 scale
    const QImage card1 = LeapKNerdStats::renderNerdStatsCard(1.0, stats);
    assert(!card1.isNull());
    assert(card1.width() == 330);
    assert(card1.height() == 165);
    assert(card1.format() == QImage::Format_ARGB32_Premultiplied);

    // 2. Render at HiDPI 2.0 scale
    const QImage card2 = LeapKNerdStats::renderNerdStatsCard(2.0, stats);
    assert(!card2.isNull());
    assert(card2.width() == 660);
    assert(card2.height() == 330);

    // 3. Render idle fallback without package power
    LeapKNerdStats::NerdStatsSnapshot idleStats;
    idleStats.avgLeapkWatts = 0.0;
    idleStats.avgPackagePowerWatts = 0.0;
    idleStats.avgCpuPercent = 0.0;
    idleStats.avgFrameLatencyMs = 0.0;
    idleStats.refreshRateHz = 60;
    idleStats.activeShaderPasses = 1;
    const QImage cardIdle = LeapKNerdStats::renderNerdStatsCard(1.0, idleStats);
    assert(!cardIdle.isNull());

    std::cout << "testRenderNerdStatsCard PASSED\n";
}

void testHudMetaOverlayWithNerdStats()
{
    const LeapKTheme::Theme theme;

    // Inactive Nerd Stats
    const QImage overlayOff = OutlineBadge::renderHudMetaOverlayImage(
        1.0, theme, false, false, false, false, false, false, false, false, false);
    assert(!overlayOff.isNull());

    // Active Nerd Stats
    const QImage overlayOn = OutlineBadge::renderHudMetaOverlayImage(
        1.0, theme, false, false, false, false, false, false, false, false, true);
    assert(!overlayOn.isNull());
    assert(overlayOn.width() > 300);
    assert(overlayOn.height() > 200);

    std::cout << "testHudMetaOverlayWithNerdStats PASSED\n";
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    testRollingAverage();
    testHardwareSensors();
    testRenderNerdStatsCard();
    testHudMetaOverlayWithNerdStats();

    std::cout << "\nAll LeapK Nerd Stats tests PASSED!\n";
    return 0;
}
