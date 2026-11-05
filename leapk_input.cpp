/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk_internal.h"

namespace KWin
{

void ActiveWindowOutlineEffect::toggleSuperJumpMode()
{
    if (m_tilingPresetsModeActive) {
        exitTilingPresetsMode();
    }
    if (m_subLeapActive) {
        exitSubLeapMode();
    }

    m_superJumpActive = !m_superJumpActive;
    if (m_superJumpActive) {
        m_superJumpTargetWorkspace = m_activeHudView;
    }

    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::exitSuperJumpMode()
{
    if (!m_superJumpActive) {
        return;
    }
    if (m_crossWsActionActive) {
        exitCrossWsActionMode();
    }
    m_superJumpActive = false;
    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::enterCrossWsActionMode(EffectWindow *target)
{
    if (!target || target->isDeleted()) {
        return;
    }

    m_crossWsActionActive = true;
    m_crossWsTargetWindow = target;
    m_crossWsTargetWorkspace = findWorkspaceForWindow(target);
    m_crossWsCurrentWorkspace = (m_superJumpTargetWorkspace >= 1 && m_superJumpTargetWorkspace <= m_workspaces.size())
                                ? m_superJumpTargetWorkspace
                                : (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size() ? m_activeHudView : 0);

    if (m_crossWsTargetWorkspace <= 0 && m_pinnedWindows.contains(target)) {
        const auto pinned = m_windowPinnedWorkspaces.value(target);
        for (int ws : pinned) {
            if (ws >= 1 && ws <= m_workspaces.size()) {
                m_crossWsTargetWorkspace = ws;
                break;
            }
        }
    }
    if (m_crossWsTargetWorkspace <= 0 && effects && !target->desktops().isEmpty()) {
        const QString deskId = target->desktops().first()->id();
        for (int i = 0; i < m_workspaces.size(); ++i) {
            if (m_workspaces.at(i).lastVirtualDesktopId == deskId ||
                m_workspaceLastVirtualDesktopId.value(m_workspaces.at(i).id) == deskId) {
                m_crossWsTargetWorkspace = i + 1;
                break;
            }
        }
        if (m_crossWsTargetWorkspace <= 0) {
            const int dNum = target->desktops().first()->x11DesktopNumber();
            if (dNum >= 1 && dNum <= m_workspaces.size()) {
                m_crossWsTargetWorkspace = dNum;
            }
        }
    }

    if (m_crossWsCurrentWorkspace <= 0 && effects && effects->currentDesktop()) {
        const QString currDeskId = effects->currentDesktop()->id();
        for (int i = 0; i < m_workspaces.size(); ++i) {
            if (m_workspaces.at(i).lastVirtualDesktopId == currDeskId ||
                m_workspaceLastVirtualDesktopId.value(m_workspaces.at(i).id) == currDeskId) {
                m_crossWsCurrentWorkspace = i + 1;
                break;
            }
        }
        if (m_crossWsCurrentWorkspace <= 0) {
            const int currDNum = effects->currentDesktop()->x11DesktopNumber();
            if (currDNum >= 1 && currDNum <= m_workspaces.size()) {
                m_crossWsCurrentWorkspace = currDNum;
            }
        }
    }

    if (m_crossWsTargetWorkspace >= 1 && m_crossWsTargetWorkspace <= m_workspaces.size()) {
        m_crossWsTargetWsName = m_workspaces.at(m_crossWsTargetWorkspace - 1).name;
    } else if (isWindowInStash(target)) {
        m_crossWsTargetWsName = QStringLiteral("Stash");
    } else if (!target->isOnCurrentDesktop()) {
        const auto desktops = target->desktops();
        const int dNum = (!desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
        m_crossWsTargetWsName = dNum > 0 ? QStringLiteral("Desktop %1").arg(dNum) : QStringLiteral("Other Desktop");
    } else {
        m_crossWsTargetWsName = QStringLiteral("Standard");
    }

    if (m_crossWsCurrentWorkspace >= 1 && m_crossWsCurrentWorkspace <= m_workspaces.size()) {
        m_crossWsCurrentWsName = m_workspaces.at(m_crossWsCurrentWorkspace - 1).name;
    } else {
        const int currDNum = (effects && effects->currentDesktop()) ? effects->currentDesktop()->x11DesktopNumber() : 0;
        m_crossWsCurrentWsName = currDNum > 0 ? QStringLiteral("Desktop %1").arg(currDNum) : QStringLiteral("Standard");
    }

    m_crossWsActionOverlayTexture.reset();
    m_crossWsActionOverlayCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;

    playThemedSound(OutlineSound::SoundEffect::NavTab);

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::exitCrossWsActionMode()
{
    if (!m_crossWsActionActive) {
        return;
    }

    m_crossWsActionActive = false;
    m_crossWsTargetWindow = nullptr;
    m_crossWsActionOverlayTexture.reset();
    m_crossWsActionOverlayCacheKey = -1;
    m_pinWsSelectionActive = false;
    m_pinTargetWindow = nullptr;
    m_pinWsOverlayTexture.reset();
    m_pinWsOverlayCacheKey = -1;
    m_pinWsDialogScreenRect = QRectF();
    m_pinWsCancelScreenRect = QRectF();
    m_pinWsRowScreenRects.clear();
    m_pinWsRelRowRects.clear();
    m_pinWsRelCancelRect = QRectF();
    m_hudThumbnailZoomActive = false;
    m_nerdStatsActive = m_nerdStatsEnabled;
    if (m_nerdStatsTimer) {
        if (m_nerdStatsActive) {
            m_nerdStatsTimer->start(500);
        } else {
            m_nerdStatsTimer->stop();
        }
    }
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_crossWsDialogScreenRect = QRectF();
    m_crossWsJumpScreenRect = QRectF();
    m_crossWsMoveScreenRect = QRectF();
    m_crossWsPinScreenRect = QRectF();
    m_crossWsCancelScreenRect = QRectF();

    playThemedSound(OutlineSound::SoundEffect::Dismiss);

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::executeCrossWsAction(int actionType)
{
    EffectWindow *target = m_crossWsTargetWindow.data();
    const int currentWs = m_crossWsCurrentWorkspace;
    const int targetWs = m_crossWsTargetWorkspace;
    exitCrossWsActionMode();

    if (!target || target->isDeleted()) {
        cancelJumpMode();
        return;
    }

    if (actionType == 1) {
        // Jump to Workspace
        activateJumpTargetWindow(target);
        return;
    }

    if (actionType == 2) {
        // Move to Current Workspace
        if (m_pinnedWindows.contains(target)) {
            unpinWindow(target, currentWs);
        } else {
            if (currentWs >= 1 && currentWs <= m_workspaces.size()) {
                addWindowToWorkspace(target, currentWs);
                applyWorkspaceViewIsolation(currentWs);
            } else if (currentWs == 0) {
                addWindowToWorkspace(target, 0);
            }
            if (effects && effects->currentDesktop()) {
                effects->windowToDesktops(target, {effects->currentDesktop()});
            }
            m_workspaceMinimizedWindows.remove(target);
            m_stashedWindows.remove(target);
            if (target->isMinimized()) {
                target->setMinimized(false);
            }
        }
        cancelJumpMode();
        if (m_tilingActive) {
            performTilingSwap(target);
        } else {
            effects->activateWindow(target);
        }
        warpCursorToWindow(target);
        return;
    }

    if (actionType == 3) {
        // Pin to Both Workspaces (target's workspace and current workspace)
        QSet<int> targetWorkspaces;
        if (targetWs >= 1 && targetWs <= m_workspaces.size()) {
            targetWorkspaces.insert(targetWs);
        }
        if (currentWs >= 1 && currentWs <= m_workspaces.size()) {
            targetWorkspaces.insert(currentWs);
        }

        // If target was already pinned to some specific workspaces, preserve them
        if (m_pinnedWindows.contains(target)) {
            const auto existing = m_windowPinnedWorkspaces.value(target);
            for (int ws : existing) {
                if (ws >= 1 && ws <= m_workspaces.size()) {
                    targetWorkspaces.insert(ws);
                }
            }
        }

        pinWindowToWorkspaces(target, targetWorkspaces);
        m_workspaceMinimizedWindows.remove(target);
        m_stashedWindows.remove(target);
        if (target->isMinimized()) {
            target->setMinimized(false);
        }
        if (currentWs >= 1 && currentWs <= m_workspaces.size()) {
            applyWorkspaceViewIsolation(currentWs);
        }
        cancelJumpMode();
        if (m_tilingActive) {
            performTilingSwap(target);
        } else {
            effects->activateWindow(target);
        }
        warpCursorToWindow(target);
        return;
    }
}

bool ActiveWindowOutlineEffect::isCrossWorkspaceTarget(EffectWindow *target) const
{
    if (!target || target->isDeleted()) {
        return false;
    }
    const int targetWs = findWorkspaceForWindow(target);
    const int currentWs = (m_superJumpTargetWorkspace >= 1 && m_superJumpTargetWorkspace <= m_workspaces.size())
                          ? m_superJumpTargetWorkspace
                          : (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size() ? m_activeHudView : 0);
    if (targetWs != currentWs) {
        return true;
    }
    if (effects && effects->currentDesktop() && !target->isOnCurrentDesktop() && !target->isOnAllDesktops()) {
        return true;
    }
    if (isWindowInStash(target)) {
        return true;
    }
    return false;
}

bool ActiveWindowOutlineEffect::handleSuperJumpCrossWorkspaceSelection(EffectWindow *target, int keyToPass)
{
    if (!m_superJumpActive || !isCrossWorkspaceTarget(target)) {
        return false;
    }

    if (m_superJumpCrossWsAction == 1) {
        // Classic: Direct Jump
        return false;
    }

    if (m_superJumpCrossWsAction == 2) {
        // Direct Move to Current Workspace
        const int currentWs = (m_superJumpTargetWorkspace >= 1 && m_superJumpTargetWorkspace <= m_workspaces.size())
                              ? m_superJumpTargetWorkspace
                              : (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size() ? m_activeHudView : 0);
        if (m_pinnedWindows.contains(target)) {
            unpinWindow(target, currentWs);
        } else {
            if (currentWs >= 1 && currentWs <= m_workspaces.size()) {
                addWindowToWorkspace(target, currentWs);
                applyWorkspaceViewIsolation(currentWs);
            } else if (currentWs == 0) {
                addWindowToWorkspace(target, 0);
            }
            if (effects && effects->currentDesktop()) {
                effects->windowToDesktops(target, {effects->currentDesktop()});
            }
            m_workspaceMinimizedWindows.remove(target);
            m_stashedWindows.remove(target);
            if (target->isMinimized()) {
                target->setMinimized(false);
            }
        }
        cancelJumpMode(keyToPass);
        if (m_tilingActive) {
            performTilingSwap(target);
        } else {
            effects->activateWindow(target);
        }
        warpCursorToWindow(target);
        return true;
    }

    // Default: m_superJumpCrossWsAction == 0 (Ask / Interactive Action Menu)
    enterCrossWsActionMode(target);
    return true;
}

void ActiveWindowOutlineEffect::enterCloseWindowMode()
{
    if (m_closeWindowModeActive) {
        exitCloseWindowMode();
        return;
    }
    if (m_tilingPresetsModeActive) {
        exitTilingPresetsMode();
    }
    if (m_subLeapActive) {
        exitSubLeapMode();
    }
    if (m_searchModeActive) {
        exitSearchMode();
    }
    if (m_pinModeActive) {
        exitPinMode();
    }

    m_closeWindowModeActive = true;
    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::exitCloseWindowMode()
{
    if (!m_closeWindowModeActive) {
        return;
    }
    m_closeWindowModeActive = false;
    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotTriggerCloseWindowMode()
{
    if (!m_jumpMode) {
        m_closeWindowModeActive = true;
        slotTriggerJumpMode();
    } else {
        if (m_closeWindowModeActive) {
            exitCloseWindowMode();
        } else {
            enterCloseWindowMode();
        }
    }
}

void ActiveWindowOutlineEffect::enterPinMode()
{
    if (m_pinModeActive) {
        exitPinMode();
        return;
    }
    if (m_tilingPresetsModeActive) {
        exitTilingPresetsMode();
    }
    if (m_subLeapActive) {
        exitSubLeapMode();
    }
    if (m_searchModeActive) {
        exitSearchMode();
    }
    if (m_closeWindowModeActive) {
        exitCloseWindowMode();
    }

    m_pinModeActive = true;
    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::exitPinMode()
{
    if (!m_pinModeActive) {
        return;
    }
    if (m_pinWsSelectionActive) {
        exitPinWsSelectionMode();
    }
    m_pinModeActive = false;
    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotTriggerPinMode()
{
    if (!m_jumpMode) {
        m_pinModeActive = true;
        slotTriggerJumpMode();
    } else {
        if (m_pinModeActive) {
            exitPinMode();
        } else {
            enterPinMode();
        }
    }
}

void ActiveWindowOutlineEffect::enterPinWsSelectionMode(EffectWindow *target)
{
    if (!target || target->isDeleted()) {
        return;
    }
    m_pinWsSelectionActive = true;
    m_pinTargetWindow = target;
    m_pinWsOverlayTexture.reset();
    m_pinWsOverlayCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    playThemedSound(OutlineSound::SoundEffect::NavTab);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::exitPinWsSelectionMode()
{
    if (!m_pinWsSelectionActive) {
        return;
    }
    m_pinWsSelectionActive = false;
    m_pinTargetWindow = nullptr;
    m_pinWsOverlayTexture.reset();
    m_pinWsOverlayCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_pinWsDialogScreenRect = QRectF();
    m_pinWsCancelScreenRect = QRectF();
    m_pinWsRowScreenRects.clear();
    m_pinWsRelRowRects.clear();
    m_pinWsRelCancelRect = QRectF();
    playThemedSound(OutlineSound::SoundEffect::Dismiss);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::toggleHudThumbnailZoom()
{
    m_hudThumbnailZoomActive = !m_hudThumbnailZoomActive;
    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudMetaOverlayTexture.reset();
    m_hudMetaOverlayCacheKey = -1;
    playThemedSound(m_hudThumbnailZoomActive ? OutlineSound::SoundEffect::NavTab : OutlineSound::SoundEffect::Dismiss);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotToggleThumbnailZoom()
{
    if (!m_jumpMode) {
        m_hudThumbnailZoomActive = true;
        slotTriggerJumpMode();
    } else {
        toggleHudThumbnailZoom();
    }
}

void ActiveWindowOutlineEffect::toggleNerdStats()
{
    m_nerdStatsActive = !m_nerdStatsActive;
    if (m_nerdStatsTimer) {
        if (m_jumpMode && m_nerdStatsActive) {
            if (!m_nerdStatsTimer->isActive()) {
                m_nerdStatsTimer->start(500);
            }
        } else {
            if (m_nerdStatsTimer->isActive()) {
                m_nerdStatsTimer->stop();
            }
        }
    }
    m_hudMetaOverlayTexture.reset();
    m_hudMetaOverlayCacheKey = -1;
    m_nerdStatsOverlayTexture.reset();
    m_nerdStatsCacheKey = -1;
    playThemedSound(m_nerdStatsActive ? OutlineSound::SoundEffect::NavTab : OutlineSound::SoundEffect::Dismiss);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotTriggerNerdStats()
{
    if (!m_jumpMode) {
        m_nerdStatsActive = true;
        slotTriggerJumpMode();
    } else {
        toggleNerdStats();
    }
}

void ActiveWindowOutlineEffect::slotTriggerAudioFilterMode()
{
    if (!m_jumpMode) {
        m_audioFilterModeActive = true;
        slotTriggerJumpMode();
    } else {
        toggleAudioFilterMode();
    }
}

void ActiveWindowOutlineEffect::toggleAudioFilterMode()
{
    if (m_tilingPresetsModeActive) {
        exitTilingPresetsMode();
    }
    if (m_subLeapActive) {
        exitSubLeapMode();
    }
    if (m_stashModeActive) {
        exitStashMode();
    }
    if (m_superJumpActive) {
        exitSuperJumpMode();
    }

    m_audioFilterModeActive = !m_audioFilterModeActive;
    m_audioTracker.refresh(getKnownWindowsInfo());
    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::exitAudioFilterMode()
{
    if (!m_audioFilterModeActive) {
        return;
    }
    m_audioFilterModeActive = false;
    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotTriggerSearchMode()
{
    if (!m_jumpMode) {
        m_searchModeActive = true;
        slotTriggerJumpMode();
    } else {
        toggleSearchMode();
    }
}

void ActiveWindowOutlineEffect::slotTriggerTilingPresetsMode()
{
    if (m_jumpMode && m_tilingPresetsModeActive) {
        cancelJumpMode();
        return;
    }
    if (!m_jumpMode) {
        slotTriggerJumpMode();
    }
    enterTilingPresetsMode();
}

void ActiveWindowOutlineEffect::enterSearchMode()
{
    if (m_tilingPresetsModeActive) {
        exitTilingPresetsMode();
    }
    if (m_subLeapActive) {
        exitSubLeapMode();
    }
    if (m_stashModeActive) {
        exitStashMode();
    }
    if (m_superJumpActive) {
        exitSuperJumpMode();
    }
    if (m_audioFilterModeActive) {
        exitAudioFilterMode();
    }

    m_searchModeActive = true;
    m_searchQuery.clear();
    m_searchSelectedIndex = 0;
    updateSearchMatches();
}

void ActiveWindowOutlineEffect::exitSearchMode()
{
    if (!m_searchModeActive) {
        return;
    }
    m_searchModeActive = false;
    m_searchQuery.clear();
    m_searchMatches.clear();
    m_searchSelectedIndex = 0;
    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::toggleSearchMode()
{
    if (m_searchModeActive) {
        exitSearchMode();
    } else {
        enterSearchMode();
    }
}

void ActiveWindowOutlineEffect::updateSearchMatches()
{
    if (!effects) {
        return;
    }

    QList<OutlineFuzzy::SearchCandidate> candidates;
    const auto windows = effects->stackingOrder();

    for (EffectWindow *w : windows) {
        if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
            continue;
        }

        const bool isStashed = isWindowInStash(w);
        const int wsIndex = findWorkspaceForWindow(w);
        const bool onCurrent = w->isOnCurrentDesktop();

        if (m_searchScope == 0) { // Current Workspace
            if (isStashed || !onCurrent) {
                continue;
            }
        } else if (m_searchScope == 1) { // All Workspaces
            if (isStashed) {
                continue;
            }
        } else { // 2: All + Stash
            // Include everything
        }

        OutlineFuzzy::SearchCandidate cand;
        cand.windowPtr = w;
        cand.caption = w->caption();
        cand.appName = w->windowClass();
        cand.customLabel = m_markedWindowLabels.value(w);
        cand.isRecent = (w == findRecentWindow()) || m_recentWindows.contains(w);
        cand.isPinned = isWindowPinned(w);
        cand.isPlayingAudio = m_audioTracker.isWindowPlaying(w->pid(), w->caption());
        cand.workspaceIndex = wsIndex;
        cand.isStashed = isStashed;

        candidates.append(cand);
    }

    OutlineFuzzy::rankSearchCandidates(m_searchQuery, candidates);
    m_searchMatches = candidates;
    m_searchSelectedIndex = qBound(0, m_searchSelectedIndex, qMax(0, m_searchMatches.size() - 1));

    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::setKdeGlobalShortcutsBlocked(bool blocked)
{
    if (m_globalShortcutsBlocked == blocked) {
        return;
    }
    m_globalShortcutsBlocked = blocked;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/kglobalaccel"),
        QStringLiteral("org.kde.KGlobalAccel"),
        QStringLiteral("blockGlobalShortcuts")
    );
    msg << blocked;
    QDBusConnection::sessionBus().send(msg);
}

void ActiveWindowOutlineEffect::slotCycleHudView(int delta)
{
    playThemedSound(OutlineSound::SoundEffect::NavTab);
    refreshWorkspaces();
    const int totalViews = LeapKWorkspace::computeTotalWorkspaceViews(m_workspaces.size());
    int targetView = (m_activeHudView + delta) % totalViews;
    if (targetView < 0) {
        targetView += totalViews;
    }

    if (m_subLeapActive) {
        exitSubLeapMode();
    }

    switchToWorkspaceView(targetView);

    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }

    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::slotCycleBadge(int delta)
{
    if (m_searchModeActive) {
        if (m_searchMatches.isEmpty()) {
            return;
        }
        m_searchSelectedIndex = (m_searchSelectedIndex + delta + m_searchMatches.size()) % m_searchMatches.size();
        m_badgeTextureCache.clear();
        if (effects) {
            effects->addRepaintFull();
        }
        return;
    }

    const auto &letterMap = m_subLeapActive ? m_subLeapLetterToWindow : m_jumpLetterToWindow;
    if (letterMap.isEmpty() && m_hudTargetPillRects.isEmpty()) {
        return;
    }

    QList<EffectWindow *> orderedWindows;
    auto addWindow = [&orderedWindows](EffectWindow *w) {
        if (w && !w->isDeleted() && !orderedWindows.contains(w)) {
            orderedWindows.append(w);
        }
    };

    // 1. Windows from HUD dock pills (if present)
    for (const auto &pair : m_hudTargetPillRects) {
        addWindow(pair.second);
    }

    // 2. Windows from letterMap (including on-screen badges)
    for (auto it = letterMap.constBegin(); it != letterMap.constEnd(); ++it) {
        addWindow(it.value());
    }

    // 3. Multi-instance window groups (Mode A)
    for (auto it = m_letterWindowGroups.constBegin(); it != m_letterWindowGroups.constEnd(); ++it) {
        for (EffectWindow *w : it.value()) {
            addWindow(w);
        }
    }

    // 4. Origin active window
    if (m_hudOriginWindow && isEligibleTargetWindow(m_hudOriginWindow.data())) {
        addWindow(m_hudOriginWindow.data());
    }

    if (orderedWindows.isEmpty()) {
        return;
    }

    // Sort orderedWindows consistently with HUD pills:
    // Pinned/stashed windows first, then regular windows.
    // Within each partition, sort by recency rank oldest to newest (rankA > rankB).
    const auto &history = !m_hudSnapshotHistory.isEmpty() ? m_hudSnapshotHistory : m_windowHistory;
    auto recencyRank = [&history](EffectWindow *w) -> int {
        if (!w) return 999999;
        int idx = history.indexOf(w);
        return idx >= 0 ? idx : 999999;
    };

    QList<EffectWindow *> leftGroup;
    QList<EffectWindow *> rightGroup;
    if (m_stashModeActive) {
        for (EffectWindow *w : orderedWindows) {
            if (isWindowInStash(w)) {
                rightGroup.append(w);
            } else {
                leftGroup.append(w);
            }
        }
    } else {
        for (EffectWindow *w : orderedWindows) {
            if (isWindowPinned(w)) {
                leftGroup.append(w);
            } else {
                rightGroup.append(w);
            }
        }
    }

    auto sortByRecencyOldestToNewest = [&recencyRank](QList<EffectWindow *> &list) {
        std::stable_sort(list.begin(), list.end(), [&recencyRank](EffectWindow *a, EffectWindow *b) {
            int rankA = recencyRank(a);
            int rankB = recencyRank(b);
            if (rankA != rankB) {
                return rankA > rankB; // Older window (higher index) on the left
            }
            return false;
        });
    };
    sortByRecencyOldestToNewest(leftGroup);
    sortByRecencyOldestToNewest(rightGroup);
    orderedWindows = leftGroup + rightGroup;

    if (orderedWindows.isEmpty()) {
        return;
    }

    EffectWindow *curr = effects ? effects->activeWindow() : m_activeWindow;
    int currIdx = orderedWindows.indexOf(curr);
    if (currIdx < 0 && m_hudOriginWindow) {
        currIdx = orderedWindows.indexOf(m_hudOriginWindow.data());
    }
    if (currIdx < 0 && m_activeWindow) {
        currIdx = orderedWindows.indexOf(m_activeWindow);
    }

    int nextIdx = 0;
    if (currIdx >= 0) {
        nextIdx = (currIdx + delta + orderedWindows.size()) % orderedWindows.size();
    } else {
        nextIdx = (delta > 0) ? 0 : (orderedWindows.size() - 1);
    }

    EffectWindow *target = orderedWindows.at(nextIdx);
    if (!target || target->isDeleted()) {
        return;
    }

    playThemedSound(OutlineSound::SoundEffect::NavTab);

    if (!target->isOnCurrentDesktop()) {
        const auto desktops = target->desktops();
        if (!desktops.isEmpty() && !target->isOnAllDesktops()) {
            effects->setCurrentDesktop(desktops.first());
        }
    }
    if (target->isMinimized()) {
        target->setMinimized(false);
    }

    if (target != m_activeWindow) {
        m_hudPrevActiveWindow = m_activeWindow;
        m_hudPrevMarkerRect = m_hudLastDrawnMarkerRect.isValid() ? m_hudLastDrawnMarkerRect : m_hudCurrentMarkerRect;
        m_hudPrevActiveDockIndex = m_hudCurrentActiveDockIndex;
        m_hudSelectAnimTimer.restart();
        m_hudSelectAnimActive = true;
        updateAnimationTimer();
    }

    m_activeWindow = target;
    if (effects) {
        effects->activateWindow(target);
        effects->addRepaintFull();
    }
    warpCursorToWindow(target);

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
}

void ActiveWindowOutlineEffect::slotTriggerJumpMode()
{
    if (m_releaseKeyToConsume != 0) {
        finishKeyboardGrab();
    }

    if (m_badgePreviewActive) {
        clearBadgePreview();
    }

    if (effects && effects->isScreenLocked()) {
        return;
    }
    if (!m_outlineEnabled || m_badgeDisplayMode == 2) {
        return;
    }

    if (m_jumpMode) {
        if (m_subLeapActive) {
            exitSubLeapMode();
            return;
        }
        if (m_tilingPresetsModeActive) {
            exitTilingPresetsMode();
            return;
        }
        if (m_stashModeActive) {
            exitStashMode();
            return;
        }
        if (m_closeWindowModeActive) {
            exitCloseWindowMode();
            return;
        }
        if (m_pinWsSelectionActive) {
            exitPinWsSelectionMode();
            return;
        }
        if (m_crossWsActionActive) {
            exitCrossWsActionMode();
            return;
        }
        if (m_pinModeActive) {
            exitPinMode();
            return;
        }
        if (m_superJumpActive) {
            exitSuperJumpMode();
            return;
        }
        const bool metaHeld = (QGuiApplication::queryKeyboardModifiers() & Qt::MetaModifier) != 0;
        if (metaHeld || m_hudMetaHeld) {
            const bool isShift = (QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier) != 0;
            traverseRecentWindows(isShift ? -1 : 1);
            return;
        }
        cancelJumpMode(Qt::Key_Space);
        return;
    }

    m_subLeapActive = false;
    m_stashModeActive = false;
    m_superJumpActive = false;
    m_crossWsActionActive = false;
    m_crossWsTargetWindow = nullptr;
    m_crossWsActionOverlayTexture.reset();
    m_crossWsActionOverlayCacheKey = -1;
    m_pinWsSelectionActive = false;
    m_pinTargetWindow = nullptr;
    m_pinWsOverlayTexture.reset();
    m_pinWsOverlayCacheKey = -1;
    m_pinWsDialogScreenRect = QRectF();
    m_pinWsCancelScreenRect = QRectF();
    m_pinWsRowScreenRects.clear();
    m_pinWsRelRowRects.clear();
    m_pinWsRelCancelRect = QRectF();
    m_hudThumbnailZoomActive = false;
    m_nerdStatsActive = false;
    if (m_nerdStatsTimer && m_nerdStatsTimer->isActive()) {
        m_nerdStatsTimer->stop();
    }
    m_nerdStatsOverlayTexture.reset();
    m_nerdStatsCacheKey = -1;
    m_nerdStatsScreenRect = QRectF();
    m_closeWindowModeActive = false;
    m_pinModeActive = false;
    m_metaSpaceTraverseActive = false;
    m_hudMetaHeld = (QGuiApplication::queryKeyboardModifiers() & Qt::MetaModifier) != 0;
    m_hudMetaOverlayTexture.reset();
    m_hudMetaOverlayCacheKey = -1;
    m_tilingPresetsModeActive = false;
    m_hudTilingPresetPillRects.clear();
    m_subLeapBaseKey = QChar();
    m_subLeapWindows.clear();
    m_subLeapLetterToWindow.clear();
    m_subLeapTargets.clear();
    m_closingWindows.clear();

    if (effects) {
        m_activeWindow = effects->activeWindow();
    }
    m_audioTracker.refresh(getKnownWindowsInfo());
    refreshPersistentMarksForWindows();
    refreshWorkspaces();

    if (m_searchModeActive) {
        m_searchQuery.clear();
        m_searchSelectedIndex = 0;
        updateSearchMatches();
    } else {
        m_jumpLetterToWindow = assignJumpLetters();
    }

    if (!effects->grabKeyboard(this)) {
        m_jumpLetterToWindow.clear();
    m_keyboardOverlayWindowMap.clear();
        return;
    }
    m_keyboardGrabActive = true;

    m_jumpMode = true;
    m_hudOriginWindow = effects ? effects->activeWindow() : m_activeWindow;
    m_hudSnapshotHistory = m_windowHistory;
#ifdef ACTIVE_WINDOW_OUTLINE_HAS_WINDOW_INPUT_MOUSE_EVENT
    effects->startMouseInterception(this, Qt::ArrowCursor);
    m_mouseIntercepting = true;
#endif
    if (m_soundFeedbackEnabled) {
        playThemedSound(OutlineSound::SoundEffect::EnterJump);
    }
    if (m_audioRefreshTimer) {
        m_audioRefreshTimer->start(1500);
    }
    setKdeGlobalShortcutsBlocked(true);
    updateAnimationTimer();
    m_badgeTextureCache.clear();
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();
    if (m_jumpTimeoutTimer) {
        if (m_badgeTimeout > 0) {
            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
        } else {
            m_jumpTimeoutTimer->stop();
        }
    }
    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::finishKeyboardGrab()
{
    if (m_keyReleaseSafetyTimer && m_keyReleaseSafetyTimer->isActive()) {
        m_keyReleaseSafetyTimer->stop();
    }
    m_releaseKeyToConsume = 0;
    if (m_isHistoryNavigating && !m_jumpMode) {
        slotCommitHistoryNavigation();
    }
    if (m_keyboardGrabActive) {
        effects->ungrabKeyboard();
        m_keyboardGrabActive = false;
    }
}

void ActiveWindowOutlineEffect::cancelJumpMode()
{
    cancelJumpMode(0);
}

void ActiveWindowOutlineEffect::cancelJumpMode(int keyToRelease)
{
    if (!m_jumpMode) {
        return;
    }
    m_jumpMode = false;
    m_hudSnapshotHistory.clear();
    EffectWindow *curr = (m_activeWindow && !m_activeWindow->isDeleted() && isEligibleOutlineWindow(m_activeWindow))
        ? m_activeWindow
        : (effects ? effects->activeWindow() : nullptr);
    if (curr && isEligibleOutlineWindow(curr)) {
        m_windowHistory.removeAll(curr);
        m_windowHistory.prepend(curr);
        m_activeWindow = curr;
        updateRecentWindowsFromHistory();
    }
    playThemedSound(OutlineSound::SoundEffect::Dismiss);
#ifdef ACTIVE_WINDOW_OUTLINE_HAS_WINDOW_INPUT_MOUSE_EVENT
    if (m_mouseIntercepting) {
        effects->stopMouseInterception(this);
        m_mouseIntercepting = false;
    }
#endif
    m_hudCloseButtonLocalRect = QRectF();
    m_hudCloseButtonScreenRect = QRectF();
    m_hudHeaderScreenRect = QRectF();
    m_hudHintBarScreenRect = QRectF();
    m_hudTabLocalRects.clear();
    m_hudTabScreenRects.clear();
    m_hudTargetPillRects.clear();
    m_hudOriginWindow.clear();
    m_hudSelectAnimActive = false;
    m_hudPrevActiveWindow.clear();
    m_hudPrevMarkerRect = QRectF();
    m_hudCurrentMarkerRect = QRectF();
    m_hudLastDrawnMarkerRect = QRectF();
    m_hudCurrentActiveDockIndex = -1;
    m_hudPrevActiveDockIndex = -1;
    setKdeGlobalShortcutsBlocked(false);
    m_subLeapActive = false;
    m_stashModeActive = false;
    m_superJumpActive = false;
    m_crossWsActionActive = false;
    m_crossWsTargetWindow = nullptr;
    m_crossWsActionOverlayTexture.reset();
    m_crossWsActionOverlayCacheKey = -1;
    m_pinWsSelectionActive = false;
    m_pinTargetWindow = nullptr;
    m_pinWsOverlayTexture.reset();
    m_pinWsOverlayCacheKey = -1;
    m_pinWsDialogScreenRect = QRectF();
    m_pinWsCancelScreenRect = QRectF();
    m_pinWsRowScreenRects.clear();
    m_pinWsRelRowRects.clear();
    m_pinWsRelCancelRect = QRectF();
    m_hudThumbnailZoomActive = false;
    m_nerdStatsActive = false;
    if (m_nerdStatsTimer && m_nerdStatsTimer->isActive()) {
        m_nerdStatsTimer->stop();
    }
    m_nerdStatsOverlayTexture.reset();
    m_nerdStatsCacheKey = -1;
    m_nerdStatsScreenRect = QRectF();
    m_closeWindowModeActive = false;
    m_pinModeActive = false;
    m_metaSpaceTraverseActive = false;
    m_hudMetaHeld = false;
    m_hudMetaOverlayTexture.reset();
    m_hudMetaOverlayCacheKey = -1;
    m_tilingPresetsModeActive = false;
    m_hudTilingPresetPillRects.clear();
    m_audioFilterModeActive = false;
    m_searchModeActive = false;
    m_searchQuery.clear();
    m_searchMatches.clear();
    m_searchSelectedIndex = 0;
    const bool badgesAlwaysOn = (m_outlineEnabled && m_badgeConfig.windowBadgesEnabled && m_badgeDisplayMode == 1);
    if (m_audioRefreshTimer && !badgesAlwaysOn) {
        m_audioRefreshTimer->stop();
    }
    m_subLeapBaseKey = QChar();
    m_subLeapWindows.clear();
    m_subLeapLetterToWindow.clear();
    m_subLeapTargets.clear();
    m_closingWindows.clear();
    m_letterWindowGroups.clear();
    if (m_jumpTimeoutTimer && m_jumpTimeoutTimer->isActive()) {
        m_jumpTimeoutTimer->stop();
    }
    m_jumpLetterToWindow.clear();
    m_keyboardOverlayWindowMap.clear();
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;

    if (keyToRelease != 0 && m_keyboardGrabActive) {
        m_releaseKeyToConsume = keyToRelease;
        if (!m_keyReleaseSafetyTimer) {
            m_keyReleaseSafetyTimer = new QTimer(this);
            m_keyReleaseSafetyTimer->setSingleShot(true);
            connect(m_keyReleaseSafetyTimer, &QTimer::timeout, this, &ActiveWindowOutlineEffect::finishKeyboardGrab);
        }
        m_keyReleaseSafetyTimer->start(180);
    } else {
        finishKeyboardGrab();
    }

    updateAnimationTimer();
    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::activateJumpTargetWindow(EffectWindow *target, int keyToPass)
{
    if (!target || target->isDeleted()) {
        cancelJumpMode(keyToPass);
        return;
    }

    cancelJumpMode(keyToPass);
    m_expectedJumpTarget = target;
    if (m_expectedJumpTargetTimer) {
        m_expectedJumpTargetTimer->start(1000);
    }

    const int targetWs = findWorkspaceForWindow(target);
    if (m_activeHudView != targetWs && m_activeHudView <= m_workspaces.size()) {
        switchToWorkspaceView(targetWs);
    }

    if (!target->isOnCurrentDesktop()) {
        const auto desktops = target->desktops();
        if (!desktops.isEmpty() && !target->isOnAllDesktops()) {
            effects->setCurrentDesktop(desktops.first());
        }
    }
    m_workspaceMinimizedWindows.remove(target);
    m_stashedWindows.remove(target);
    if (target->isMinimized()) {
        target->setMinimized(false);
    }
    if (m_tilingActive) {
        performTilingSwap(target);
    } else {
        effects->activateWindow(target);
    }
    warpCursorToWindow(target);
}

#ifdef ACTIVE_WINDOW_OUTLINE_HAS_WINDOW_INPUT_MOUSE_EVENT
void ActiveWindowOutlineEffect::windowInputMouseEvent(QEvent *e)
{
    if (m_badgePreviewActive && !m_jumpMode) {
        if (e->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (!isPosInsideBadgePreview(me->position())) {
                if (m_previewMouseIntercepting) {
                    effects->stopMouseInterception(this);
                    m_previewMouseIntercepting = false;
                    effects->defineCursor(Qt::ArrowCursor);
                }
            } else {
                effects->defineCursor(Qt::PointingHandCursor);
            }
            return;
        }
        if (e->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (isPosInsideBadgePreview(me->position())) {
                playThemedSound(OutlineSound::SoundEffect::Dismiss);
                clearBadgePreview();
                return;
            } else {
                if (m_previewMouseIntercepting) {
                    effects->stopMouseInterception(this);
                    m_previewMouseIntercepting = false;
                    effects->defineCursor(Qt::ArrowCursor);
                }
            }
        }
        return;
    }

    if (!m_jumpMode) {
        return;
    }

    if (m_crossWsActionActive) {
        if (e->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(e);
            const QPointF pos = me->position();
            if (m_crossWsJumpScreenRect.contains(pos) ||
                m_crossWsMoveScreenRect.contains(pos) ||
                m_crossWsPinScreenRect.contains(pos) ||
                m_crossWsCancelScreenRect.contains(pos)) {
                effects->defineCursor(Qt::PointingHandCursor);
            } else {
                effects->defineCursor(Qt::ArrowCursor);
            }
            return;
        }
        if (e->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton) {
                const QPointF pos = me->position();
                if (m_crossWsJumpScreenRect.contains(pos)) {
                    executeCrossWsAction(1);
                    return;
                }
                if (m_crossWsMoveScreenRect.contains(pos)) {
                    executeCrossWsAction(2);
                    return;
                }
                if (m_crossWsPinScreenRect.contains(pos)) {
                    executeCrossWsAction(3);
                    return;
                }
                if (m_crossWsCancelScreenRect.contains(pos) || !m_crossWsDialogScreenRect.contains(pos)) {
                    exitCrossWsActionMode();
                    return;
                }
                return;
            } else if (me->button() == Qt::RightButton) {
                exitCrossWsActionMode();
                return;
            }
        }
        return;
    }

    if (m_pinWsSelectionActive) {
        if (e->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(e);
            const QPointF pos = me->position();
            bool isHover = m_pinWsCancelScreenRect.contains(pos);
            if (!isHover) {
                for (const auto &r : m_pinWsRowScreenRects) {
                    if (r.contains(pos)) {
                        isHover = true;
                        break;
                    }
                }
            }
            effects->defineCursor(isHover ? Qt::PointingHandCursor : Qt::ArrowCursor);
            return;
        }
        if (e->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton) {
                const QPointF pos = me->position();
                if (m_pinWsCancelScreenRect.contains(pos) || !m_pinWsDialogScreenRect.contains(pos)) {
                    exitPinWsSelectionMode();
                    return;
                }
                if (!m_pinWsRowScreenRects.isEmpty() && m_pinWsRowScreenRects[0].contains(pos)) {
                    // Row 0: All Workspaces
                    if (m_pinTargetWindow && !m_pinTargetWindow->isDeleted()) {
                        pinWindow(m_pinTargetWindow.data());
                    }
                    exitPinWsSelectionMode();
                    return;
                }
                const int wsCount = m_workspaces.size();
                for (int i = 0; i < wsCount; ++i) {
                    const int rowIdx = i + 1;
                    if (rowIdx < m_pinWsRowScreenRects.size() && m_pinWsRowScreenRects[rowIdx].contains(pos)) {
                        if (m_pinTargetWindow && !m_pinTargetWindow->isDeleted()) {
                            toggleWindowPinWorkspace(m_pinTargetWindow.data(), i + 1);
                        }
                        exitPinWsSelectionMode();
                        return;
                    }
                }
                const int unpinIdx = wsCount + 1;
                if (unpinIdx < m_pinWsRowScreenRects.size() && m_pinWsRowScreenRects[unpinIdx].contains(pos)) {
                    if (m_pinTargetWindow && !m_pinTargetWindow->isDeleted()) {
                        unpinWindow(m_pinTargetWindow.data(), m_activeHudView);
                    }
                    exitPinWsSelectionMode();
                    return;
                }
                return;
            } else if (me->button() == Qt::RightButton) {
                exitPinWsSelectionMode();
                return;
            }
        }
        return;
    }

    if (e->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent *>(e);
        const QPointF pos = me->position();
        bool isHoveringClickable = false;

        if (m_hudCloseButtonScreenRect.isValid() && m_hudCloseButtonScreenRect.contains(pos)) {
            isHoveringClickable = true;
        }
        if (!isHoveringClickable && m_nerdStatsActive && m_nerdStatsScreenRect.isValid() && m_nerdStatsScreenRect.contains(pos)) {
            isHoveringClickable = true;
        }
        if (!isHoveringClickable && isKeyboardOverlayActive()) {
            for (const auto &kg : m_keyboardOverlayLayout.keycaps) {
                if (kg.rect.contains(pos) && m_keyboardOverlayWindowMap.contains(kg.key)) {
                    isHoveringClickable = true;
                    break;
                }
            }
        }
        if (!isHoveringClickable && m_tilingPresetsModeActive) {
            for (const auto &pair : m_hudTilingPresetPillRects) {
                if (pair.first.isValid() && pair.first.contains(pos)) {
                    isHoveringClickable = true;
                    break;
                }
            }
        }
        if (!isHoveringClickable) {
            for (const auto &tr : m_hudTabScreenRects) {
                if (tr.isValid() && tr.contains(pos)) {
                    isHoveringClickable = true;
                    break;
                }
            }
            if (!isHoveringClickable) {
                for (const auto &pair : m_hudTargetPillRects) {
                    if (pair.first.isValid() && pair.first.contains(pos)) {
                        isHoveringClickable = true;
                        break;
                    }
                }
            }
        }

        if (isHoveringClickable) {
            effects->defineCursor(Qt::PointingHandCursor);
        } else {
            effects->defineCursor(Qt::ArrowCursor);
        }
        return;
    }

    if (e->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::LeftButton) {
            const QPointF pos = me->position();

            // 1. Click on Close button
            if (m_hudCloseButtonScreenRect.isValid() && m_hudCloseButtonScreenRect.contains(pos)) {
                cancelJumpMode();
                return;
            }

            // Click on Nerd Stats card to toggle off
            if (m_nerdStatsActive && m_nerdStatsScreenRect.isValid() && m_nerdStatsScreenRect.contains(pos)) {
                toggleNerdStats();
                return;
            }

            // Keyboard overlay key click
            if (isKeyboardOverlayActive()) {
                for (const auto &kg : m_keyboardOverlayLayout.keycaps) {
                    if (kg.rect.contains(pos)) {
                        if (m_keyboardOverlayWindowMap.contains(kg.key)) {
                            EffectWindow *target = m_keyboardOverlayWindowMap.value(kg.key);
                            if (target && !target->isDeleted()) {
                                if (m_pinModeActive) {
                                    enterPinWsSelectionMode(target);
                                    return;
                                }
                                if (handleSuperJumpCrossWorkspaceSelection(target, Qt::Key_Space)) {
                                    return;
                                }
                                activateJumpTargetWindow(target, Qt::Key_Space);
                                return;
                            }
                        }
                    }
                }
            }

            // If in tiling presets mode, check clicks on preset pills:
            if (m_tilingPresetsModeActive) {
                for (const auto &pair : m_hudTilingPresetPillRects) {
                    if (pair.first.isValid() && pair.first.contains(pos)) {
                        activateTilingPreset(pair.second);
                        return;
                    }
                }
            }

            // 2. Click on a window pill
            for (const auto &pair : m_hudTargetPillRects) {
                if (pair.first.isValid() && pair.first.contains(pos) && pair.second) {
                    EffectWindow *target = pair.second;
                    if (m_pinModeActive) {
                        enterPinWsSelectionMode(target);
                        return;
                    }
                    if (handleSuperJumpCrossWorkspaceSelection(target)) {
                        return;
                    }
                    cancelJumpMode();
                    const int targetWs = findWorkspaceForWindow(target);
                    if (m_activeHudView != targetWs && m_activeHudView <= m_workspaces.size()) {
                        switchToWorkspaceView(targetWs);
                    }
                    if (!target->isOnCurrentDesktop()) {
                        const auto desktops = target->desktops();
                        if (!desktops.isEmpty() && !target->isOnAllDesktops()) {
                            effects->setCurrentDesktop(desktops.first());
                        }
                    }
                    m_workspaceMinimizedWindows.remove(target);
                    m_stashedWindows.remove(target);
                    if (target->isMinimized()) {
                        target->setMinimized(false);
                    }
                    if (m_tilingActive) {
                        performTilingSwap(target);
                    } else {
                        effects->activateWindow(target);
                    }
                    warpCursorToWindow(target);
                    return;
                }
            }

            // 3. Click inside header on a workspace tab
            if (m_hudHeaderScreenRect.isValid() && m_hudHeaderScreenRect.contains(pos)) {
                for (int i = 0; i < m_hudTabScreenRects.size(); ++i) {
                    if (m_hudTabScreenRects[i].contains(pos)) {
                        playThemedSound(OutlineSound::SoundEffect::NavTab);
                        switchToWorkspaceView(i);
                        m_jumpLetterToWindow = assignJumpLetters();
                        m_badgeTextureCache.clear();
                        m_hudHeaderTexture.reset();
                        m_hudHeaderCacheKey = -1;
                        m_hudHintBarTexture.reset();
                        m_hudHintBarCacheKey = -1;
                        if (effects) {
                            effects->addRepaintFull();
                        }
                        return;
                    }
                }
                return;
            }

            // 4. Click outside on backdrop / empty space -> close HUD
            cancelJumpMode();
            return;
        } else if (me->button() == Qt::RightButton) {
            cancelJumpMode();
            return;
        }
    }
}
#endif

void ActiveWindowOutlineEffect::grabbedKeyboardEvent(QKeyEvent *e)
{
    if (m_releaseKeyToConsume != 0) {
        if (e->type() == QEvent::KeyRelease) {
            if (m_releaseKeyToConsume == -1 || e->key() == m_releaseKeyToConsume || e->key() == Qt::Key_Space) {
                finishKeyboardGrab();
                return;
            }
            return;
        } else if (e->type() == QEvent::KeyPress) {
            finishKeyboardGrab();
            return;
        }
        return;
    }

    auto isMetaKey = [](int key) {
        return key == Qt::Key_Meta || key == Qt::Key_Super_L || key == Qt::Key_Super_R;
    };

    if (e->type() == QEvent::KeyRelease) {
        if (isMetaKey(e->key()) || !(e->modifiers() & Qt::MetaModifier)) {
            if (m_metaSpaceTraverseActive) {
                m_metaSpaceTraverseActive = false;
                slotCommitHistoryNavigation();
                warpCursorToWindow(m_activeWindow);
                cancelJumpMode();
                if (effects) {
                    effects->addRepaintFull();
                }
                return;
            }
            if (m_hudMetaHeld) {
                m_hudMetaHeld = false;
                if (effects) {
                    effects->addRepaintFull();
                }
            }
        }
        return;
    }

    if (m_crossWsActionActive) {
        if (e->type() != QEvent::KeyPress) {
            return;
        }

        // Jump to Workspace (default)
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_J || e->key() == Qt::Key_1) {
            executeCrossWsAction(1);
            return;
        }

        // Move to Current Workspace
        if (e->key() == Qt::Key_M || e->key() == Qt::Key_2) {
            executeCrossWsAction(2);
            return;
        }

        // Pin / Both Workspaces
        if (e->key() == Qt::Key_P || e->key() == Qt::Key_3) {
            executeCrossWsAction(3);
            return;
        }

        // Return back to Super Jump
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Space) {
            exitCrossWsActionMode();
            return;
        }

        // Any other printable key -> feedback
        const QString text = e->text();
        if (!text.isEmpty()) {
            triggerInvalidKeyFeedback(text.at(0).toUpper());
            return;
        }
        return;
    }

    if (m_pinWsSelectionActive) {
        if (e->type() != QEvent::KeyPress) {
            return;
        }

        // Pin to All Workspaces
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter || e->key() == Qt::Key_A || e->key() == Qt::Key_0 || (!e->text().isEmpty() && (e->text().at(0) == QLatin1Char('A') || e->text().at(0) == QLatin1Char('a') || e->text().at(0) == QLatin1Char('0')))) {
            if (m_pinTargetWindow && !m_pinTargetWindow->isDeleted()) {
                pinWindow(m_pinTargetWindow.data());
            }
            exitPinWsSelectionMode();
            return;
        }

        // Unpin
        if (e->key() == Qt::Key_U || (!e->text().isEmpty() && (e->text().at(0) == QLatin1Char('U') || e->text().at(0) == QLatin1Char('u')))) {
            if (m_pinTargetWindow && !m_pinTargetWindow->isDeleted()) {
                unpinWindow(m_pinTargetWindow.data(), m_activeHudView);
            }
            exitPinWsSelectionMode();
            return;
        }

        // Pin to Workspace by index 1..N
        if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_9) {
            const int wsNum = e->key() - Qt::Key_0;
            if (wsNum >= 1 && wsNum <= m_workspaces.size()) {
                if (m_pinTargetWindow && !m_pinTargetWindow->isDeleted()) {
                    toggleWindowPinWorkspace(m_pinTargetWindow.data(), wsNum);
                }
                exitPinWsSelectionMode();
                return;
            }
        }

        // Cancel / Return back
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Backspace) {
            exitPinWsSelectionMode();
            return;
        }

        // Any other printable key -> feedback
        const QString text = e->text();
        if (!text.isEmpty()) {
            triggerInvalidKeyFeedback(text.at(0).toUpper());
            return;
        }
        return;
    }

    if (m_tilingEditModeActive) {
        if (e->type() != QEvent::KeyPress) {
            return;
        }

        // Sub-mode: Merge slot selection (Ctrl+M was pressed)
        if (m_tilingMergeModeActive) {
            if (e->key() == Qt::Key_Escape) {
                m_tilingMergeModeActive = false;
                playThemedSound(OutlineSound::SoundEffect::Dismiss);
                if (effects) {
                    effects->addRepaintFull();
                }
                return;
            }

            QString keyText = e->text().toUpper();
            QChar pressedChar = keyText.isEmpty() ? QChar(e->key()).toUpper() : keyText.at(0).toUpper();

            int activeSlotIdx = findTileSlotIndexForWindow(m_activeWindow);
            if (activeSlotIdx < 0 && !m_activeTilingSlots.isEmpty()) {
                activeSlotIdx = 0;
            }

            const int numKeys = 14;
            for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
                if (i == activeSlotIdx) continue;
                QChar slotChar = LeapKTiling::kTileEditKeys[i % numKeys];
                if (slotChar == pressedChar) {
                    m_tilingMergeModeActive = false;
                    mergeTileSlots(activeSlotIdx, i);
                    return;
                }
            }
            return;
        }

        const Qt::KeyboardModifiers mods = e->modifiers();
        if (mods & Qt::ControlModifier) {
            if (e->key() == Qt::Key_S) {
                slotSaveCurrentTilingLayout();
                return;
            } else if (e->key() == Qt::Key_V) {
                splitActiveTileSlot(Qt::Vertical);
                return;
            } else if (e->key() == Qt::Key_H) {
                splitActiveTileSlot(Qt::Horizontal);
                return;
            } else if (e->key() == Qt::Key_M) {
                if (m_activeTilingSlots.size() >= 2) {
                    m_tilingMergeModeActive = true;
                    playThemedSound(OutlineSound::SoundEffect::EnterJump);
                    if (effects) {
                        effects->addRepaintFull();
                    }
                }
                return;
            } else if (e->key() == Qt::Key_F) {
                slotToggleActiveWindowFloating();
                return;
            } else if (e->key() == Qt::Key_D) {
                if (m_activeTilingSlots.size() >= 2) {
                    int idx = findTileSlotIndexForWindow(m_activeWindow);
                    if (idx < 0) idx = 0;
                    deleteTileSlot(idx);
                }
                return;
            }
        }

        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter
            || ((mods & Qt::MetaModifier) && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter))) {
            exitTilingEditMode();
            return;
        }

        if (e->key() == Qt::Key_Backspace) {
            if (m_tilingEditMultiplier > 0) {
                m_tilingEditMultiplier /= 10;
                effects->addRepaintFull();
            }
            return;
        }

        if (e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9) {
            int digit = e->key() - Qt::Key_0;
            m_tilingEditMultiplier = qBound(0, m_tilingEditMultiplier * 10 + digit, 50);
            effects->addRepaintFull();
            return;
        }

        bool isResizeKey = false;
        LeapKTiling::ResizeDirection dir = LeapKTiling::ResizeDirection::Left;
        switch (e->key()) {
        case Qt::Key_H:
        case Qt::Key_Left:
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
        case Qt::Key_BracketLeft:
            isResizeKey = true;
            dir = LeapKTiling::ResizeDirection::Left;
            break;
        case Qt::Key_L:
        case Qt::Key_Right:
        case Qt::Key_Plus:
        case Qt::Key_Equal:
        case Qt::Key_BracketRight:
            isResizeKey = true;
            dir = LeapKTiling::ResizeDirection::Right;
            break;
        case Qt::Key_K:
        case Qt::Key_Up:
        case Qt::Key_I:
            isResizeKey = true;
            dir = LeapKTiling::ResizeDirection::Up;
            break;
        case Qt::Key_J:
        case Qt::Key_Down:
            isResizeKey = true;
            dir = LeapKTiling::ResizeDirection::Down;
            // If the active slot has no vertical neighbors (e.g. 2 or 3 columns, or Master in Master & Stack),
            // pressing J in an IJKL mindset (J=Left, L=Right) should naturally shrink/resize Left!
            if (e->key() == Qt::Key_J && m_activeTilingSlots.size() >= 2) {
                int activeSlotIdx = findTileSlotIndexForWindow(m_activeWindow);
                if (activeSlotIdx < 0) activeSlotIdx = 0;
                const QRectF activeGeo = m_activeTilingSlots[activeSlotIdx].geometry;
                bool hasVerticalNeighbor = false;
                for (int s = 0; s < m_activeTilingSlots.size(); ++s) {
                    if (s == activeSlotIdx) continue;
                    const QRectF &other = m_activeTilingSlots[s].geometry;
                    if (qMax(activeGeo.left(), other.left()) < qMin(activeGeo.right(), other.right()) - 2.0) {
                        hasVerticalNeighbor = true;
                        break;
                    }
                }
                if (!hasVerticalNeighbor) {
                    dir = LeapKTiling::ResizeDirection::Left;
                }
            }
            break;
        default:
            break;
        }

        if (isResizeKey) {
            int count = m_tilingEditMultiplier > 0 ? m_tilingEditMultiplier : 1;
            m_tilingEditMultiplier = 0;
            LeapKTiling::TileEdge edge = LeapKTiling::TileEdge::Auto;
            if (e->modifiers() & Qt::ShiftModifier) {
                // Shift inverts the active edge:
                // For width: operates on the left edge (instead of default right)
                // For height: operates on the top edge (instead of default bottom)
                if (dir == LeapKTiling::ResizeDirection::Left || dir == LeapKTiling::ResizeDirection::Right) {
                    edge = LeapKTiling::TileEdge::Left;
                } else {
                    edge = LeapKTiling::TileEdge::Top;
                }
            }
            performTilingResize(dir, count, edge);
            return;
        }

        QString keyText = e->text().toUpper();
        QChar pressedChar = keyText.isEmpty() ? QChar(e->key()).toUpper() : keyText.at(0).toUpper();

        int activeSlotIdx = findTileSlotIndexForWindow(m_activeWindow);
        if (activeSlotIdx < 0 && !m_activeTilingSlots.isEmpty()) {
            activeSlotIdx = 0;
        }

        const int numKeys = 14;
        for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
            if (i == activeSlotIdx) continue;
            QChar slotChar = LeapKTiling::kTileEditKeys[i % numKeys];
            if (slotChar == pressedChar) {
                m_tilingEditMultiplier = 0;
                performTileSlotSwap(activeSlotIdx, i);
                return;
            }
        }

        return;
    }

    if (!m_jumpMode) {
        return;
    }
    if (e->type() != QEvent::KeyPress) {
        return;
    }

    // Reset timeout countdown on EVERY keypress in HUD!
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }

    // Ignore standalone modifier keys so holding Shift, Ctrl, Alt, Meta etc.
    // to perform a modal jump action does NOT dismiss the Jump HUD.
    switch (e->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    case Qt::Key_Menu:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_Help:
    case Qt::Key_Direction_L:
    case Qt::Key_Direction_R:
    case Qt::Key_Mode_switch:
        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
        }
        return;
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
        if (!m_hudMetaHeld) {
            m_hudMetaHeld = true;
            if (effects) {
                effects->addRepaintFull();
            }
        }
        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
        }
        return;
    default:
        break;
    }

    const Qt::KeyboardModifiers mods = e->modifiers();
    if (mods & Qt::MetaModifier) {
        if (!m_hudMetaHeld) {
            m_hudMetaHeld = true;
            if (effects) {
                effects->addRepaintFull();
            }
        }
    }
    const bool isShift = (mods & Qt::ShiftModifier);
    const bool isAlt = (mods & Qt::AltModifier) && !(mods & Qt::ControlModifier);
    const bool isCtrl = (mods & Qt::ControlModifier) && !(mods & Qt::AltModifier);
    const bool isMetaOnly = (mods & Qt::MetaModifier) && !isAlt && !isCtrl && !isShift;

    // Global Close HUD shortcuts: Esc, Ctrl+C, Ctrl+] -> completely dismiss HUD in all modes
    const bool isCloseHudKey = (e->key() == Qt::Key_Escape)
        || (isCtrl && (e->key() == Qt::Key_C || e->key() == Qt::Key_BracketRight));
    if (isCloseHudKey) {
        if (m_metaSpaceTraverseActive) {
            m_metaSpaceTraverseActive = false;
            m_isHistoryNavigating = false;
            m_historyNavIndex = 0;
            if (m_hudOriginWindow && !m_hudOriginWindow->isDeleted()) {
                const int originWs = findWorkspaceForWindow(m_hudOriginWindow.data());
                if (m_activeHudView != originWs && m_activeHudView <= m_workspaces.size()) {
                    switchToWorkspaceView(originWs);
                }
                if (!m_hudOriginWindow->isOnCurrentDesktop()) {
                    const auto desktops = m_hudOriginWindow->desktops();
                    if (!desktops.isEmpty() && !m_hudOriginWindow->isOnAllDesktops()) {
                        effects->setCurrentDesktop(desktops.first());
                    }
                }
                effects->activateWindow(m_hudOriginWindow.data());
                m_activeWindow = m_hudOriginWindow.data();
                warpCursorToWindow(m_hudOriginWindow.data());
            }
        }
        cancelJumpMode(e->key());
        return;
    }

    const bool isBadgePrevKey = (isShift && (e->key() == Qt::Key_Comma || e->key() == Qt::Key_Less || (!e->text().isEmpty() && (e->text().at(0) == QLatin1Char('<') || e->text().at(0) == QLatin1Char(',')))))
        || (!isShift && (e->key() == Qt::Key_Less || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char('<'))));

    const bool isBadgeNextKey = (isShift && (e->key() == Qt::Key_Period || e->key() == Qt::Key_Greater || (!e->text().isEmpty() && (e->text().at(0) == QLatin1Char('>') || e->text().at(0) == QLatin1Char('.')))))
        || (!isShift && (e->key() == Qt::Key_Greater || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char('>'))));

    if (isBadgePrevKey) {
        slotCycleBadge(-1);
        return;
    }
    if (isBadgeNextKey) {
        slotCycleBadge(1);
        return;
    }

    if (isMetaOnly && e->key() == Qt::Key_Z) {
        toggleHudThumbnailZoom();
        return;
    }
    if (isMetaOnly && e->key() == Qt::Key_D) {
        toggleNerdStats();
        return;
    }

    auto extractKeyChar = [e, isShift]() -> QString {
        if (e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z) {
            return QString(QChar('A' + (e->key() - Qt::Key_A)));
        }
        if (e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9) {
            return QString(QChar('0' + (e->key() - Qt::Key_0)));
        }
        if (isShift) {
            switch (e->key()) {
            case Qt::Key_Exclam: return QStringLiteral("1");
            case Qt::Key_At: return QStringLiteral("2");
            case Qt::Key_NumberSign: return QStringLiteral("3");
            case Qt::Key_Dollar: return QStringLiteral("4");
            case Qt::Key_Percent: return QStringLiteral("5");
            case Qt::Key_AsciiCircum: return QStringLiteral("6");
            case Qt::Key_Ampersand: return QStringLiteral("7");
            case Qt::Key_Asterisk: return QStringLiteral("8");
            case Qt::Key_ParenLeft: return QStringLiteral("9");
            case Qt::Key_ParenRight: return QStringLiteral("0");
            default: break;
            }
        }
        return e->text().toUpper().trimmed();
    };

    // 0. If currently in Tiling Presets mode:
    if (m_tilingPresetsModeActive) {
        if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_S) {
            slotSaveCurrentTilingLayout();
            return;
        }

        if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_E) {
            cancelJumpMode();
            slotOpenKdeTileEditor();
            return;
        }

        if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_R) {
            resetTilingPresetsToDefaults();
            return;
        }

        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Space || (isMetaOnly && (e->key() == Qt::Key_T || e->key() == Qt::Key_Space))) {
            exitTilingPresetsMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_W) {
            cancelJumpMode();
            slotOpenWorkspaceManager();
            return;
        }
        if (isMetaOnly && e->key() == Qt::Key_M) {
            cancelJumpMode();
            slotOpenMarkerManager();
            return;
        }

        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            if (!m_tilingPresets.isEmpty()) {
                activateTilingPreset(m_tilingPresets.first());
                return;
            }
        }

        // Shift + <Key>: Delete layout
        if (isShift) {
            const QString text = extractKeyChar();
            if (!text.isEmpty()) {
                const QChar keyChar = text.at(0).toUpper();
                if (keyChar != QLatin1Char('X') && keyChar != QLatin1Char('K')) {
                    deleteTilingPreset(keyChar);
                    return;
                }
            }
        }

        const QString text = extractKeyChar();
        if (!text.isEmpty()) {
            const QChar keyChar = text.at(0).toUpper();

            if (keyChar == QLatin1Char('X')) {
                deactivateTiling();
                return;
            }

            for (const auto &preset : m_tilingPresets) {
                if (preset.triggerKey.toUpper() == keyChar) {
                    activateTilingPreset(preset);
                    return;
                }
            }
        }
        return;
    }

    // 1. If currently in Sub-Leap disambiguation:
    if (m_subLeapActive) {
        if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Space || (isMetaOnly && e->key() == Qt::Key_Space)) {
            exitSubLeapMode();
            return;
        }

        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            if (m_subLeapLetterToWindow.size() == 1) {
                EffectWindow *target = m_subLeapLetterToWindow.first();
                if (target && !target->isDeleted()) {
                    exitSubLeapMode();
                    if (handleSuperJumpCrossWorkspaceSelection(target, e->key())) {
                        return;
                    }
                    activateJumpTargetWindow(target, e->key());
                    return;
                }
            } else if (m_subLeapWindows.size() == 1) {
                EffectWindow *target = m_subLeapWindows.first();
                if (target && !target->isDeleted()) {
                    exitSubLeapMode();
                    if (handleSuperJumpCrossWorkspaceSelection(target, e->key())) {
                        return;
                    }
                    activateJumpTargetWindow(target, e->key());
                    return;
                }
            }
            exitSubLeapMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_A) {
            exitSubLeapMode();
            toggleSuperJumpMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_S) {
            exitSubLeapMode();
            enterStashMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_P) {
            exitSubLeapMode();
            enterPinMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_Q) {
            exitSubLeapMode();
            enterCloseWindowMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_W) {
            cancelJumpMode();
            slotOpenWorkspaceManager();
            return;
        }
        if (isMetaOnly && e->key() == Qt::Key_M) {
            cancelJumpMode();
            slotOpenMarkerManager();
            return;
        }

        const QString text = extractKeyChar();

        if (!text.isEmpty()) {
            const QChar keyChar = text.at(0);
            if (m_subLeapLetterToWindow.contains(keyChar)) {
                EffectWindow *target = m_subLeapLetterToWindow.value(keyChar);
                if (target && !target->isDeleted()) {
                    if (isShift) {
                        // Quick close selected window
                        target->closeWindow();
                        m_closingWindows.insert(target);
                        m_subLeapWindows.removeAll(target);
                        if (m_subLeapWindows.size() <= 1) {
                            exitSubLeapMode();
                            m_jumpLetterToWindow = assignJumpLetters();
                            m_badgeTextureCache.clear();
                            if (m_jumpLetterToWindow.isEmpty()) {
                                cancelJumpMode();
                            } else {
                                if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                                    m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                                }
                                effects->addRepaintFull();
                            }
                        } else {
                            enterSubLeapForGroup(m_subLeapBaseKey, m_subLeapWindows);
                        }
                        return;
                    }

                    if (isCtrl) {
                        // Toggle stash/minimized
                        if (!target->isMinimized()) {
                            m_stashedWindows.insert(target);
                            target->setMinimized(true);
                        } else {
                            m_stashedWindows.remove(target);
                            target->setMinimized(false);
                        }
                        playThemedSound(OutlineSound::SoundEffect::Stash);
                        m_badgeTextureCache.clear();
                        m_hudHeaderTexture.reset();
                        m_hudHeaderCacheKey = -1;
                        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                        }
                        effects->addRepaintFull();
                        return;
                    }

                    if (isAlt) {
                        // Pin / Pull / Drop
                        const int targetWs = (m_superJumpActive && m_superJumpTargetWorkspace >= 1 && m_superJumpTargetWorkspace <= m_workspaces.size())
                                             ? m_superJumpTargetWorkspace
                                             : (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size() ? m_activeHudView : 0);
                        if (m_pinnedWindows.contains(target)) {
                            unpinWindow(target, targetWs);
                        } else if (!m_superJumpActive && findWorkspaceForWindow(target) == targetWs && target->isOnCurrentDesktop()) {
                            pinWindow(target);
                        } else {
                            if (targetWs >= 1 && targetWs <= m_workspaces.size()) {
                                addWindowToWorkspace(target, targetWs);
                                applyWorkspaceViewIsolation(targetWs);
                            } else if (targetWs == 0) {
                                addWindowToWorkspace(target, 0);
                            }
                            if (effects && effects->currentDesktop()) {
                                effects->windowToDesktops(target, {effects->currentDesktop()});
                            }
                            m_workspaceMinimizedWindows.remove(target);
                            m_stashedWindows.remove(target);
                            if (target->isMinimized()) {
                                target->setMinimized(false);
                            }
                        }
                        cancelJumpMode(e->key());
                        effects->activateWindow(target);
                        warpCursorToWindow(target);
                        return;
                    }

                    // Plain letter: normal jump & activate (switches workspace if window is in another workspace)
                    if (handleSuperJumpCrossWorkspaceSelection(target, e->key())) {
                        return;
                    }
                    activateJumpTargetWindow(target, e->key());
                    return;
                }
            }
        }

        if (!text.isEmpty()) {
            const QChar keyChar = text.at(0).toUpper();
            triggerInvalidKeyFeedback(keyChar);
            return;
        }

        cancelJumpMode();
        return;
    }

    // 2. Main Jump Mode:
    if (m_searchModeActive) {
        if (e->key() == Qt::Key_Tab) {
            m_searchScope = (m_searchScope + 1) % 3;
            updateSearchMatches();
            return;
        }

        if (e->key() == Qt::Key_Backtab) {
            m_searchScope = (m_searchScope + 2) % 3;
            updateSearchMatches();
            return;
        }

        if (e->key() == Qt::Key_Up || (isCtrl && e->key() == Qt::Key_P)) {
            m_searchSelectedIndex = qMax(0, m_searchSelectedIndex - 1);
            m_badgeTextureCache.clear();
            if (effects) {
                effects->addRepaintFull();
            }
            return;
        }

        if (e->key() == Qt::Key_Down || (isCtrl && e->key() == Qt::Key_N)) {
            m_searchSelectedIndex = qMin(m_searchMatches.size() - 1, m_searchSelectedIndex + 1);
            m_badgeTextureCache.clear();
            if (effects) {
                effects->addRepaintFull();
            }
            return;
        }

        if (e->key() == Qt::Key_PageUp) {
            m_searchSelectedIndex = qMax(0, m_searchSelectedIndex - 5);
            m_badgeTextureCache.clear();
            if (effects) {
                effects->addRepaintFull();
            }
            return;
        }

        if (e->key() == Qt::Key_PageDown) {
            m_searchSelectedIndex = qMin(m_searchMatches.size() - 1, m_searchSelectedIndex + 5);
            m_badgeTextureCache.clear();
            if (effects) {
                effects->addRepaintFull();
            }
            return;
        }

        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            if (!m_searchMatches.isEmpty() && m_searchSelectedIndex >= 0 && m_searchSelectedIndex < m_searchMatches.size()) {
                EffectWindow *target = reinterpret_cast<EffectWindow *>(m_searchMatches.at(m_searchSelectedIndex).windowPtr);
                if (target && !target->isDeleted()) {
                    activateJumpTargetWindow(target, e->key());
                    return;
                }
            }
            cancelJumpMode(e->key());
            return;
        }

        if (isAlt) {
            const QString altChar = extractKeyChar();
            if (!altChar.isEmpty() && m_jumpLetterToWindow.contains(altChar.at(0))) {
                EffectWindow *target = m_jumpLetterToWindow.value(altChar.at(0));
                if (target && !target->isDeleted()) {
                    activateJumpTargetWindow(target, e->key());
                    return;
                }
            }
        }

        if (e->key() == Qt::Key_Backspace) {
            if (!m_searchQuery.isEmpty()) {
                m_searchQuery.chop(1);
                updateSearchMatches();
            } else {
                exitSearchMode();
            }
            return;
        }

        const QString input = e->text();
        if (!input.isEmpty() && input.at(0).isPrint()) {
            m_searchQuery.append(input);
            updateSearchMatches();
            return;
        }

        return;
    }

    const bool isQuestionMark = (e->key() == Qt::Key_Question)
        || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char('?'));
    if (isQuestionMark) {
        slotToggleHudHints();
        return;
    }

    if (m_audioFilterModeActive) {
        if (e->key() == Qt::Key_Backspace) {
            exitAudioFilterMode();
            return;
        }

        if (isMetaOnly && (e->key() == Qt::Key_U || e->key() == Qt::Key_A)) {
            exitAudioFilterMode();
            return;
        }
    }

    if (m_stashModeActive) {
        if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Space || (isMetaOnly && (e->key() == Qt::Key_S || e->key() == Qt::Key_Space))) {
            exitStashMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_S) {
            exitStashMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_A) {
            toggleSuperJumpMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_P) {
            exitStashMode();
            enterPinMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_Q) {
            exitStashMode();
            enterCloseWindowMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_W) {
            cancelJumpMode();
            slotOpenWorkspaceManager();
            return;
        }
        if (isMetaOnly && e->key() == Qt::Key_M) {
            cancelJumpMode();
            slotOpenMarkerManager();
            return;
        }

        if (e->key() == Qt::Key_Comma || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char(','))) {
            slotCycleHudView(-1);
            return;
        }

        if (e->key() == Qt::Key_Period || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char('.'))) {
            slotCycleHudView(1);
            return;
        }

        if (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) {
            exitStashMode();
            return;
        }

        const QString text = extractKeyChar();
        if (!text.isEmpty()) {
            const QChar keyChar = text.at(0);
            const bool hasKey = (isKeyboardOverlayActive() && m_keyboardOverlayWindowMap.contains(keyChar)) || m_jumpLetterToWindow.contains(keyChar);
            if (hasKey) {
                EffectWindow *target = (isKeyboardOverlayActive() && m_keyboardOverlayWindowMap.contains(keyChar))
                    ? m_keyboardOverlayWindowMap.value(keyChar)
                    : m_jumpLetterToWindow.value(keyChar);
                if (target && !target->isDeleted()) {
                    if (isCtrl) {
                        // Shortcut Ctrl + Key adds chosen window to stash
                        m_stashedWindows.insert(target);
                        m_workspaceMinimizedWindows.remove(target);
                        target->setMinimized(true);
                        playThemedSound(OutlineSound::SoundEffect::Stash);

                        m_jumpLetterToWindow = assignJumpLetters();
                        m_badgeTextureCache.clear();
                        m_hudHeaderTexture.reset();
                        m_hudHeaderCacheKey = -1;
                        m_hudHintBarTexture.reset();
                        m_hudHintBarCacheKey = -1;
                        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                        }
                        effects->addRepaintFull();
                        return;
                    }

                    if (isShift) {
                        target->closeWindow();
                        m_stashedWindows.remove(target);
                        m_workspaceMinimizedWindows.remove(target);
                        m_closingWindows.insert(target);
                        m_jumpLetterToWindow = assignJumpLetters();
                        m_badgeTextureCache.clear();
                        m_hudHeaderTexture.reset();
                        m_hudHeaderCacheKey = -1;
                        m_hudHintBarTexture.reset();
                        m_hudHintBarCacheKey = -1;
                        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                        }
                        effects->addRepaintFull();
                        return;
                    }

                    if (isWindowInStash(target)) {
                        addWindowToWorkspace(target, m_activeHudView);
                        m_jumpLetterToWindow = assignJumpLetters();
                        m_badgeTextureCache.clear();
                        m_hudHeaderTexture.reset();
                        m_hudHeaderCacheKey = -1;
                        m_hudHintBarTexture.reset();
                        m_hudHintBarCacheKey = -1;
                        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                        }
                        effects->addRepaintFull();
                        return;
                    } else {
                        // User pressed letter of a visible window on screen: focus/activate it
                        cancelJumpMode();
                        if (effects) {
                            if (m_tilingActive) {
                                performTilingSwap(target);
                            } else {
                                effects->activateWindow(target);
                            }
                            warpCursorToWindow(target);
                        }
                        return;
                    }
                }
            }
        }
        return;
    }

    if (m_closeWindowModeActive) {
        if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Space || (isMetaOnly && (e->key() == Qt::Key_Q || e->key() == Qt::Key_Space))) {
            exitCloseWindowMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_Q) {
            exitCloseWindowMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_A) {
            toggleSuperJumpMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_S) {
            exitCloseWindowMode();
            enterStashMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_P) {
            exitCloseWindowMode();
            enterPinMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_U) {
            exitCloseWindowMode();
            toggleAudioFilterMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_W) {
            cancelJumpMode();
            slotOpenWorkspaceManager();
            return;
        }
        if (isMetaOnly && e->key() == Qt::Key_M) {
            cancelJumpMode();
            slotOpenMarkerManager();
            return;
        }

        if (e->key() == Qt::Key_Comma || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char(','))) {
            slotCycleHudView(-1);
            return;
        }

        if (e->key() == Qt::Key_Period || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char('.'))) {
            slotCycleHudView(1);
            return;
        }

        const QString text = extractKeyChar();
        if (!text.isEmpty()) {
            const QChar keyChar = text.at(0);
            const bool hasKey = (isKeyboardOverlayActive() && m_keyboardOverlayWindowMap.contains(keyChar)) || m_jumpLetterToWindow.contains(keyChar);
            if (hasKey) {
                EffectWindow *target = (isKeyboardOverlayActive() && m_keyboardOverlayWindowMap.contains(keyChar))
                    ? m_keyboardOverlayWindowMap.value(keyChar)
                    : m_jumpLetterToWindow.value(keyChar);
                if (target && !target->isDeleted()) {
                    if (m_pinnedWindows.contains(target)) {
                        unpinWindow(target, m_activeHudView);
                    }
                    target->closeWindow();
                    m_stashedWindows.remove(target);
                    m_workspaceMinimizedWindows.remove(target);
                    m_closingWindows.insert(target);
                    m_jumpLetterToWindow = assignJumpLetters();
                    m_badgeTextureCache.clear();
                    m_hudHeaderTexture.reset();
                    m_hudHeaderCacheKey = -1;
                    m_hudHintBarTexture.reset();
                    m_hudHintBarCacheKey = -1;
                    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                    }
                    if (m_jumpLetterToWindow.isEmpty()) {
                        exitCloseWindowMode();
                        cancelJumpMode();
                    } else {
                        effects->addRepaintFull();
                    }
                    return;
                }
            }
        }
        return;
    }

    if (m_pinModeActive) {
        if (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Space || (isMetaOnly && (e->key() == Qt::Key_P || e->key() == Qt::Key_Space))) {
            exitPinMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_P) {
            exitPinMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_A) {
            toggleSuperJumpMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_S) {
            exitPinMode();
            enterStashMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_Q) {
            exitPinMode();
            enterCloseWindowMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_U) {
            exitPinMode();
            toggleAudioFilterMode();
            return;
        }

        if (isMetaOnly && e->key() == Qt::Key_W) {
            cancelJumpMode();
            slotOpenWorkspaceManager();
            return;
        }
        if (isMetaOnly && e->key() == Qt::Key_M) {
            cancelJumpMode();
            slotOpenMarkerManager();
            return;
        }

        if (e->key() == Qt::Key_Comma || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char(','))) {
            slotCycleHudView(-1);
            return;
        }

        if (e->key() == Qt::Key_Period || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char('.'))) {
            slotCycleHudView(1);
            return;
        }

        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && m_jumpLetterToWindow.size() == 1) {
            EffectWindow *target = m_jumpLetterToWindow.first();
            if (target && !target->isDeleted()) {
                enterPinWsSelectionMode(target);
                return;
            }
        }

        const QString text = extractKeyChar();
        if (!text.isEmpty()) {
            const QChar keyChar = text.at(0);
            const bool hasKey = (isKeyboardOverlayActive() && m_keyboardOverlayWindowMap.contains(keyChar)) || m_jumpLetterToWindow.contains(keyChar);
            if (hasKey) {
                EffectWindow *target = (isKeyboardOverlayActive() && m_keyboardOverlayWindowMap.contains(keyChar))
                    ? m_keyboardOverlayWindowMap.value(keyChar)
                    : m_jumpLetterToWindow.value(keyChar);
                if (target && !target->isDeleted()) {
                    enterPinWsSelectionMode(target);
                    return;
                }
            }
        }
        return;
    }

    if (m_superJumpActive && (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Space || (isMetaOnly && e->key() == Qt::Key_Space))) {
        exitSuperJumpMode();
        return;
    }

    if (isMetaOnly && e->key() == Qt::Key_A) {
        toggleSuperJumpMode();
        return;
    }

    const bool isMetaHeld = (mods & Qt::MetaModifier) || m_hudMetaHeld;
    if (e->key() == Qt::Key_Space) {
        if (isMetaHeld) {
            traverseRecentWindows(isShift ? -1 : 1);
            return;
        } else {
            cancelJumpMode(e->key());
            return;
        }
    }

    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (m_metaSpaceTraverseActive) {
            m_metaSpaceTraverseActive = false;
            slotCommitHistoryNavigation();
            warpCursorToWindow(m_activeWindow);
            cancelJumpMode(e->key());
            return;
        }

        // When there is only one card/badge to choose in HUD, Enter selects it!
        if (m_jumpLetterToWindow.size() == 1) {
            const QChar keyChar = m_jumpLetterToWindow.firstKey();
            if (m_groupJumpLetters.contains(keyChar) && m_groupJumpLetters.value(keyChar).size() > 1) {
                enterSubLeapForGroup(keyChar, m_groupJumpLetters.value(keyChar));
                return;
            }
            EffectWindow *target = m_jumpLetterToWindow.first();
            if (target && !target->isDeleted()) {
                if (handleSuperJumpCrossWorkspaceSelection(target, e->key())) {
                    return;
                }
                activateJumpTargetWindow(target, e->key());
                return;
            }
        }

        cancelJumpMode(e->key());
        return;
    }

    if (e->key() == Qt::Key_Backspace) {
        cancelJumpMode(e->key());
        return;
    }

    if (isMetaOnly && e->key() == Qt::Key_S) {
        enterStashMode();
        return;
    }

    if (isMetaOnly && e->key() == Qt::Key_Q) {
        enterCloseWindowMode();
        return;
    }

    if (isMetaOnly && e->key() == Qt::Key_P) {
        enterPinMode();
        return;
    }

    if (isMetaOnly && (e->key() == Qt::Key_U || e->key() == Qt::Key_A)) {
        toggleAudioFilterMode();
        return;
    }

    if (!isShift && ((e->key() == Qt::Key_Slash) || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char('/')) || (isCtrl && e->key() == Qt::Key_F))) {
        enterSearchMode();
        return;
    }

    if (e->key() == Qt::Key_Backtab) { // Shift+Tab: open Stash drawer
        enterStashMode();
        return;
    }

    if (e->key() == Qt::Key_Tab || (isMetaOnly && e->key() == Qt::Key_G)) {
        slotToggleHudGrouping();
        return;
    }

    if ((isMetaOnly && e->key() == Qt::Key_T) || (isShift && (e->key() == Qt::Key_T || (!e->text().isEmpty() && (e->text().at(0) == QLatin1Char('T') || e->text().at(0) == QLatin1Char('t')))))) {
        enterTilingPresetsMode();
        return;
    }

    if (isMetaOnly && e->key() == Qt::Key_W) {
        cancelJumpMode();
        slotOpenWorkspaceManager();
        return;
    }

    if (isMetaOnly && e->key() == Qt::Key_M) {
        cancelJumpMode();
        slotOpenMarkerManager();
        return;
    }

    if (e->key() == Qt::Key_Comma || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char(','))) {
        slotCycleHudView(-1);
        return;
    }

    if (e->key() == Qt::Key_Period || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char('.'))) {
        slotCycleHudView(1);
        return;
    }

    if (e->key() == Qt::Key_QuoteLeft || e->key() == Qt::Key_AsciiTilde || e->key() == Qt::Key_F1
        || (!e->text().isEmpty() && (e->text().at(0) == QLatin1Char('`') || e->text().at(0) == QLatin1Char('~')))) {
        slotCycleHudView(1);
        return;
    }

    bool isRecentKeyMatch = false;
    const QString rKey = m_jumpRecentKey.trimmed();
    if (rKey.compare(QLatin1String("Space"), Qt::CaseInsensitive) != 0 && !rKey.isEmpty()) {
        QKeySequence seq(rKey);
        if (!seq.isEmpty()) {
            const QKeyCombination comb = seq[0];
            const int keyOnly = comb.key();
            const Qt::KeyboardModifiers keyMods = comb.keyboardModifiers();
            if (e->key() == keyOnly && (keyMods == Qt::NoModifier || e->modifiers() == keyMods)) {
                isRecentKeyMatch = true;
            }
        }
        if (!isRecentKeyMatch && !e->text().isEmpty() && e->text().compare(rKey, Qt::CaseInsensitive) == 0) {
            isRecentKeyMatch = true;
        }
    }

    if (!isCtrl && isRecentKeyMatch) {
        if (isAlt) {
            EffectWindow *target = findRecentWindow();
            if (target && !target->isDeleted()) {
                const int targetWs = (m_superJumpActive && m_superJumpTargetWorkspace >= 1 && m_superJumpTargetWorkspace <= m_workspaces.size())
                                     ? m_superJumpTargetWorkspace
                                     : (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size() ? m_activeHudView : 0);
                if (m_pinnedWindows.contains(target)) {
                    unpinWindow(target, targetWs);
                } else if (!m_superJumpActive && findWorkspaceForWindow(target) == targetWs && target->isOnCurrentDesktop()) {
                    pinWindow(target);
                } else {
                    if (targetWs >= 1 && targetWs <= m_workspaces.size()) {
                        addWindowToWorkspace(target, targetWs);
                        applyWorkspaceViewIsolation(targetWs);
                    } else if (targetWs == 0) {
                        addWindowToWorkspace(target, 0);
                    }
                    if (effects && effects->currentDesktop()) {
                        effects->windowToDesktops(target, {effects->currentDesktop()});
                    }
                    m_workspaceMinimizedWindows.remove(target);
                    m_stashedWindows.remove(target);
                    if (target->isMinimized()) {
                        target->setMinimized(false);
                    }
                }
                cancelJumpMode(e->key());
                if (m_tilingActive) {
                    performTilingSwap(target);
                } else {
                    effects->activateWindow(target);
                }
                warpCursorToWindow(target);
            }
            return;
        }
        slotJumpToRecentWindow(e->key());
        return;
    }

    const QString text = extractKeyChar();

    if (!text.isEmpty()) {
        const QChar keyChar = text.at(0);

        // Check if keyChar is a group trigger key with multiple windows
        if (m_groupJumpLetters.contains(keyChar) && m_groupJumpLetters.value(keyChar).size() > 1) {
            const auto group = m_groupJumpLetters.value(keyChar);
            if (isAlt) {
                // Smart Workspace summon: pull all windows in group to current desktop!
                const int targetWs = (m_superJumpActive && m_superJumpTargetWorkspace >= 1 && m_superJumpTargetWorkspace <= m_workspaces.size())
                                     ? m_superJumpTargetWorkspace
                                     : (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size() ? m_activeHudView : 0);
                VirtualDesktop *currDesktop = effects->currentDesktop();
                EffectWindow *topWin = nullptr;
                for (EffectWindow *w : group) {
                    if (w && !w->isDeleted()) {
                        if (m_pinnedWindows.contains(w)) {
                            unpinWindow(w, targetWs);
                        } else {
                            if (targetWs >= 1 && targetWs <= m_workspaces.size()) {
                                addWindowToWorkspace(w, targetWs);
                            }
                            if (currDesktop) {
                                effects->windowToDesktops(w, {currDesktop});
                            }
                            m_workspaceMinimizedWindows.remove(w);
                            m_stashedWindows.remove(w);
                            if (w->isMinimized()) {
                                w->setMinimized(false);
                            }
                        }
                        if (!topWin) {
                            topWin = w;
                        }
                    }
                }
                if (targetWs >= 1 && targetWs <= m_workspaces.size()) {
                    applyWorkspaceViewIsolation(targetWs);
                }
                cancelJumpMode(e->key());
                if (topWin) {
                    effects->activateWindow(topWin);
                    warpCursorToWindow(topWin);
                }
                return;
            }

            if (isCtrl) {
                // Toggle stash/minimized for whole group
                bool anyVisible = false;
                for (EffectWindow *w : group) {
                    if (w && !w->isDeleted() && !w->isMinimized()) {
                        anyVisible = true;
                        break;
                    }
                }
                for (EffectWindow *w : group) {
                    if (w && !w->isDeleted()) {
                        if (anyVisible) {
                            m_stashedWindows.insert(w);
                            w->setMinimized(true);
                        } else {
                            m_stashedWindows.remove(w);
                            w->setMinimized(false);
                        }
                    }
                }
                m_jumpLetterToWindow = assignJumpLetters();
                m_badgeTextureCache.clear();
                m_hudHeaderTexture.reset();
                m_hudHeaderCacheKey = -1;
                if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                    m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                }
                effects->addRepaintFull();
                return;
            }

            // Normal or Shift: enter Sub-Leap to disambiguate or pick individual window to close
            enterSubLeapForGroup(keyChar, group);
            return;
        }

        // Check direct jump letter (either single marked window or unique direct letter)
        const bool hasKey = (isKeyboardOverlayActive() && m_keyboardOverlayWindowMap.contains(keyChar)) || m_jumpLetterToWindow.contains(keyChar);
        if (hasKey) {
            EffectWindow *target = (isKeyboardOverlayActive() && m_keyboardOverlayWindowMap.contains(keyChar))
                ? m_keyboardOverlayWindowMap.value(keyChar)
                : m_jumpLetterToWindow.value(keyChar);
            if (target && !target->isDeleted()) {
                if (isShift) {
                    // Quick close single window
                    target->closeWindow();
                    m_closingWindows.insert(target);
                    m_jumpLetterToWindow = assignJumpLetters();
                    m_badgeTextureCache.clear();
                    if (m_jumpLetterToWindow.isEmpty()) {
                        cancelJumpMode(e->key());
                    } else {
                        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                        }
                        effects->addRepaintFull();
                    }
                    return;
                }

                if (isCtrl) {
                    // Quick stash toggle
                    if (!target->isMinimized()) {
                        m_stashedWindows.insert(target);
                        target->setMinimized(true);
                    } else {
                        m_stashedWindows.remove(target);
                        target->setMinimized(false);
                    }
                    m_jumpLetterToWindow = assignJumpLetters();
                    m_badgeTextureCache.clear();
                    m_hudHeaderTexture.reset();
                    m_hudHeaderCacheKey = -1;
                    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                    }
                    effects->addRepaintFull();
                    return;
                }

                if (isAlt) {
                    // Pin / Pull / Drop
                    const int targetWs = (m_superJumpActive && m_superJumpTargetWorkspace >= 1 && m_superJumpTargetWorkspace <= m_workspaces.size())
                                         ? m_superJumpTargetWorkspace
                                         : (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size() ? m_activeHudView : 0);
                    if (m_pinnedWindows.contains(target)) {
                        // Drop pinned window into current workspace
                        unpinWindow(target, targetWs);
                    } else if (!m_superJumpActive && findWorkspaceForWindow(target) == targetWs && target->isOnCurrentDesktop()) {
                        // Already in current workspace and desktop -> Pin across all workspaces!
                        pinWindow(target);
                    } else {
                        // Pull from other desktop/workspace to current desktop & workspace
                        if (targetWs >= 1 && targetWs <= m_workspaces.size()) {
                            addWindowToWorkspace(target, targetWs);
                            applyWorkspaceViewIsolation(targetWs);
                        } else if (targetWs == 0) {
                            addWindowToWorkspace(target, 0);
                        }
                        if (effects && effects->currentDesktop()) {
                            effects->windowToDesktops(target, {effects->currentDesktop()});
                        }
                        m_workspaceMinimizedWindows.remove(target);
                        m_stashedWindows.remove(target);
                        if (target->isMinimized()) {
                            target->setMinimized(false);
                        }
                    }
                    cancelJumpMode(e->key());
                    effects->activateWindow(target);
                    warpCursorToWindow(target);
                    return;
                }

                // Normal jump & activate: switch to the window's workspace and desktop!
                if (handleSuperJumpCrossWorkspaceSelection(target, e->key())) {
                    return;
                }
                activateJumpTargetWindow(target, e->key());
                return;
            }
        }
    }

    if (!text.isEmpty()) {
        const QChar keyChar = text.at(0).toUpper();
        triggerInvalidKeyFeedback(keyChar);
        return;
    }

    cancelJumpMode(e->key());
}

void ActiveWindowOutlineEffect::triggerInvalidKeyFeedback(const QChar &key)
{
    m_invalidJumpKey = key.toUpper();
    m_invalidJumpKeyTimer.start();
    if (m_soundFeedbackEnabled) {
        playThemedSound(OutlineSound::SoundEffect::InvalidKey);
    } else {
        OutlineSound::playSound(OutlineSound::SoundEffect::InvalidKey, 40, OutlineSound::SoundProfile::CrispClick);
    }
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::playThemedSound(OutlineSound::SoundEffect effect)
{
    if (!m_soundFeedbackEnabled || m_soundFeedbackVolume <= 0) {
        return;
    }
    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;
    const QString profStr = theme.resolveVar(QStringLiteral("$soundProfile")).isEmpty()
        ? theme.sound.profile
        : theme.resolveVar(QStringLiteral("$soundProfile"));
    const OutlineSound::SoundProfile profile = OutlineSound::profileFromString(profStr);
    const double volFactor = theme.resolveDouble(QStringLiteral("$soundVolume"), theme.sound.volume > 0.0 ? theme.sound.volume : 1.0);
    const int effectiveVol = qBound(0, qRound(m_soundFeedbackVolume * volFactor), 100);

    QString customFile;
    switch (effect) {
    case OutlineSound::SoundEffect::EnterJump: customFile = theme.sound.enterSound; break;
    case OutlineSound::SoundEffect::LandWindow: customFile = theme.sound.landSound; break;
    case OutlineSound::SoundEffect::SwitchWorkspace: customFile = theme.sound.workspaceSound; break;
    case OutlineSound::SoundEffect::NavTab: customFile = theme.sound.navSound; break;
    case OutlineSound::SoundEffect::Dismiss: customFile = theme.sound.dismissSound; break;
    case OutlineSound::SoundEffect::Stash: customFile = theme.sound.stashSound; break;
    case OutlineSound::SoundEffect::InvalidKey: customFile = theme.sound.dismissSound; break;
    }

    OutlineSound::playSound(effect, effectiveVol, profile, customFile);
}


} // namespace KWin
