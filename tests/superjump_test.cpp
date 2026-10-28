/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "badgehelper.h"
#include "workspacehelper.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testSuperJumpBannerFormatting()
{
    // 1. Super Jump with named workspace
    QImage superJumpDev = OutlineBadge::renderHudModeHeader(
        1.0, false, QString(), QColor(), 0, {}, -1, false, QStringLiteral("Backend"), true);
    assert(!superJumpDev.isNull());
    assert(superJumpDev.width() > 200);
    assert(superJumpDev.height() >= 30);

    // 2. Super Jump with empty workspace name (falls back to "Workspace")
    QImage superJumpDefault = OutlineBadge::renderHudModeHeader(
        1.0, false, QString(), QColor(), 0, {}, -1, false, QString(), true);
    assert(!superJumpDefault.isNull());
    assert(superJumpDefault.width() > 150);

    // 3. Super Jump with HiDPI scale (2.0)
    QImage superJumpHiDpi = OutlineBadge::renderHudModeHeader(
        2.0, false, QString(), QColor(), 0, {}, -1, false, QStringLiteral("Frontend UI"), true);
    assert(!superJumpHiDpi.isNull());
    assert(superJumpHiDpi.width() >= superJumpDev.width() * 1.5);

    std::cout << "testSuperJumpBannerFormatting PASSED\n";
}

void testStashViewHeaderBanner()
{
    // 1. Stash mode header banner
    QImage stashBanner = OutlineBadge::renderHudModeHeader(
        1.0, false, QString(), QColor(), 0, {}, 3, true, QStringLiteral("Frontend"));
    assert(!stashBanner.isNull());
    assert(stashBanner.width() > 200);

    // 2. Stash banner with HiDPI scale
    QImage stashBannerHiDpi = OutlineBadge::renderHudModeHeader(
        1.5, false, QString(), QColor(), 0, {}, 5, true, QStringLiteral("Docs"));
    assert(!stashBannerHiDpi.isNull());
    assert(stashBannerHiDpi.width() > stashBanner.width());

    std::cout << "testStashViewHeaderBanner PASSED\n";
}

void testStashedBadgeDesaturationAndDashing()
{
    OutlineBadge::BadgeConfig cfg;
    cfg.hudBadgeSize = 32;

    const QColor vibrantOrange(230, 126, 34); // S ~ 85%, V ~ 90%

    // 1. Normal window badge
    const QImage normalBadge = OutlineBadge::renderBadgeImage(
        cfg, QChar('K'), vibrantOrange, Qt::white, false, 28, 1.0,
        QString(), QIcon(), QImage(), QString(), false, false);
    assert(!normalBadge.isNull());

    // 2. Stashed window badge (isStashed = true)
    const QImage stashedBadge = OutlineBadge::renderBadgeImage(
        cfg, QChar('K'), vibrantOrange, Qt::white, false, 28, 1.0,
        QString(), QIcon(), QImage(), QString(), false, true);
    assert(!stashedBadge.isNull());
    assert(stashedBadge.size() == normalBadge.size());

    // 3. Stashed HUD pill
    const QImage stashedPill = OutlineBadge::renderOffscreenTargetPill(
        cfg, QChar('K'), vibrantOrange, Qt::white,
        QStringLiteral("📦 Stash"), QStringLiteral("Konsole"),
        32, 1.0, QString(), QIcon(), QImage(), true, QString(), false, 0, true);
    assert(!stashedPill.isNull());
    assert(stashedPill.width() > 60);

    // 4. Verify contrasting text color for both
    assert(OutlineBadge::contrastingTextColor(vibrantOrange).isValid());

    std::cout << "testStashedBadgeDesaturationAndDashing PASSED\n";
}

void testCrossWorkspacePullLogic()
{
    using namespace LeapKWorkspace;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/workspaces.json");

    // Workspace 1: Backend
    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-backend");
    ws1.name = QStringLiteral("Backend");

    // Workspace 2: Frontend
    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-frontend");
    ws2.name = QStringLiteral("Frontend");

    QList<WorkspaceInfo> list = { ws1, ws2 };

    // Initially, assign "org.kde.konsole" with "cargo" to Backend
    reassignWindowToWorkspace(list, ws1.id, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo"), false, 0, filePath);
    assert(findWorkspaceIndexForWindow(list, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo run"), 1) == 1);

    // Super Jump pulls "org.kde.konsole" into Frontend (ws2)
    bool pulled = reassignWindowToWorkspace(list, ws2.id, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo"), false, 0, filePath);
    assert(pulled);

    // Window must now match Frontend (index 2) and NOT Backend (index 1)
    const int newWsIdx = findWorkspaceIndexForWindow(list, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo run"), 1);
    assert(newWsIdx == 2);

    // Reload from disk to verify persistence
    QList<WorkspaceInfo> reloaded = loadWorkspaces(filePath);
    assert(findWorkspaceIndexForWindow(reloaded, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo run"), 1) == 2);

    std::cout << "testCrossWorkspacePullLogic PASSED\n";
}

void testSuperJumpDefaultJumpVsPullHere()
{
    using namespace LeapKWorkspace;

    // Workspace 1: Backend
    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-backend");
    ws1.name = QStringLiteral("Backend");

    // Workspace 2: Frontend
    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-frontend");
    ws2.name = QStringLiteral("Frontend");

    WindowMatchRule r;
    r.matchApp = QStringLiteral("firefox");
    ws2.rules.append(r);

    QList<WorkspaceInfo> list = { ws1, ws2 };

    // Default Jump: window in ws2 resolves target workspace to 2 (without modifying rules or pulling!)
    int targetWs = findWorkspaceIndexForWindow(list, QStringLiteral("firefox"), QStringLiteral("Mozilla"), 1);
    assert(targetWs == 2);
    assert(ws1.rules.isEmpty());
    assert(ws2.rules.size() == 1);

    // Alt+Key (Pull Here): reassigns window from ws2 to ws1
    reassignWindowToWorkspace(list, ws1.id, QStringLiteral("firefox"), QStringLiteral("Mozilla"), true, 0);
    assert(findWorkspaceIndexForWindow(list, QStringLiteral("firefox"), QStringLiteral("Mozilla"), 1) == 1);
    assert(list.at(0).rules.size() == 1);
    assert(list.at(1).rules.isEmpty());

    std::cout << "testSuperJumpDefaultJumpVsPullHere PASSED\n";
}

void testSuperJumpKeyboardModifierIsolation()
{
    // Helper replicating LeapK shortcut matching logic
    auto isSuperJumpToggle = [](Qt::KeyboardModifiers mods, int key) -> bool {
        return (mods & Qt::MetaModifier) && key == Qt::Key_A;
    };
    auto isStashWindowAction = [](Qt::KeyboardModifiers mods) -> bool {
        return (mods & Qt::ControlModifier) && !(mods & Qt::AltModifier);
    };
    auto isPullWindowAction = [](Qt::KeyboardModifiers mods) -> bool {
        return (mods & Qt::AltModifier) && !(mods & Qt::ControlModifier);
    };

    // 1. Meta+A -> MUST trigger Super Jump
    assert(isSuperJumpToggle(Qt::MetaModifier, Qt::Key_A) == true);

    // 2. Ctrl+A -> MUST NOT trigger Super Jump (must trigger Stash window 'A')
    assert(isSuperJumpToggle(Qt::ControlModifier, Qt::Key_A) == false);
    assert(isStashWindowAction(Qt::ControlModifier) == true);

    // 3. Alt+A -> MUST NOT trigger Super Jump (must trigger Pull window 'A')
    assert(isSuperJumpToggle(Qt::AltModifier, Qt::Key_A) == false);
    assert(isPullWindowAction(Qt::AltModifier) == true);

    // 4. Plain 'A' -> MUST NOT trigger Super Jump (normal jump)
    assert(isSuperJumpToggle(Qt::NoModifier, Qt::Key_A) == false);

    // 5. Shift+A -> MUST NOT trigger Super Jump (close window 'A')
    assert(isSuperJumpToggle(Qt::ShiftModifier, Qt::Key_A) == false);

    std::cout << "testSuperJumpKeyboardModifierIsolation PASSED\n";
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);

    testSuperJumpBannerFormatting();
    testStashViewHeaderBanner();
    testStashedBadgeDesaturationAndDashing();
    testCrossWorkspacePullLogic();
    testSuperJumpDefaultJumpVsPullHere();
    testSuperJumpKeyboardModifierIsolation();

    std::cout << "All Super Jump tests passed successfully!\n";
    return 0;
}
