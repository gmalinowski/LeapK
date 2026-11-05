/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk_internal.h"

namespace KWin
{


KWIN_EFFECT_FACTORY_SUPPORTED(ActiveWindowOutlineEffect, "metadata.json",
                              return ActiveWindowOutlineEffect::supported();)

ActiveWindowOutlineEffect::ActiveWindowOutlineEffect()
{
    m_lastDesktop = effects ? effects->currentDesktop() : nullptr;
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(16);
    connect(m_animationTimer, &QTimer::timeout,
            this, &ActiveWindowOutlineEffect::slotAnimationTick);
    m_animationClock.start();

    m_menuHideTimer = new QTimer(this);
    m_menuHideTimer->setSingleShot(true);
    connect(m_menuHideTimer, &QTimer::timeout, this, [this] {
        m_visibilityState.handleMenuHideTimerTimeout(hasVisibleBlockingPopup(), m_visibilityConfig);
        updateAnimationTimer();
        if (m_activeWindow) {
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
    });

    m_menuShowTimer = new QTimer(this);
    m_menuShowTimer->setSingleShot(true);
    connect(m_menuShowTimer, &QTimer::timeout, this, [this] {
        m_visibilityState.handleMenuShowTimerTimeout(hasVisibleBlockingPopup(), m_visibilityConfig);
        updateAnimationTimer();
        if (m_activeWindow && shouldOutlineWindow(m_activeWindow)) {
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
    });

    m_windowHideTimer = new QTimer(this);
    m_windowHideTimer->setSingleShot(true);
    connect(m_windowHideTimer, &QTimer::timeout, this, [this] {
        if (m_delayedHideWindow) {
            if (m_visibilityConfig.fadeDuration > 0) {
                m_delayedHideState.startFade(m_visibilityConfig.fadeDuration);
                updateAnimationTimer();
                effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
            } else {
                effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
                m_suppressedWindows.remove(m_delayedHideWindow);
                m_lastGeometry.remove(m_delayedHideWindow);
                m_outlineCache.remove(m_delayedHideWindow);
                m_delayedHideWindow = nullptr;
                m_delayedHideState.reset();
            }
        }
    });

    m_windowShowTimer = new QTimer(this);
    m_windowShowTimer->setSingleShot(true);
    connect(m_windowShowTimer, &QTimer::timeout, this, [this] {
        if (m_activeWindow && shouldOutlineWindow(m_activeWindow)
            && !m_visibilityState.popupBlocked && !hasVisibleBlockingPopup()) {
            if (m_visibilityConfig.fadeDuration > 0) {
                m_visibilityState.visibilityOpacity = 0.0;
            }
            m_visibilityState.setTargetOpacity(1.0, m_visibilityConfig.fadeDuration);
            updateAnimationTimer();
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
    });

    m_ambilightTimer = new QTimer(this);
    m_ambilightTimer->setInterval(1000);
    connect(m_ambilightTimer, &QTimer::timeout, this, [this] {
        if ((m_ambilightEnabled || m_ensurePerimeterContrast) && m_activeWindow && !m_activeWindow->isMinimized() && m_activeWindow->isOnCurrentDesktop()) {
            m_ambilightSampleNeeded = true;
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
    });

    m_jumpTimeoutTimer = new QTimer(this);
    m_jumpTimeoutTimer->setSingleShot(true);
    connect(m_jumpTimeoutTimer, &QTimer::timeout, this, [this]() {
        cancelJumpMode();
    });

    m_expectedJumpTargetTimer = new QTimer(this);
    m_expectedJumpTargetTimer->setSingleShot(true);
    connect(m_expectedJumpTargetTimer, &QTimer::timeout, this, [this]() {
        m_expectedJumpTarget = nullptr;
    });

    m_historyCommitTimer = new QTimer(this);
    m_historyCommitTimer->setSingleShot(true);
    connect(m_historyCommitTimer, &QTimer::timeout, this, &ActiveWindowOutlineEffect::slotCommitHistoryNavigation);

    m_audioRefreshTimer = new QTimer(this);
    connect(m_audioRefreshTimer, &QTimer::timeout, this, [this]() {
        const bool badgesAlwaysOn = (m_outlineEnabled && m_badgeConfig.windowBadgesEnabled && m_badgeDisplayMode == 1);
        if (m_jumpMode || badgesAlwaysOn) {
            m_audioTracker.refresh(getKnownWindowsInfo());
            if (m_jumpMode && m_audioFilterModeActive) {
                m_jumpLetterToWindow = assignJumpLetters();
            }
            m_badgeTextureCache.clear();
            m_hudHeaderTexture.reset();
            m_hudHeaderCacheKey = -1;
            updateAnimationTimer();
            if (effects) {
                effects->addRepaintFull();
            }
        }
    });

    m_nerdStatsTimer = new QTimer(this);
    connect(m_nerdStatsTimer, &QTimer::timeout, this, [this]() {
        if (m_jumpMode && m_nerdStatsActive && effects) {
            effects->addRepaintFull();
        }
    });

    reconfigure(ReconfigureAll);

    // Some third-party effect KCMs are not reliably reconfigured by KWin's
    // D-Bus hook on every Plasma version. Watch kwinrc as a fallback so a
    // saved setting always reaches the running effect without a logout.
    m_configPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/kwinrc");
    m_configWatcher = new QFileSystemWatcher(this);
    if (QFileInfo::exists(m_configPath)) {
        m_configWatcher->addPath(m_configPath);
    }

    m_configReloadTimer = new QTimer(this);
    m_configReloadTimer->setSingleShot(true);
    m_configReloadTimer->setInterval(150);

    const auto scheduleConfigReload = [this] {
        m_configReloadTimer->start();
    };
    connect(m_configWatcher, &QFileSystemWatcher::fileChanged,
            this, scheduleConfigReload);
    connect(m_configWatcher, &QFileSystemWatcher::directoryChanged,
            this, scheduleConfigReload);
    connect(m_configReloadTimer, &QTimer::timeout, this, [this] {
        const QString configDirectory = QFileInfo(m_configPath).absolutePath();
        if (QFileInfo::exists(m_configPath)
            && !m_configWatcher->files().contains(m_configPath)) {
            m_configWatcher->addPath(m_configPath);
        }
        if (m_configWatcher->directories().contains(configDirectory)) {
            m_configWatcher->removePath(configDirectory);
        }
        effects->config()->reparseConfiguration();
        reconfigure(ReconfigureAll);
    });

    // KConfig may replace the file atomically. When that happens the file
    // watch disappears, so temporarily watch its parent until it reappears.
    connect(m_configWatcher, &QFileSystemWatcher::fileChanged,
            this, [this] {
        if (!m_configWatcher->files().contains(m_configPath)) {
            const QString configDirectory = QFileInfo(m_configPath).absolutePath();
            if (!m_configWatcher->directories().contains(configDirectory)) {
                m_configWatcher->addPath(configDirectory);
            }
        }
    });

    const QString marksDir = LeapKMarks::defaultMarksDirectory();
    const QString marksPath = LeapKMarks::defaultMarksFilePath();
    m_marksWatcher = new QFileSystemWatcher(this);
    if (QFileInfo::exists(marksPath)) {
        m_marksWatcher->addPath(marksPath);
    }
    if (QDir(marksDir).exists()) {
        m_marksWatcher->addPath(marksDir);
    }

    m_marksReloadTimer = new QTimer(this);
    m_marksReloadTimer->setSingleShot(true);
    m_marksReloadTimer->setInterval(150);

    const auto scheduleMarksReload = [this] {
        m_marksReloadTimer->start();
    };
    connect(m_marksWatcher, &QFileSystemWatcher::fileChanged,
            this, scheduleMarksReload);
    connect(m_marksWatcher, &QFileSystemWatcher::directoryChanged,
            this, scheduleMarksReload);
    connect(m_marksReloadTimer, &QTimer::timeout, this, [this, marksDir, marksPath] {
        if (QFileInfo::exists(marksPath) && !m_marksWatcher->files().contains(marksPath)) {
            m_marksWatcher->addPath(marksPath);
        }
        m_persistentMarks = LeapKMarks::loadPersistentMarks();
        refreshPersistentMarksForWindows();
        if (effects) {
            effects->addRepaintFull();
        }
    });

    connect(m_marksWatcher, &QFileSystemWatcher::fileChanged,
            this, [this, marksDir, marksPath] {
        if (!m_marksWatcher->files().contains(marksPath)) {
            if (QDir(marksDir).exists() && !m_marksWatcher->directories().contains(marksDir)) {
                m_marksWatcher->addPath(marksDir);
            }
        }
    });

    m_activeWindow = effects->activeWindow();
    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        connect(w, &EffectWindow::windowFrameGeometryChanged,
                this, &ActiveWindowOutlineEffect::slotWindowFrameGeometryChanged);
        connect(w, &EffectWindow::minimizedChanged,
                this, &ActiveWindowOutlineEffect::slotWindowMinimizedChanged);
        connect(w, &EffectWindow::windowHiddenChanged,
                this, &ActiveWindowOutlineEffect::slotWindowHiddenChanged);
        connect(w, &EffectWindow::windowFullScreenChanged,
                this, &ActiveWindowOutlineEffect::slotWindowFullScreenChanged);
        connect(w, &EffectWindow::windowDamaged,
                this, &ActiveWindowOutlineEffect::slotWindowDamaged);
        connect(w, &EffectWindow::windowStartUserMovedResized,
                this, &ActiveWindowOutlineEffect::slotWindowStartUserMovedResized);
        connect(w, &EffectWindow::windowStepUserMovedResized,
                this, &ActiveWindowOutlineEffect::slotWindowStepUserMovedResized);
        connect(w, &EffectWindow::windowFinishUserMovedResized,
                this, &ActiveWindowOutlineEffect::slotWindowFinishUserMovedResized);
    }

    connect(effects, &EffectsHandler::windowActivated,
            this, &ActiveWindowOutlineEffect::slotWindowActivated);
    connect(effects, &EffectsHandler::windowAdded,
            this, &ActiveWindowOutlineEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &ActiveWindowOutlineEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::mouseChanged,
            this, &ActiveWindowOutlineEffect::slotMouseChanged);
    connect(effects, &EffectsHandler::showingDesktopChanged,
            this, &ActiveWindowOutlineEffect::slotShowingDesktopChanged);
    connect(effects, &EffectsHandler::activeFullScreenEffectChanged,
            this, &ActiveWindowOutlineEffect::slotActiveFullScreenEffectChanged);
    connect(effects, &EffectsHandler::hasActiveFullScreenEffectChanged,
            this, &ActiveWindowOutlineEffect::slotActiveFullScreenEffectChanged);
    connect(effects, &EffectsHandler::stackingOrderChanged,
            this, &ActiveWindowOutlineEffect::repaintAllOutlines);
    connect(effects, &EffectsHandler::desktopChanged,
            this, [this]() {
                VirtualDesktop *cur = effects ? effects->currentDesktop() : nullptr;
                if (m_lastDesktop && cur && m_lastDesktop != cur) {
                    playThemedSound(OutlineSound::SoundEffect::SwitchWorkspace);
                }
                m_lastDesktop = cur;
                if (cur) {
                    const QString curWsId = workspaceIdForView(m_activeHudView);
                    m_workspaceLastVirtualDesktopId[curWsId] = cur->id();
                    if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
                        m_workspaces[m_activeHudView - 1].lastVirtualDesktopId = cur->id();
                        LeapKWorkspace::saveWorkspaces(m_workspaces);
                    } else if (m_activeHudView == 0 && effects && effects->config()) {
                        KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
                        primaryConfig.writeEntry("StandardLastVirtualDesktopId", cur->id());
                        primaryConfig.sync();
                    }
                }
                repaintAllOutlines();
            });
    connect(effects, &EffectsHandler::screenLockingChanged,
            this, [this](bool locked) {
                if (locked) {
                    if (m_jumpMode) {
                        cancelJumpMode();
                    }
                    m_badgeTextureCache.clear();
                    m_thumbnailCache.clear();
                    m_lastCaptureTimestamp.clear();
                }
                repaintAllOutlines();
            });
    connect(effects, &EffectsHandler::screenAboutToLock,
            this, [this]() {
                if (m_jumpMode) {
                    cancelJumpMode();
                }
                repaintAllOutlines();
            });
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(effects, &EffectsHandler::sessionStateChanged,
            this, [this]() {
                if (m_jumpMode) {
                    cancelJumpMode();
                }
                m_badgeTextureCache.clear();
                m_thumbnailCache.clear();
                m_lastCaptureTimestamp.clear();
                repaintAllOutlines();
            });
#endif

    initShortcuts();

    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/LeapK"),
        QStringLiteral("org.kde.LeapK"),
        this,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    );
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/KLeap"),
        QStringLiteral("org.kde.KLeap"),
        this,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    );
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/ActiveWindowOutline"),
        QStringLiteral("org.kde.ActiveWindowOutline"),
        this,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    );

    m_kdeTilesEditorWatchTimer = new QTimer(this);
    m_kdeTilesEditorWatchTimer->setInterval(100);
    connect(m_kdeTilesEditorWatchTimer, &QTimer::timeout, this, &ActiveWindowOutlineEffect::checkKdeTilesEditorState);
    m_kdeTilesEditorWatchTimer->start();
}

ActiveWindowOutlineEffect::~ActiveWindowOutlineEffect()
{
    if (m_kdeTilesEditorWatchTimer) {
        m_kdeTilesEditorWatchTimer->stop();
    }
    if (m_tilingToastClearTimer) {
        m_tilingToastClearTimer->stop();
    }
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/LeapK"));
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/KLeap"));
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/ActiveWindowOutline"));
    finishKeyboardGrab();
    if (m_jumpMode) {
        cancelJumpMode();
    }
    if (m_globalShortcutsBlocked) {
        setKdeGlobalShortcutsBlocked(false);
    }
    if (m_jumpTimeoutTimer) {
        m_jumpTimeoutTimer->stop();
    }
    if (m_expectedJumpTargetTimer) {
        m_expectedJumpTargetTimer->stop();
    }
    if (m_historyCommitTimer) {
        m_historyCommitTimer->stop();
    }
    if (m_previewSafetyTimer && m_previewSafetyTimer->isActive()) {
        m_previewSafetyTimer->stop();
    }
    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();

    if (m_animationTimer) {
        m_animationTimer->stop();
    }
    for (EffectWindow *w : m_workspaceMinimizedWindows) {
        if (w && !w->isDeleted() && w->isMinimized()) {
            w->setMinimized(false);
        }
    }
    m_workspaceMinimizedWindows.clear();
    m_stashedWindows.clear();
    if (m_menuHideTimer) {
        m_menuHideTimer->stop();
    }
    if (m_menuShowTimer) {
        m_menuShowTimer->stop();
    }
    if (m_windowHideTimer) {
        m_windowHideTimer->stop();
    }
    if (m_windowShowTimer) {
        m_windowShowTimer->stop();
    }
    if (m_ambilightTimer) {
        m_ambilightTimer->stop();
    }
}

void ActiveWindowOutlineEffect::initShortcuts()
{
    QAction *toggleAction = new QAction(this);
    toggleAction->setObjectName(QStringLiteral("ToggleActiveWindowOutline"));
    toggleAction->setText(QStringLiteral("LeapK: Toggle Window Highlight"));
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_O));
    KGlobalAccel::self()->setGlobalShortcut(toggleAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_O));
    connect(toggleAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotToggleAction);

    QAction *markAction = new QAction(this);
    markAction->setObjectName(QStringLiteral("MarkActiveWindow"));
    markAction->setText(QStringLiteral("LeapK: Toggle Mark Window (Quick)"));
    KGlobalAccel::self()->setDefaultShortcut(markAction, QList<QKeySequence>());
    KGlobalAccel::self()->setGlobalShortcut(markAction, QKeySequence());
    connect(markAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotMarkWindowToggle);

    QAction *markWithColorAction = new QAction(this);
    markWithColorAction->setObjectName(QStringLiteral("MarkActiveWindowWithColor"));
    markWithColorAction->setText(QStringLiteral("LeapK: Mark Window with Color Picker"));
    KGlobalAccel::self()->setDefaultShortcut(markWithColorAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_Space));
    KGlobalAccel::self()->setGlobalShortcut(markWithColorAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_Space));
    connect(markWithColorAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotMarkWindowWithColor);

    QAction *workspaceManagerAction = new QAction(this);
    workspaceManagerAction->setObjectName(QStringLiteral("OpenWorkspaceManager"));
    workspaceManagerAction->setText(QStringLiteral("LeapK: Open Workspace Manager"));
    KGlobalAccel::self()->setDefaultShortcut(workspaceManagerAction, QList<QKeySequence>());
    KGlobalAccel::self()->setGlobalShortcut(workspaceManagerAction, QKeySequence());
    connect(workspaceManagerAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotOpenWorkspaceManager);

    QAction *markerManagerAction = new QAction(this);
    markerManagerAction->setObjectName(QStringLiteral("OpenMarkerManager"));
    markerManagerAction->setText(QStringLiteral("LeapK: Open Marker Manager"));
    KGlobalAccel::self()->setDefaultShortcut(markerManagerAction, QList<QKeySequence>());
    KGlobalAccel::self()->setGlobalShortcut(markerManagerAction, QKeySequence());
    connect(markerManagerAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotOpenMarkerManager);

    QAction *toggleGroupAction = new QAction(this);
    toggleGroupAction->setObjectName(QStringLiteral("ToggleHudGrouping"));
    toggleGroupAction->setText(QStringLiteral("LeapK: Toggle HUD Window Grouping"));
    KGlobalAccel::self()->setDefaultShortcut(toggleGroupAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_G));
    KGlobalAccel::self()->setGlobalShortcut(toggleGroupAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_G));
    connect(toggleGroupAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotToggleHudGrouping);

    QAction *pinAction = new QAction(this);
    pinAction->setObjectName(QStringLiteral("TogglePinActiveWindow"));
    pinAction->setText(QStringLiteral("LeapK: Toggle Pin Active Window"));
    KGlobalAccel::self()->setDefaultShortcut(pinAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_P));
    KGlobalAccel::self()->setGlobalShortcut(pinAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_P));
    connect(pinAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotTogglePinActiveWindow);

    QAction *zoomThumbAction = new QAction(this);
    zoomThumbAction->setObjectName(QStringLiteral("ToggleHudThumbnailZoom"));
    zoomThumbAction->setText(QStringLiteral("LeapK: Toggle HUD Thumbnail Zoom"));
    KGlobalAccel::self()->setDefaultShortcut(zoomThumbAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::Key_Z));
    KGlobalAccel::self()->setGlobalShortcut(zoomThumbAction, QKeySequence(Qt::META | Qt::Key_Z));
    connect(zoomThumbAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotToggleThumbnailZoom);

    QAction *jumpAction = new QAction(this);
    jumpAction->setObjectName(QStringLiteral("JumpToWindow"));
    jumpAction->setText(QStringLiteral("LeapK: Jump to Window"));
    KGlobalAccel::self()->setDefaultShortcut(jumpAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::Key_Space));
    KGlobalAccel::self()->setGlobalShortcut(jumpAction, QKeySequence(Qt::META | Qt::Key_Space));
    connect(jumpAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotTriggerJumpMode);

    QAction *audioAction = new QAction(this);
    audioAction->setObjectName(QStringLiteral("JumpToAudioWindow"));
    audioAction->setText(QStringLiteral("LeapK: Jump to Audio Window"));
    KGlobalAccel::self()->setDefaultShortcut(audioAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_A));
    KGlobalAccel::self()->setGlobalShortcut(audioAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_A));
    connect(audioAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotTriggerAudioFilterMode);

    QAction *searchAction = new QAction(this);
    searchAction->setObjectName(QStringLiteral("JumpToSearchWindow"));
    searchAction->setText(QStringLiteral("LeapK: Fuzzy Search Windows"));
    KGlobalAccel::self()->setDefaultShortcut(searchAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_F) << QKeySequence(Qt::META | Qt::Key_Slash));
    KGlobalAccel::self()->setGlobalShortcut(searchAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_F));
    connect(searchAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotTriggerSearchMode);

    QAction *prevRecentAction = new QAction(this);
    prevRecentAction->setObjectName(QStringLiteral("PrevRecentWindow"));
    prevRecentAction->setText(QStringLiteral("LeapK: Previous Recent Window"));
    KGlobalAccel::self()->setDefaultShortcut(prevRecentAction, QList<QKeySequence>());
    KGlobalAccel::self()->setGlobalShortcut(prevRecentAction, QKeySequence());
    connect(prevRecentAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotPrevRecentWindow);

    QAction *nextRecentAction = new QAction(this);
    nextRecentAction->setObjectName(QStringLiteral("NextRecentWindow"));
    nextRecentAction->setText(QStringLiteral("LeapK: Next Recent Window"));
    KGlobalAccel::self()->setDefaultShortcut(nextRecentAction, QList<QKeySequence>());
    KGlobalAccel::self()->setGlobalShortcut(nextRecentAction, QKeySequence());
    connect(nextRecentAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotNextRecentWindow);

    QAction *tilingAction = new QAction(this);
    tilingAction->setObjectName(QStringLiteral("ToggleTilingPresets"));
    tilingAction->setText(QStringLiteral("LeapK: Tiling Presets"));
    KGlobalAccel::self()->setDefaultShortcut(tilingAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::Key_T));
    KGlobalAccel::self()->setGlobalShortcut(tilingAction, QKeySequence(Qt::META | Qt::Key_T));
    connect(tilingAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotTriggerTilingPresetsMode);

    QAction *tilingEditAction = new QAction(this);
    tilingEditAction->setObjectName(QStringLiteral("ToggleTilingEdit"));
    tilingEditAction->setText(QStringLiteral("LeapK: Tile Edit Mode (Swap & Resize)"));
    KGlobalAccel::self()->setDefaultShortcut(tilingEditAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::Key_Return) << QKeySequence(Qt::META | Qt::Key_Enter));
    KGlobalAccel::self()->setGlobalShortcut(tilingEditAction, QKeySequence(Qt::META | Qt::Key_Return));
    connect(tilingEditAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotToggleTilingEditMode);

    QAction *tilingFloatAction = new QAction(this);
    tilingFloatAction->setObjectName(QStringLiteral("ToggleActiveWindowFloating"));
    tilingFloatAction->setText(QStringLiteral("LeapK: Toggle Window Float/Tile"));
    KGlobalAccel::self()->setDefaultShortcut(tilingFloatAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_F));
    KGlobalAccel::self()->setGlobalShortcut(tilingFloatAction, QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_F));
    connect(tilingFloatAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotToggleActiveWindowFloating);
}

bool ActiveWindowOutlineEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void ActiveWindowOutlineEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
    KConfigGroup fallbackConfig = effects->config()->group(QStringLiteral("Effect-active-window-outline"));
    if (!effects->config()->hasGroup(QStringLiteral("Effect-leapk")) && effects->config()->hasGroup(QStringLiteral("Effect-active-window-outline"))) {
        fallbackConfig.copyTo(&primaryConfig);
        primaryConfig.sync();
    }
    KConfigGroup config = primaryConfig.exists() ? primaryConfig : fallbackConfig;
    const bool configOutlineEnabled = config.readEntry("OutlineEnabled", true);
    if (m_outlineEnabled != configOutlineEnabled) {
        slotToggleAction();
    }
    m_useAccentColor = config.readEntry("UseAccentColor", true);
    m_accentOpacity = qBound(0, config.readEntry("AccentOpacity", 66), 100);
    m_customColor = config.readEntry("CustomColor", QColor(61, 174, 233));
    m_ensurePerimeterContrast = config.readEntry("EnsurePerimeterContrast", config.readEntry("AmbilightEnsureContrast", true));
    m_ambilightEnsureContrast = m_ensurePerimeterContrast;
    m_ambilightEnabled = config.readEntry("AmbilightEnabled", false);
    m_ambilightMode = qBound(0, config.readEntry("AmbilightMode", 0), 2);
    m_ambilightOpacity = qBound(0, config.readEntry("AmbilightOpacity", 100), 100);
    m_ambilightFrequency = qBound<qreal>(0.1, config.readEntry("AmbilightFrequency", 1.5), 10.0);
    m_ambilightDuration = qBound<qreal>(0.1, config.readEntry("AmbilightDuration", 0.5), 5.0);
    m_ambilightSaturationLeveling = qBound(0, config.readEntry("AmbilightSaturationLeveling", 65), 100);
    m_ambilightFallbackStrength = qBound(0, config.readEntry("AmbilightFallbackStrength", 50), 100);
    m_ambilightMinBrightness = qBound(0, config.readEntry("AmbilightMinBrightness", 25), 50);
    m_ambilightMaxBrightness = qBound(50, config.readEntry("AmbilightMaxBrightness", 85), 100);

    if (m_ambilightTimer) {
        m_ambilightTimer->setInterval(qRound(1000.0 / m_ambilightFrequency));
    }

    if (m_ambilightEnabled || m_ensurePerimeterContrast) {
        m_ambilightSampleNeeded = true;
        m_ambilightTargetColor = QColor();
        m_ambilightCurrentColor = QColor();
        if (m_ambilightTimer && !m_ambilightTimer->isActive()) {
            m_ambilightTimer->start();
        }
    } else {
        if (m_ambilightTimer && m_ambilightTimer->isActive()) {
            m_ambilightTimer->stop();
        }
        m_ambilightInTransition = false;
        m_ambilightTargetColor = QColor();
        m_ambilightCurrentColor = QColor();
    }

    m_width = qBound(1, config.readEntry("BorderWidth", 2), 100);
    m_gap = qBound(0, config.readEntry("OutlineGap", 0), 200);
    m_radius = qBound(0, config.readEntry("BorderRadius", 6), 200);
    m_fadeCorners = config.readEntry("FadeCorners", true);
    m_drawInside = config.readEntry("DrawInside", true);
    m_smoothEdges = config.readEntry("SmoothEdges", true);
    m_glowEnabled = config.readEntry("GlowEnabled", false);
    m_glowRadius = qBound(1, config.readEntry("GlowRadius", 6), 200);
    m_hideOnFullScreen = config.readEntry("HideOnFullScreen", true);

    m_colorCycleEnabled = config.readEntry("ColorCycleEnabled", false);
    parseColorList(config.readEntry("ColorList", QStringLiteral("#3daee9:3.0;#2ecc71:3.0;#9b59b6:3.0")));
    m_colorTransitionDuration = qMax<qreal>(0.1, config.readEntry("ColorTransitionDuration", 2.0));
    m_colorCycleOrder = config.readEntry("ColorCycleOrder", 0);
    m_colorCycleEasing = config.readEntry("ColorCycleEasing", 0);

    m_breatheEnabled = config.readEntry("BreatheEnabled", false);
    m_breatheSpeed = qBound<qreal>(0.01, config.readEntry("BreatheSpeed", 0.5), 20.0);
    m_breatheIntensity = qBound(10, config.readEntry("BreatheIntensity", 40), 100);

    m_glitchEnabled = config.readEntry("GlitchEnabled", false);
    m_glitchSpeed = qBound<qreal>(0.1, config.readEntry("GlitchSpeed", 1.0), 5.0);
    m_glitchIntensity = qBound(10, config.readEntry("GlitchIntensity", 50), 100);
    m_glitchAnomalyEnabled = config.readEntry("GlitchAnomalyEnabled", false);
    m_glitchFrequency = qBound(1, config.readEntry("GlitchFrequency", 3), 10);

    m_neonFlickerEnabled = config.readEntry("NeonFlickerEnabled", false);
    m_neonFlickerInterval = qMax<qreal>(0.5, config.readEntry("NeonFlickerInterval", 3.0));
    m_neonFlickerDuration = qBound<qreal>(0.05, config.readEntry("NeonFlickerDuration", 0.25), 2.0);
    m_neonFlickerIntensity = qBound(10, config.readEntry("NeonFlickerIntensity", 30), 100);

    m_flashEnabled = config.readEntry("FlashEnabled", false);
    m_flashInterval = qMax<qreal>(0.5, config.readEntry("FlashInterval", 4.0));
    m_flashDuration = qBound<qreal>(0.05, config.readEntry("FlashDuration", 0.15), 1.0);

    m_customShaderEnabled = config.readEntry("CustomShaderEnabled", false);
    m_activeCustomShaderId = config.readEntry("ActiveCustomShaderId", QString());
    m_glowShader.reset();
    m_fadeCornersShader.reset();
    if (m_customShaderEnabled && !m_activeCustomShaderId.isEmpty()) {
        LeapKShader::loadShaderById(m_activeCustomShaderId, m_activeCustomShader);
    }

    m_audioShaderRuleEnabled = config.readEntry("AudioShaderRuleEnabled", false);
    m_audioShaderId = config.readEntry("AudioShaderId", QString());
    m_audioWindowFrameShaderMode = config.readEntry("AudioWindowFrameShaderMode", QStringLiteral("always"));
    m_audioHudShaderEnabled = config.readEntry("AudioHudShaderEnabled", true);
    m_badgeConfig.audioHudShaderEnabled = m_audioHudShaderEnabled;
    m_audioGlowShader.reset();
    m_audioOutlineShader.reset();
    m_audioShaderEffect = LeapKShader::CustomShaderEffect();
    if (m_audioShaderRuleEnabled && !m_audioShaderId.isEmpty()) {
        LeapKShader::loadShaderById(m_audioShaderId, m_audioShaderEffect);
    }

    m_fadeDuration = qBound(0, config.readEntry("FadeDuration", 200), 2000);
    m_menuHideDelay = qBound(0, config.readEntry("MenuHideDelay", 500), 2000);
    m_menuShowDelay = qBound(0, config.readEntry("MenuShowDelay", 500), 2000);
    m_windowHideDelay = qBound(0, config.readEntry("WindowHideDelay", 0), 2000);
    m_windowShowDelay = qBound(0, config.readEntry("WindowShowDelay", 0), 2000);
    m_highlightRecentWindow = config.readEntry("HighlightRecentWindow", false);
    m_recentWindowsCount = qBound(1, config.readEntry("RecentWindowsCount", 1), 5);
    m_recentWindowColorMode = qBound(0, config.readEntry("RecentWindowColorMode", 0), 1);
    m_recentWindowCustomColor = config.readEntry("RecentWindowCustomColor", QColor(127, 140, 141));
    m_recentWindowOpacity = qBound(5, config.readEntry("RecentWindowOpacity", 20), 100);
    m_recentWindowHueShift = qBound(-180, config.readEntry("RecentWindowHueShift", 0), 180);
    m_recentWindowSaturation = qBound(0, config.readEntry("RecentWindowSaturation", 0), 100);
    m_recentWindowWidthReduction = qBound(0, config.readEntry("RecentWindowWidthReduction", 0), 10);
    m_recentWindowDashed = config.readEntry("RecentWindowDashed", false);
    m_recentWindowGapGrowth = qBound(0, config.readEntry("RecentWindowGapGrowth", 6), 30);
    m_recentWindowDashSmooth = config.readEntry("RecentWindowDashSmooth", false);
    m_markDefaultColor = config.readEntry("MarkDefaultColor", QColor(QStringLiteral("#e74c3c")));
    m_markSmartAdaptive = config.readEntry("MarkSmartAdaptive", true);
    m_markRandomColor = config.readEntry("MarkRandomColor", false);
    m_markInactiveWidthReduction = qBound(0, config.readEntry("MarkInactiveWidthReduction", 1), 10);
    m_markInactiveDashed = config.readEntry("MarkInactiveDashed", true);
    m_markInactiveDashGap = qBound(1, config.readEntry("MarkInactiveDashGap", 6), 40);
    m_markInactiveDashSmooth = config.readEntry("MarkInactiveDashSmooth", true);
    m_stylePrecedence = OutlinePrecedence::parsePrecedence(config.readEntry("StylePrecedence", QStringLiteral("Marked,Active,Recent")));
     m_badgeDisplayMode = qBound(0, config.readEntry("BadgeDisplayMode", 0), 2);
    if (config.hasKey("BadgesAlwaysVisible") && !config.hasKey("BadgeDisplayMode")) {
        if (!config.readEntry("MarkShowLetterBadges", true)) {
            m_badgeDisplayMode = 2;
        } else if (config.readEntry("BadgesAlwaysVisible", false)) {
            m_badgeDisplayMode = 1;
        } else {
            m_badgeDisplayMode = 0;
        }
    }
    m_badgeWindowBadgesEnabled = config.readEntry("BadgeWindowBadgesEnabled", true);
    m_badgeConfig.windowBadgesEnabled = m_badgeWindowBadgesEnabled;
    m_badgePosition = qBound(0, config.readEntry("BadgePosition", 1), 8);
    m_badgeHudPosition = qBound(0, config.readEntry("BadgeHudPosition", 0), 3);
    m_badgeSize = qBound(16, config.readEntry("BadgeSize", 28), 500);
    m_badgeMargin = qBound(0, config.readEntry("BadgeMargin", 0), 500);
    m_badgeConfig.windowShowIcon = config.readEntry("BadgeWindowShowIcon", false);
    m_badgeConfig.windowShowThumbnail = config.readEntry("BadgeWindowShowThumbnail", false);
    m_badgeConfig.windowBadgeSize = m_badgeSize;
    m_badgeConfig.jumpBadgeSize = qBound(16, config.readEntry("BadgeJumpSize", 34), 500);
    m_badgeConfig.windowIconSize = qBound(12, config.readEntry("BadgeWindowIconSize", 20), 500);
    const int defWinThumb = qMax(config.readEntry("BadgeWindowThumbnailWidth", 56), config.readEntry("BadgeWindowThumbnailHeight", 32));
    const int winThumbSize = qBound(16, config.readEntry("BadgeWindowThumbnailSize", defWinThumb), 2500);
    m_badgeConfig.windowThumbnailSize = winThumbSize;
    m_badgeConfig.windowThumbnailWidth = winThumbSize;
    m_badgeConfig.windowThumbnailHeight = winThumbSize;
    m_badgeConfig.windowLetterFontSize = qBound(0, config.readEntry("BadgeWindowLetterFontSize", 0), 200);
    m_badgeConfig.windowLabelFontSize = qBound(0, config.readEntry("BadgeWindowLabelFontSize", 0), 200);

    m_badgeConfig.hudShowIcon = config.readEntry("BadgeHudShowIcon", true);
    m_badgeConfig.hudShowThumbnail = config.readEntry("BadgeHudShowThumbnail", true);
    m_badgeConfig.hudBadgeSize = qBound(24, config.readEntry("BadgeHudSize", 44), 500);
    m_badgeConfig.hudIconSize = qBound(12, config.readEntry("BadgeHudIconSize", 24), 500);
    const int defHudThumb = qMax(config.readEntry("BadgeHudThumbnailWidth", 80), config.readEntry("BadgeHudThumbnailHeight", 48));
    const int hudThumbSize = qBound(24, config.readEntry("BadgeHudThumbnailSize", defHudThumb), 3500);
    m_badgeConfig.hudThumbnailSize = hudThumbSize;
    m_badgeConfig.hudThumbnailWidth = hudThumbSize;
    m_badgeConfig.hudThumbnailHeight = hudThumbSize;
    m_badgeConfig.hudLetterFontSize = qBound(0, config.readEntry("BadgeHudLetterFontSize", 0), 200);
    m_badgeConfig.hudCaptionFontSize = qBound(0, config.readEntry("BadgeHudCaptionFontSize", 0), 200);
    m_badgeConfig.hudLocationFontSize = qBound(0, config.readEntry("BadgeHudLocationFontSize", 0), 200);
    m_badgeHudNavFontSize = qBound(0, config.readEntry("BadgeHudNavFontSize", 0), 200);
    m_badgeConfig.hudNavFontSize = m_badgeHudNavFontSize;
    m_badgeHudHintsFontSize = qBound(0, config.readEntry("BadgeHudHintsFontSize", 0), 200);
    m_badgeConfig.hudHintsFontSize = m_badgeHudHintsFontSize;
    m_badgeHudShowHints = config.readEntry("BadgeHudShowHints", true);
    m_badgeConfig.hudShowHints = m_badgeHudShowHints;
    m_nerdStatsEnabled = config.readEntry("NerdStatsEnabled", false);
    if (m_nerdStatsEnabled && !m_nerdStatsActive) {
        m_nerdStatsActive = true;
    }

    m_badgeHudIncludeAll = config.readEntry("BadgeHudIncludeAll", false);
    m_badgeConfig.hudIncludeAll = m_badgeHudIncludeAll;
    m_badgeHudGroupWindows = config.readEntry("BadgeHudGroupWindows", true);
    m_badgeConfig.hudGroupWindows = m_badgeHudGroupWindows;
    m_badgeHudGroupBorderWidth = qBound<qreal>(0.5, config.readEntry("BadgeHudGroupBorderWidth", 2.0), 20.0);
    m_badgeConfig.hudGroupBorderWidth = m_badgeHudGroupBorderWidth;
    m_badgeHudGroupFontSize = qBound(0, config.readEntry("BadgeHudGroupFontSize", 0), 200);
    m_badgeConfig.hudGroupFontSize = m_badgeHudGroupFontSize;
    m_badgeHudGroupCornerRadius = qBound(0, config.readEntry("BadgeHudGroupCornerRadius", 8), 50);
    m_badgeConfig.hudGroupCornerRadius = m_badgeHudGroupCornerRadius;
    m_badgeHudGroupShaderEnabled = config.readEntry("BadgeHudGroupShaderEnabled", true);
    m_badgeConfig.hudGroupShaderEnabled = m_badgeHudGroupShaderEnabled;
    m_badgeLivePreviews = config.readEntry("BadgeLivePreviews", false);
    m_badgeConfig.livePreviews = m_badgeLivePreviews;
    m_badgeTimeout = qBound(0, config.readEntry("BadgeTimeout", 5), 300);
    m_badgeConfig.badgeTimeout = m_badgeTimeout;
    m_jumpRecentKey = config.readEntry("JumpRecentKey", QStringLiteral("Space"));
    m_badgeConfig.jumpRecentKey = m_jumpRecentKey;

    m_badgeConfig.activeThemeId = config.readEntry("ActiveThemeId", QStringLiteral("default-breeze"));
    m_badgeConfig.activeThemeParamsJson = config.readEntry("ActiveThemeParams", QString());
    QJsonObject themeOverrides;
    if (!m_badgeConfig.activeThemeParamsJson.isEmpty()) {
        themeOverrides = QJsonDocument::fromJson(m_badgeConfig.activeThemeParamsJson.toUtf8()).object();
    }
    m_badgeConfig.activeTheme = LeapKTheme::findThemeById(m_badgeConfig.activeThemeId, themeOverrides);

    if (config.hasKey("AudioHudIndicatorStyle")) {
        m_badgeConfig.activeTheme.hudCard.audioIndicatorStyle = config.readEntry("AudioHudIndicatorStyle", m_badgeConfig.activeTheme.hudCard.audioIndicatorStyle);
    }
    if (config.hasKey("AudioHudPulseEnabled")) {
        m_badgeConfig.activeTheme.hudCard.audioPulseBadge = config.readEntry("AudioHudPulseEnabled", m_badgeConfig.activeTheme.hudCard.audioPulseBadge);
    }
    if (config.hasKey("AudioHudPulseSpeed")) {
        m_badgeConfig.activeTheme.hudCard.audioPulseSpeed = config.readEntry("AudioHudPulseSpeed", m_badgeConfig.activeTheme.hudCard.audioPulseSpeed);
    }
    if (config.hasKey("AudioHudPulseIntensity")) {
        m_badgeConfig.activeTheme.hudCard.audioPulseIntensity = config.readEntry("AudioHudPulseIntensity", m_badgeConfig.activeTheme.hudCard.audioPulseIntensity);
    }
    if (config.hasKey("AudioHudIndicatorColor") && !config.readEntry("AudioHudIndicatorColorAuto", true)) {
        m_badgeConfig.activeTheme.hudCard.audioIndicatorColor = config.readEntry("AudioHudIndicatorColor", QColor(QStringLiteral("#f1c40f"))).name();
    }

    if (config.hasKey("AudioWindowIndicatorStyle")) {
        m_badgeConfig.activeTheme.windowBadge.audioIndicatorStyle = config.readEntry("AudioWindowIndicatorStyle", m_badgeConfig.activeTheme.windowBadge.audioIndicatorStyle);
    }
    if (config.hasKey("AudioWindowPulseEnabled")) {
        m_badgeConfig.activeTheme.windowBadge.audioPulseBadge = config.readEntry("AudioWindowPulseEnabled", m_badgeConfig.activeTheme.windowBadge.audioPulseBadge);
    }
    if (config.hasKey("AudioWindowPulseSpeed")) {
        m_badgeConfig.activeTheme.windowBadge.audioPulseSpeed = config.readEntry("AudioWindowPulseSpeed", m_badgeConfig.activeTheme.windowBadge.audioPulseSpeed);
    }
    if (config.hasKey("AudioWindowPulseIntensity")) {
        m_badgeConfig.activeTheme.windowBadge.audioPulseIntensity = config.readEntry("AudioWindowPulseIntensity", m_badgeConfig.activeTheme.windowBadge.audioPulseIntensity);
    }
    if (config.hasKey("AudioWindowIndicatorScale")) {
        m_badgeConfig.activeTheme.windowBadge.audioIndicatorScale = config.readEntry("AudioWindowIndicatorScale", m_badgeConfig.activeTheme.windowBadge.audioIndicatorScale);
    }

    m_badgeConfig.iconSize = m_badgeConfig.windowIconSize;
    m_badgeConfig.thumbnailSize = m_badgeConfig.windowThumbnailSize;
    m_badgeConfig.thumbnailWidth = m_badgeConfig.windowThumbnailWidth;
    m_badgeConfig.thumbnailHeight = m_badgeConfig.windowThumbnailHeight;
    m_badgeConfig.showIcon = m_badgeConfig.windowShowIcon;
    m_badgeConfig.showThumbnail = m_badgeConfig.windowShowThumbnail;
    m_badgeShowIcon = m_badgeConfig.windowShowIcon;
    m_badgeShowThumbnail = m_badgeConfig.windowShowThumbnail;

    m_badgeConfig.margin = m_badgeMargin;
    m_badgeConfig.cornerRadius = qBound(0, config.readEntry("BadgeCornerRadius", 6), 200);
    m_badgeConfig.opacityPercent = qBound(10, config.readEntry("BadgeOpacity", 92), 100);
    m_badgeConfig.hudPosition = m_badgeHudPosition;
    m_badgeConfig.hudMarkColorMode = qBound(0, config.readEntry("BadgeHudMarkColorMode", 0), 2);
    m_badgeConfig.defaultBgColor = config.readEntry("BadgeDefaultColor", QColor(QStringLiteral("#232629")));
    m_badgeConfig.letterColorAuto = config.readEntry("BadgeLetterColorAuto", true);
    m_badgeConfig.letterColor = config.readEntry("BadgeLetterColor", QColor(QStringLiteral("#ffffff")));
    m_badgeConfig.labelColorAuto = config.readEntry("BadgeLabelColorAuto", true);
    m_badgeConfig.labelUnmarkedColor = config.readEntry("BadgeLabelUnmarkedColor", QColor(QStringLiteral("#f1c40f")));
    m_badgeConfig.captionColorAuto = config.readEntry("BadgeCaptionColorAuto", true);
    m_badgeConfig.captionColor = config.readEntry("BadgeCaptionColor", QColor(QStringLiteral("#dce1e6")));
    m_badgeConfig.locationColorAuto = config.readEntry("BadgeLocationColorAuto", true);
    m_badgeConfig.locationColor = config.readEntry("BadgeLocationColor", QColor(QStringLiteral("#f1c40f")));
    m_badgeConfig.letterFontFamily = config.readEntry("BadgeLetterFont", QString());
    m_badgeConfig.labelFontFamily = config.readEntry("BadgeLabelFont", QString());
    m_badgeConfig.captionFontFamily = config.readEntry("BadgeCaptionFont", QString());
    m_badgeConfig.locationFontFamily = config.readEntry("BadgeLocationFont", QString());
    m_badgeConfig.letterFontSize = qBound(0, config.readEntry("BadgeLetterFontSize", 0), 200);
    m_badgeConfig.labelFontSize = qBound(0, config.readEntry("BadgeLabelFontSize", 0), 200);
    m_badgeConfig.captionFontSize = qBound(0, config.readEntry("BadgeCaptionFontSize", 0), 200);
    m_badgeConfig.locationFontSize = qBound(0, config.readEntry("BadgeLocationFontSize", 0), 200);
    m_markShowLetterBadges = (m_badgeDisplayMode != 2);
    m_badgesAlwaysVisible = (m_badgeDisplayMode == 1);
    m_jumpTargets = qBound(0, config.readEntry("JumpTargets", 0), 3);
    m_superJumpCrossWsAction = qBound(0, config.readEntry("SuperJumpCrossWsAction", 0), 2);
    m_keyboardOverlayEnabled = config.readEntry("KeyboardOverlayEnabled", true);
    m_keyboardOverlayTilingOnly = config.readEntry("KeyboardOverlayTilingOnly", false);
    m_keyboardOverlayExtendToHud = config.readEntry("KeyboardOverlayExtendToHud", true);
    m_keyboardOverlayKeySize = qBound(32, config.readEntry("KeyboardOverlayKeySize", 54), 80);
    m_keyboardOverlayKeyFontSize = qBound(8, config.readEntry("KeyboardOverlayKeyFontSize", 14), 28);
    m_keyboardOverlayLineWidth = qBound(1.0, config.readEntry("KeyboardOverlayLineWidth", 3.4), 10.0);
    m_keyboardOverlayBadgeRadius = qBound(10, config.readEntry("KeyboardOverlayBadgeRadius", 18), 40);
    m_keyboardOverlayBadgeFontSize = qBound(8, config.readEntry("KeyboardOverlayBadgeFontSize", 13), 24);
    m_keyboardOverlayFontFamily = config.readEntry("KeyboardOverlayFontFamily", QString());
    m_keyboardOverlayTargetLabelFontSize = qBound(6, config.readEntry("KeyboardOverlayTargetLabelFontSize", 10), 24);
    m_keyboardOverlayStackScaleReduction = qBound(0, config.readEntry("KeyboardOverlayStackScaleReduction", 35), 70);
    m_keyboardOverlayStackOpacityReduction = qBound(0, config.readEntry("KeyboardOverlayStackOpacityReduction", 40), 70);
    m_cursorWarpMode = qBound(0, config.readEntry("CursorWarpMode", 0), 2);
    m_soundFeedbackEnabled = config.readEntry("SoundFeedbackEnabled", false);
    m_soundFeedbackVolume = qBound(0, config.readEntry("SoundFeedbackVolume", 30), 100);
    m_activeHudView = config.readEntry("ActiveHudView", 0);
    const QString stdDeskId = config.readEntry("StandardLastVirtualDesktopId", QString());
    if (!stdDeskId.isEmpty()) {
        m_workspaceLastVirtualDesktopId[QStringLiteral("__standard__")] = stdDeskId;
    }
    const QString stdPresetId = config.readEntry("StandardTilingPresetId", QString());
    if (!stdPresetId.isEmpty()) {
        for (const auto &p : m_tilingPresets) {
            if (p.id == stdPresetId) {
                m_workspaceTilingActive[QStringLiteral("__standard__")] = true;
                m_workspaceTilingPreset[QStringLiteral("__standard__")] = p;
                break;
            }
        }
    }
    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();
    if (!m_lastMarkColor.isValid()) {
        m_lastMarkColor = m_markDefaultColor;
    }
    updateRecentWindowsFromHistory();

    m_visibilityConfig.fadeDuration = m_fadeDuration;
    m_visibilityConfig.menuHideDelay = m_menuHideDelay;
    m_visibilityConfig.menuShowDelay = m_menuShowDelay;
    m_visibilityConfig.windowHideDelay = m_windowHideDelay;
    m_visibilityConfig.windowShowDelay = m_windowShowDelay;

    m_visibilityState.popupBlocked = hasVisibleBlockingPopup();
    m_visibilityState.targetOpacity = m_visibilityState.popupBlocked ? 0.0 : 1.0;
    if (m_visibilityState.targetOpacity > 0.5 && m_visibilityState.visibilityOpacity < 0.001) {
        m_visibilityState.visibilityOpacity = 1.0;
    }

    m_outlineCache.clear();
    const bool badgesAlwaysOn = (m_outlineEnabled && m_badgeConfig.windowBadgesEnabled && m_badgeDisplayMode == 1);
    if (m_audioRefreshTimer) {
        if (m_jumpMode || badgesAlwaysOn) {
            if (!m_audioRefreshTimer->isActive()) {
                m_audioRefreshTimer->start(1500);
            }
        } else {
            if (m_audioRefreshTimer->isActive()) {
                m_audioRefreshTimer->stop();
            }
        }
    }
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
    updateAnimationTimer();

    m_persistentMarks = LeapKMarks::loadPersistentMarks();
    m_tilingGaps = qBound(0, config.readEntry("TilingGaps", 6), 64);
    m_tilingMargins = qBound(0, config.readEntry("TilingMargins", 8), 64);
    m_tilingRemoveMinSizeLimit = config.readEntry("TilingRemoveMinSizeLimit", true);
    m_tilingPresets = LeapKTiling::loadPresets();
    if (m_tilingRemoveMinSizeLimit) {
        ensureTilingNoMinSizeRule();
    }
    refreshWorkspaces();
    for (EffectWindow *w : effects->stackingOrder()) {
        applyPersistentMarksToWindow(w);
    }

    effects->addRepaintFull();
    Q_EMIT stateChanged();
}

void ActiveWindowOutlineEffect::slotWindowAdded(EffectWindow *w)
{
    connect(w, &EffectWindow::windowFrameGeometryChanged,
            this, &ActiveWindowOutlineEffect::slotWindowFrameGeometryChanged);
    connect(w, &EffectWindow::minimizedChanged,
            this, &ActiveWindowOutlineEffect::slotWindowMinimizedChanged);
    connect(w, &EffectWindow::windowHiddenChanged,
            this, &ActiveWindowOutlineEffect::slotWindowHiddenChanged);
    connect(w, &EffectWindow::windowFullScreenChanged,
            this, &ActiveWindowOutlineEffect::slotWindowFullScreenChanged);
    connect(w, &EffectWindow::windowDamaged,
            this, &ActiveWindowOutlineEffect::slotWindowDamaged);
    connect(w, &EffectWindow::windowStartUserMovedResized,
            this, &ActiveWindowOutlineEffect::slotWindowStartUserMovedResized);
    connect(w, &EffectWindow::windowStepUserMovedResized,
            this, &ActiveWindowOutlineEffect::slotWindowStepUserMovedResized);
    connect(w, &EffectWindow::windowFinishUserMovedResized,
            this, &ActiveWindowOutlineEffect::slotWindowFinishUserMovedResized);

    applyPersistentMarksToWindow(w);

    if (isSystemOverlay(w)) {
        if (m_jumpMode) {
            cancelJumpMode();
        }
        m_badgeTextureCache.clear();
        m_thumbnailCache.clear();
        m_lastCaptureTimestamp.clear();
    }

    updatePopupBlockingState();
    if (m_badgeDisplayMode == 1) {
        effects->addRepaintFull();
    }

    refreshWorkspaces();
    if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
        if (isEligibleOutlineWindow(w) && !w->isSkipSwitcher() && w->acceptsFocus()) {
            if (findWorkspaceForWindow(w) == 0) {
                if (w->windowClass().isEmpty() && w->caption().isEmpty()) {
                    QPointer<EffectWindow> pw(w);
                    const int currentWsView = m_activeHudView;
                    QTimer::singleShot(60, this, [this, pw, currentWsView]() {
                        if (pw && !pw->isDeleted() && m_activeHudView == currentWsView && m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
                            if (findWorkspaceForWindow(pw.data()) == 0) {
                                addWindowToWorkspace(pw.data(), currentWsView);
                            }
                        }
                    });
                } else {
                    addWindowToWorkspace(w, m_activeHudView);
                }
            }
        }
    }

    if (m_tilingActive && !m_floatingWindows.contains(w) && isEligibleOutlineWindow(w) && !w->isSkipSwitcher() && w->acceptsFocus() && !isBlockingPopup(w)) {
        VirtualDesktop *currentDesktop = effects ? effects->currentDesktop() : nullptr;
        bool onDesk = !currentDesktop || w->isOnDesktop(currentDesktop);
        bool matchesWs = true;
        if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
            const int wWs = findWorkspaceForWindow(w);
            if (wWs != 0 && wWs != m_activeHudView) {
                matchesWs = false;
            }
        }
        if (onDesk && matchesWs && !m_stashedWindows.contains(w) && !m_workspaceMinimizedWindows.contains(w)) {
            int emptyIdx = -1;
            for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
                if (!m_activeTilingSlots[i].window || m_activeTilingSlots[i].window->isDeleted()) {
                    emptyIdx = i;
                    break;
                }
            }
            if (emptyIdx >= 0) {
                m_activeTilingSlots[emptyIdx].window = w;
                if (!m_preTilingGeometries.contains(w)) {
                    m_preTilingGeometries.insert(w, w->frameGeometry());
                }
                QPointer<EffectWindow> pw(w);
                const QRectF slotGeo = m_activeTilingSlots[emptyIdx].geometry;
                QTimer::singleShot(80, this, [this, pw, slotGeo]() {
                    if (pw && !pw->isDeleted()) {
                        applyWindowToTilingSlot(pw.data(), slotGeo);
                    }
                });
            }
        }
    }
}

bool ActiveWindowOutlineEffect::isEligibleOutlineWindow(EffectWindow *w) const
{
    if (!w || w->isDeleted()) {
        return false;
    }
    if (isSessionLockOrLogoutActive() || isSystemOverlay(w)) {
        return false;
    }
    if (w->isDesktop() || w->isDock() || w->isSpecialWindow() || w->isLockScreen()) {
        return false;
    }
    if (w->isPopupWindow() || w->isTooltip() || w->isNotification()
        || w->isCriticalNotification() || w->isOnScreenDisplay()) {
        return false;
    }
    if (effects && effects->isScreenLocked()) {
        return false;
    }
    return true;
}

QList<QPair<pid_t, QString>> ActiveWindowOutlineEffect::getKnownWindowsInfo() const
{
    QList<QPair<pid_t, QString>> list;
    if (!effects) {
        return list;
    }
    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher()) {
            continue;
        }
        list.append({w->pid(), w->caption()});
    }
    return list;
}

bool ActiveWindowOutlineEffect::shouldOutlineWindow(EffectWindow *w) const
{
    return m_outlineEnabled && w && w == m_activeWindow && isEligibleOutlineWindow(w)
        && !(m_hideOnFullScreen && w->isFullScreen())
        && !isBlockingPopup(w)
        && !isSessionLockOrLogoutActive();
}

bool ActiveWindowOutlineEffect::isBlockingPopup(EffectWindow *w) const
{
    if (!w || w->isDeleted()) {
        return false;
    }
    // Native xdg_popup menus may have WindowType::Unknown. KWin's own
    // titlebar menu is an internal window, not necessarily a semantic menu
    // either. A grab is sufficient, but not necessary: Firefox also opens
    // interactive menus without one. The fallback checks input and ownership
    // instead of treating every generic hover popup as a blocking menu.
    return w->isMenu() || w->isDropdownMenu() || w->isPopupMenu()
        || w->isComboBox() || w->isAppletPopup()
        || (w->isPopupWindow() && w->window() && w->window()->hasPopupGrab())
        || OutlinePopupFilter::isOwnedInteractivePopup(w, m_activeWindow,
                                                       popupAcceptsInput(w));
}

bool ActiveWindowOutlineEffect::popupAcceptsInput(EffectWindow *w) const
{
#ifndef ACTIVE_WINDOW_OUTLINE_X11
    if (w && !w->isDeleted() && w->isPopupWindow() && w->surface()) {
        return !w->surface()->input().isEmpty();
    }
#endif
    return false;
}

bool ActiveWindowOutlineEffect::isSystemOverlay(EffectWindow *w) const
{
    if (!w || w->isDeleted()) {
        return false;
    }
    if (w->isLockScreen()) {
        return true;
    }
    const QString cls = w->windowClass();
    if (cls.contains(QStringLiteral("ksmserver-logout-greeter"), Qt::CaseInsensitive)
        || cls.contains(QStringLiteral("logout-greeter"), Qt::CaseInsensitive)
        || cls.contains(QStringLiteral("logoutgreeter"), Qt::CaseInsensitive)
        || cls.contains(QStringLiteral("kscreenlocker"), Qt::CaseInsensitive)) {
        return true;
    }
    const QString role = w->windowRole();
    if (role.contains(QStringLiteral("logout"), Qt::CaseInsensitive)
        || role.contains(QStringLiteral("greeter"), Qt::CaseInsensitive)
        || role.contains(QStringLiteral("lockscreen"), Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

bool ActiveWindowOutlineEffect::isSessionLockOrLogoutActive() const
{
    if (!effects) {
        return false;
    }
    if (effects->isScreenLocked()) {
        return true;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (effects->sessionState() != KWin::SessionState::Normal) {
        return true;
    }
#endif
    for (EffectWindow *w : effects->stackingOrder()) {
        if (isSystemOverlay(w) && w->isVisible()) {
            return true;
        }
    }
    return false;
}

bool ActiveWindowOutlineEffect::hasVisibleBlockingPopup() const
{
    if (isSessionLockOrLogoutActive()) {
        return true;
    }
    // Query the current stacking order on every paint instead of remembering
    // popup state from windowAdded. Toolkits often create a popup before it is
    // visible, so checking only at creation time misses context menus.
    for (EffectWindow *candidate : effects->stackingOrder()) {
        if (isBlockingPopup(candidate) && !candidate->isDeleted()
            && candidate->isVisible() && !candidate->isMinimized()
            && candidate->isOnCurrentDesktop()
            && !candidate->isHiddenByShowDesktop()) {
            return true;
        }
    }
    return false;
}

void ActiveWindowOutlineEffect::slotWindowDeleted(EffectWindow *w)
{
    if (m_delayedHideWindow == w) {
        m_delayedHideWindow = nullptr;
        m_delayedHideState.reset();
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
    }
    if (m_activeWindow == w) {
        m_activeWindow = nullptr;
        if (m_windowShowTimer) {
            m_windowShowTimer->stop();
        }
    }
    if (m_hudOriginWindow == w) {
        m_hudOriginWindow.clear();
    }
    if (m_hudPrevActiveWindow == w) {
        m_hudPrevActiveWindow.clear();
    }
    m_recentWindows.removeAll(w);
    m_windowHistory.removeAll(w);
    m_hudSnapshotHistory.removeAll(w);
    m_recentAmbilightColors.remove(w);
    m_recentCycleColors.remove(w);
    m_smartAccentCache.remove(w);
    m_floatingWindows.remove(w);
    m_thumbnailCache.remove(w);
    m_lastCaptureTimestamp.remove(w);
    m_markedWindows.remove(w);
    m_markedWindowLetters.remove(w);
    m_markedWindowLabels.remove(w);
    m_closingWindows.remove(w);
    m_workspaceMinimizedWindows.remove(w);
    m_stashedWindows.remove(w);
    m_pinnedWindows.remove(w);
    m_windowPinnedWorkspaces.remove(w);
    for (auto it = m_jumpLetterToWindow.begin(); it != m_jumpLetterToWindow.end(); ) {
        if (it.value() == w) {
            it = m_jumpLetterToWindow.erase(it);
        } else {
            ++it;
        }
    }
    m_suppressedWindows.remove(w);
    m_lastGeometry.remove(w);
    m_outlineCache.remove(w);

    for (auto &slot : m_activeTilingSlots) {
        if (slot.window == w) {
            slot.window = nullptr;
        }
    }
    m_preTilingGeometries.remove(w);

    updatePopupBlockingState();
    updateRecentWindowsFromHistory();
    repaintAllOutlines();
}

void ActiveWindowOutlineEffect::slotWindowHiddenChanged(EffectWindow *w)
{
    Q_UNUSED(w);
    updatePopupBlockingState();
    if (m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
    for (EffectWindow *recent : m_recentWindows) {
        if (recent) {
            effects->addRepaint(expandedGeometryFor(recent));
        }
    }
    for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
        if (it.key()) {
            effects->addRepaint(expandedGeometryFor(it.key()));
        }
    }
}

void ActiveWindowOutlineEffect::updatePopupBlockingState()
{
    const bool currentlyHasPopup = hasVisibleBlockingPopup();
    const auto action = m_visibilityState.handlePopupChange(currentlyHasPopup, m_visibilityConfig);

    switch (action) {
    case OutlineVisibility::StateMachine::TimerAction::StartHideTimer:
        if (m_menuHideTimer) {
            m_menuHideTimer->start(m_visibilityConfig.menuHideDelay);
        }
        break;
    case OutlineVisibility::StateMachine::TimerAction::StartShowTimer:
        if (m_menuShowTimer) {
            m_menuShowTimer->start(m_visibilityConfig.menuShowDelay);
        }
        break;
    case OutlineVisibility::StateMachine::TimerAction::CancelHideTimer:
        if (m_menuHideTimer) {
            m_menuHideTimer->stop();
        }
        break;
    case OutlineVisibility::StateMachine::TimerAction::CancelShowTimer:
        if (m_menuShowTimer) {
            m_menuShowTimer->stop();
        }
        break;
    case OutlineVisibility::StateMachine::TimerAction::None:
        break;
    }

    updateAnimationTimer();
    if (m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
}

void ActiveWindowOutlineEffect::repaintAllOutlines()
{
    if (effects && effects->isScreenLocked()) {
        return;
    }
    if (m_badgeDisplayMode == 1 || m_jumpMode) {
        effects->addRepaintFull();
    }
    if (m_delayedHideWindow && !m_delayedHideWindow->isDeleted()
        && m_delayedHideWindow->isOnCurrentDesktop() && !m_delayedHideWindow->isMinimized()) {
        effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
    }
    for (EffectWindow *recent : m_recentWindows) {
        if (recent && !recent->isDeleted()
            && recent->isOnCurrentDesktop() && !recent->isMinimized()) {
            effects->addRepaint(expandedGeometryFor(recent));
        }
    }
    for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
        EffectWindow *marked = it.key();
        if (marked && !marked->isDeleted()
            && marked->isOnCurrentDesktop() && !marked->isMinimized()) {
            effects->addRepaint(expandedGeometryFor(marked));
        }
    }
    if (m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
}

OutlineRegion ActiveWindowOutlineEffect::panelOcclusionRegion(EffectWindow *w,
                                                             QString *source) const
{
    const auto label = [source](const char *value) {
        if (source) {
            *source = QString::fromLatin1(value);
        }
    };
#ifndef ACTIVE_WINDOW_OUTLINE_X11
    if (SurfaceInterface *surface = w->surface()) {
        OutlineRegion region;
        OutlineRegion input;
        // These are surface-local coordinates, not decoration/frame-local.
        const QPointF origin = w->bufferGeometry().topLeft();
#ifdef ACTIVE_WINDOW_OUTLINE_SURFACE_BLUR_REGION
        region = surface->blurRegion().translated(origin).roundedOut();
        input = surface->input().translated(origin).roundedOut();
#else
        if (BlurInterface *blur = surface->blur()) {
            region = blur->region();
        }
        if (region.isEmpty()) {
            if (ContrastInterface *contrast = surface->contrast()) {
                region = contrast->region();
            }
        }
        region.translate(origin.toPoint());
        input = surface->input().translated(origin.toPoint());
#endif
        // Clamp protocol regions (which can cover the entire coordinate
        // space) before converting them to render-target coordinates.
        const OutlineRegion bounds(QRegion(QRectF(w->bufferGeometry()).toAlignedRect()));
        region &= bounds;
        input &= bounds;
        if (!region.isEmpty()) {
            label("background-mask");
            return region;
        }
        if (!input.isEmpty()) {
            // Plasma updates this even when its theme disables blur. The
            // desktop-facing edge follows the animated panel background.
            // The screen-facing gap is included for Fitts's law, so this is
            // a conservative fallback, not an exact visual/alpha mask.
            label("input-fallback");
            return input;
        }
    }
#endif
    label("frame-fallback");
    return OutlineRegion(QRegion(QRectF(w->frameGeometry()).toAlignedRect()));
}

void ActiveWindowOutlineEffect::slotWindowActivated(EffectWindow *w)
{
    if (isBlockingPopup(w) || isSystemOverlay(w)) {
        if (isSystemOverlay(w) && m_jumpMode) {
            cancelJumpMode();
        }
        return;
    }

    if (m_jumpMode) {
        if (w != m_activeWindow) {
            m_hudPrevActiveWindow = m_activeWindow;
            m_hudPrevMarkerRect = m_hudLastDrawnMarkerRect.isValid() ? m_hudLastDrawnMarkerRect : m_hudCurrentMarkerRect;
            m_hudPrevActiveDockIndex = m_hudCurrentActiveDockIndex;
            m_hudSelectAnimTimer.restart();
            m_hudSelectAnimActive = true;
            updateAnimationTimer();
        }
        m_activeWindow = w;
        repaintAllOutlines();
        return;
    }

    if (m_expectedJumpTarget) {
        if (!w || w->isDeleted()) {
            return;
        }
        if (w != m_expectedJumpTarget) {
            // Intermediate window activation caused by virtual desktop switch before target window is activated. Ignore!
            return;
        }
        m_expectedJumpTarget = nullptr;
        if (m_expectedJumpTargetTimer && m_expectedJumpTargetTimer->isActive()) {
            m_expectedJumpTargetTimer->stop();
        }
    }

    if (m_isHistoryNavigating) {
        if (!w || w->isDeleted()) {
            return;
        }
        if (m_historyNavIndex >= 0 && m_historyNavIndex < m_windowHistory.size()
            && w == m_windowHistory.at(m_historyNavIndex)) {
            m_activeWindow = w;
            updateRecentWindowsFromHistory();
            repaintAllOutlines();
            return;
        }
        if ((m_historyCommitTimer && m_historyCommitTimer->isActive()) || m_metaSpaceTraverseActive) {
            return;
        }
        m_isHistoryNavigating = false;
    }

    EffectWindow *previous = m_activeWindow;
    if (previous == w) {
        return;
    }

    if (m_ambilightEnabled || m_ensurePerimeterContrast) {
        m_ambilightSampleNeeded = true;
    }

    if (m_windowShowTimer && m_windowShowTimer->isActive()) {
        m_windowShowTimer->stop();
    }

    if (previous && isEligibleOutlineWindow(previous)
        && !(m_hideOnFullScreen && previous->isFullScreen()) && !isBlockingPopup(previous)) {
        m_windowHistory.removeAll(previous);
        m_windowHistory.prepend(previous);
        if (m_highlightRecentWindow) {
            if (m_ambilightCurrentColor.isValid()) {
                m_recentAmbilightColors[previous] = m_ambilightCurrentColor;
            }
            if (m_colorCycleEnabled && !m_colorStops.isEmpty()) {
                const qreal elapsedSec = m_animationClock.isValid() ? (m_animationClock.elapsed() / 1000.0) : 0.0;
                m_recentCycleColors[previous] = currentCycleColor(elapsedSec);
            }
        }
    }

    if (w && isEligibleOutlineWindow(w)) {
        m_windowHistory.removeAll(w);
        m_windowHistory.prepend(w);
        m_recentAmbilightColors.remove(w);
        m_recentCycleColors.remove(w);
        m_activeWindow = w;
    } else {
        m_activeWindow = nullptr;
    }

    m_historyNavIndex = 0;
    while (m_windowHistory.size() > 50) {
        m_windowHistory.removeLast();
    }
    updateRecentWindowsFromHistory();

    if (m_highlightRecentWindow) {
        if (previous && isEligibleOutlineWindow(previous)) {
            effects->addRepaint(expandedGeometryFor(previous));
            m_outlineCache.remove(previous);
            m_lastGeometry.remove(previous);
        }
        for (EffectWindow *recent : m_recentWindows) {
            if (recent) {
                effects->addRepaint(expandedGeometryFor(recent));
            }
        }

        if (m_delayedHideWindow) {
            if (!m_recentWindows.contains(m_delayedHideWindow) && m_delayedHideWindow != w) {
                effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
                m_outlineCache.remove(m_delayedHideWindow);
                m_lastGeometry.remove(m_delayedHideWindow);
            }
            m_delayedHideWindow = nullptr;
            m_delayedHideState.reset();
            if (m_windowHideTimer) {
                m_windowHideTimer->stop();
            }
        }
    } else {
        if (previous && isEligibleOutlineWindow(previous)) {
            if (m_markedWindows.contains(previous)) {
                m_outlineCache.remove(previous);
                m_lastGeometry.remove(previous);
                effects->addRepaint(expandedGeometryFor(previous));
            } else {
                const bool willDelayOrFade = (!previous->isDeleted() && previous->isOnCurrentDesktop())
                    && (m_windowHideDelay > 0 || m_visibilityConfig.fadeDuration > 0);

                if (willDelayOrFade) {
                    if (m_delayedHideWindow && m_delayedHideWindow != previous) {
                        effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
                        m_suppressedWindows.remove(m_delayedHideWindow);
                        m_lastGeometry.remove(m_delayedHideWindow);
                        m_outlineCache.remove(m_delayedHideWindow);
                        m_delayedHideWindow = nullptr;
                        m_delayedHideState.reset();
                    }
                    m_delayedHideWindow = previous;
                    effects->addRepaint(expandedGeometryFor(previous));

                    if (m_windowHideDelay > 0) {
                        m_delayedHideState.startBuffer();
                        if (m_windowHideTimer) {
                            m_windowHideTimer->start(m_windowHideDelay);
                        }
                    } else {
                        m_delayedHideState.startFade(m_visibilityConfig.fadeDuration);
                        updateAnimationTimer();
                    }
                } else {
                    effects->addRepaint(expandedGeometryFor(previous));
                    m_suppressedWindows.remove(previous);
                    m_lastGeometry.remove(previous);
                    m_outlineCache.remove(previous);
                    if (m_delayedHideWindow == previous) {
                        m_delayedHideWindow = nullptr;
                        m_delayedHideState.reset();
                        if (m_windowHideTimer) {
                            m_windowHideTimer->stop();
                        }
                    }
                }
            }
        }
    }

    if (w) {
        m_outlineCache.remove(w);
        m_lastGeometry.remove(w);
        if (m_markedWindows.contains(w)) {
            effects->addRepaint(expandedGeometryFor(w));
        }
    }

    if (!m_activeWindow) {
        m_visibilityState.setTargetOpacity(0.0, m_visibilityConfig.fadeDuration);
        updateAnimationTimer();
        return;
    }

    if (m_delayedHideWindow == w) {
        m_delayedHideWindow = nullptr;
        m_delayedHideState.reset();
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
    }

    // If a popup menu is currently active or buffering, let popup timers manage outline visibility
    if (hasVisibleBlockingPopup() || m_visibilityState.popupBlocked
        || m_visibilityState.menuShowTimerActive || (m_menuShowTimer && m_menuShowTimer->isActive())) {
        return;
    }

    if (shouldOutlineWindow(w)) {
        if (m_windowShowDelay > 0) {
            if (m_visibilityConfig.fadeDuration > 0) {
                m_visibilityState.visibilityOpacity = 0.0;
            }
            m_visibilityState.targetOpacity = 0.0;
            if (m_windowShowTimer) {
                m_windowShowTimer->start(m_windowShowDelay);
            }
        } else {
            if (m_visibilityConfig.fadeDuration > 0) {
                m_visibilityState.visibilityOpacity = 0.0;
            }
            m_visibilityState.setTargetOpacity(1.0, m_visibilityConfig.fadeDuration);
            updateAnimationTimer();
            effects->addRepaint(expandedGeometryFor(w));
        }
    }

    if (m_badgeDisplayMode == 1) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotWindowFrameGeometryChanged(EffectWindow *w,
                                                            const QRectF &oldGeometry)
{
    if (w && !m_markedWindows.contains(w) && !m_persistentMarks.isEmpty()) {
        applyPersistentMarksToWindow(w);
    }

    if (m_activeWindow && w != m_activeWindow) {
        const QRectF activeGeo = expandedGeometryFor(m_activeWindow);
        if (oldGeometry.intersects(activeGeo) || w->frameGeometry().intersects(activeGeo)) {
            effects->addRepaint(activeGeo);
        }
    }
    const bool isRecent = (m_highlightRecentWindow && m_recentWindows.contains(w));
    const bool isMarked = m_markedWindows.contains(w);
    const bool hasContinuousBadges = (m_badgeDisplayMode == 1) && (w != m_activeWindow) && !w->isDeleted() && w->isOnCurrentDesktop();
    if (!shouldOutlineWindow(w) && !isBlockingPopup(w) && m_delayedHideWindow != w && !isRecent && !isMarked && !hasContinuousBadges) {
        return;
    }
    const bool hasGlow = (w == m_activeWindow) ? (m_glowEnabled || m_glitchEnabled) : m_glowEnabled;
    const qreal glowExtent = hasGlow ? m_glowRadius : 0;
    const qreal extent = m_drawInside
        ? glowExtent + 1.0
        : m_width + m_gap + glowExtent + 1.0;
    effects->addRepaint(oldGeometry.adjusted(-extent, -extent, extent, extent));
    effects->addRepaint(expandedGeometryFor(w));
    if (w == m_activeWindow && (m_ambilightEnabled || m_ensurePerimeterContrast)) {
        m_ambilightSampleNeeded = true;
    }
    if (isBlockingPopup(w)) {
        updatePopupBlockingState();
    }
}

void ActiveWindowOutlineEffect::slotToggleAction()
{
    m_outlineEnabled = !m_outlineEnabled;
    KConfigGroup config = effects->config()->group(QStringLiteral("Effect-leapk"));
    config.writeEntry("OutlineEnabled", m_outlineEnabled);
    config.sync();

    if (m_outlineEnabled) {
        if (m_visibilityConfig.fadeDuration > 0) {
            m_visibilityState.visibilityOpacity = 0.0;
        }
        m_visibilityState.targetOpacity = hasVisibleBlockingPopup() ? 0.0 : 1.0;
        updateAnimationTimer();
        repaintAllOutlines();
        effects->addRepaintFull();
    } else {
        if (m_jumpMode) {
            cancelJumpMode();
        }
        m_visibilityState.targetOpacity = 0.0;
        m_visibilityState.visibilityOpacity = 0.0;
        updateAnimationTimer();
        if (m_windowShowTimer) {
            m_windowShowTimer->stop();
        }
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
        if (m_menuShowTimer) {
            m_menuShowTimer->stop();
        }
        if (m_menuHideTimer) {
            m_menuHideTimer->stop();
        }
        if (m_delayedHideWindow) {
            effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
            m_suppressedWindows.remove(m_delayedHideWindow);
            m_lastGeometry.remove(m_delayedHideWindow);
            m_outlineCache.remove(m_delayedHideWindow);
            m_delayedHideWindow = nullptr;
        }
        m_delayedHideState.reset();
        m_outlineCache.clear();
        m_badgeTextureCache.clear();
        m_thumbnailCache.clear();
        m_lastCaptureTimestamp.clear();
        repaintAllOutlines();
        effects->addRepaintFull();
    }
    Q_EMIT stateChanged();
}

void ActiveWindowOutlineEffect::toggleOutline()
{
    slotToggleAction();
}

void ActiveWindowOutlineEffect::setOutlineEnabled(bool enabled)
{
    if (m_outlineEnabled != enabled) {
        slotToggleAction();
    }
}

bool ActiveWindowOutlineEffect::isOutlineEnabled() const
{
    return m_outlineEnabled;
}

void ActiveWindowOutlineEffect::toggleAmbilight()
{
    KConfigGroup config = effects->config()->group(QStringLiteral("Effect-leapk"));
    const bool current = config.readEntry("AmbilightEnabled", false);
    config.writeEntry("AmbilightEnabled", !current);
    config.sync();
    reconfigure(ReconfigureAll);
    Q_EMIT stateChanged();
}

bool ActiveWindowOutlineEffect::isAmbilightEnabled() const
{
    return m_ambilightEnabled;
}

void ActiveWindowOutlineEffect::toggleGlow()
{
    KConfigGroup config = effects->config()->group(QStringLiteral("Effect-leapk"));
    const bool current = config.readEntry("GlowEnabled", false);
    config.writeEntry("GlowEnabled", !current);
    config.sync();
    reconfigure(ReconfigureAll);
    Q_EMIT stateChanged();
}

bool ActiveWindowOutlineEffect::isGlowEnabled() const
{
    return m_glowEnabled;
}

void ActiveWindowOutlineEffect::triggerJumpMode()
{
    slotTriggerJumpMode();
}

void ActiveWindowOutlineEffect::toggleWindowMark()
{
    slotMarkWindowToggle();
}

void ActiveWindowOutlineEffect::markWindowWithColor()
{
    slotMarkWindowWithColor();
}

void ActiveWindowOutlineEffect::openWorkspaceManager()
{
    slotOpenWorkspaceManager();
}

void ActiveWindowOutlineEffect::openMarkerManager()
{
    slotOpenMarkerManager();
}

void ActiveWindowOutlineEffect::clearAllMarks()
{
    m_markedWindows.clear();
    m_markedWindowLetters.clear();
    m_markedWindowLabels.clear();
    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();
    repaintAllOutlines();
    effects->addRepaintFull();
    Q_EMIT stateChanged();
}

void ActiveWindowOutlineEffect::prevRecentWindow()
{
    slotPrevRecentWindow();
}

void ActiveWindowOutlineEffect::nextRecentWindow()
{
    slotNextRecentWindow();
}

void ActiveWindowOutlineEffect::applyPreset(const QString &presetName)
{
    const auto presets = OutlinePreset::loadPresets(OutlinePreset::defaultPresetsDirectory());
    for (const auto &p : presets) {
        if (p.name == presetName) {
            // Apply embedded shader bundle if available
            if (!p.shader.isEmpty()) {
                LeapKShader::applyShaderBundle(p.shader);
            } else if (p.settings.contains(QStringLiteral("ShaderEffect"))) {
                LeapKShader::applyShaderBundle(p.settings.value(QStringLiteral("ShaderEffect")).toObject());
            }

            KConfigGroup group = effects->config()->group(QStringLiteral("Effect-leapk"));
            for (auto it = p.settings.constBegin(); it != p.settings.constEnd(); ++it) {
                if (it.key() == QStringLiteral("ShaderEffect")) continue;
                group.writeEntry(it.key(), it.value().toVariant());
            }
            group.sync();
            reconfigure(ReconfigureAll);
            Q_EMIT stateChanged();
            break;
        }
    }
}

void ActiveWindowOutlineEffect::slotWindowStartUserMovedResized(EffectWindow *w)
{
    if (m_tilingActive && w && w->isUserMove()) {
        deactivateTiling(w);
    }
}

void ActiveWindowOutlineEffect::slotWindowStepUserMovedResized(EffectWindow *w, const QRectF &geometry)
{
    if (m_tilingActive && w && w->isUserResize()) {
        handleTiledWindowResized(w, geometry);
    }
}

void ActiveWindowOutlineEffect::slotWindowFinishUserMovedResized(EffectWindow *w)
{
    if (m_tilingActive && w && w->isUserResize()) {
        handleTiledWindowResized(w, w->frameGeometry());
        syncActiveTilingSlotsToKdeTiles();
    }
}

void ActiveWindowOutlineEffect::slotWindowMinimizedChanged(EffectWindow *w)
{
    if (w && !w->isMinimized()) {
        m_workspaceMinimizedWindows.remove(w);
    }
    if (m_delayedHideWindow == w) {
        m_delayedHideWindow = nullptr;
        m_delayedHideState.reset();
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (m_recentWindows.contains(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (m_markedWindows.contains(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (shouldOutlineWindow(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    } else if (m_activeWindow && w != m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
}

void ActiveWindowOutlineEffect::slotWindowFullScreenChanged(EffectWindow *w)
{
    if (m_delayedHideWindow == w) {
        m_delayedHideWindow = nullptr;
        m_delayedHideState.reset();
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (m_recentWindows.contains(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (m_markedWindows.contains(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (w && w == m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(w));
    }
}

void ActiveWindowOutlineEffect::slotWindowDamaged(EffectWindow *w)
{
    if (!w || w->isDeleted()) {
        return;
    }
    const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    const bool wantThumbnail = (cfg.windowShowThumbnail || cfg.hudShowThumbnail)
        || (m_badgePreviewActive && (m_previewBadgeConfig.windowShowThumbnail || m_previewBadgeConfig.hudShowThumbnail));
    if (!wantThumbnail) {
        return;
    }
    m_thumbnailCache.remove(w);
    m_lastCaptureTimestamp.remove(w);
    const bool showContinuousBadges = m_outlineEnabled && (m_badgeDisplayMode == 1) && !hasVisibleBlockingPopup() && !m_visibilityState.popupBlocked;
    const bool hasBadgesToDraw = (showContinuousBadges || (m_outlineEnabled && m_jumpMode) || m_badgePreviewActive);
    if (hasBadgesToDraw) {
        m_badgeTextureCache.clear();
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotShowingDesktopChanged()
{
    // KWin's show-desktop animation does not minimize windows: it marks them
    // hiddenByShowDesktop and transforms/fades them in a separate effect. The
    // outline is painted at screen level, so explicitly damage its old band on
    // both transitions instead of leaving it above the desktop.
    repaintAllOutlines();
}

void ActiveWindowOutlineEffect::slotActiveFullScreenEffectChanged()
{
    // Fullscreen interactive effects (Overview, Present Windows, Desktop Grid)
    // arrange window thumbnails in a transformed QML canvas. Damage old bands
    // so outlines disappear cleanly upon entering and reappear when exiting.
    m_suppressedWindows.clear();
    repaintAllOutlines();
}

bool ActiveWindowOutlineEffect::isActive() const
{
    if (!m_outlineEnabled) {
        return false;
    }
    if (m_jumpMode) {
        return true;
    }
    if (effects->hasActiveFullScreenEffect()) {
        return false;
    }
    if (m_badgeDisplayMode == 1) {
        return true;
    }
    if (m_badgeConfig.windowShowThumbnail || m_badgeConfig.hudShowThumbnail || m_badgeShowThumbnail) {
        return true;
    }
    const bool showPending = (m_windowShowTimer && m_windowShowTimer->isActive());
    const bool activeEligible = shouldOutlineWindow(m_activeWindow) && m_visibilityState.isEffectivelyVisible();
    const bool delayedEligible = (m_delayedHideWindow && !m_delayedHideWindow->isDeleted()
        && !m_delayedHideWindow->isMinimized() && !m_delayedHideWindow->isHiddenByShowDesktop()
        && m_delayedHideWindow->isOnCurrentDesktop() && m_delayedHideState.isEffectivelyVisible());
    bool recentEligible = false;
    if (m_highlightRecentWindow) {
        for (EffectWindow *recent : m_recentWindows) {
            if (recent && !recent->isDeleted() && !recent->isMinimized()
                && !recent->isHiddenByShowDesktop() && recent->isOnCurrentDesktop()) {
                recentEligible = true;
                break;
            }
        }
    }
    bool markedEligible = false;
    for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
        EffectWindow *marked = it.key();
        if (marked && !marked->isDeleted() && !marked->isMinimized()
            && !marked->isHiddenByShowDesktop() && marked->isOnCurrentDesktop()) {
            markedEligible = true;
            break;
        }
    }
    return m_outlineEnabled && (markedEligible || recentEligible || OutlineVisibility::StateMachine::isEffectActive(m_outlineEnabled, showPending, activeEligible, delayedEligible));
}

QString ActiveWindowOutlineEffect::debug(const QString &parameter) const
{
    QString result = QStringLiteral("version=" ACTIVE_WINDOW_OUTLINE_VERSION
                                   " build=" ACTIVE_WINDOW_OUTLINE_BUILD_ID
                                   " kwinHeaders=" ACTIVE_WINDOW_OUTLINE_KWIN_VERSION "\n");
    result += QStringLiteral("width=%1 offset=%2 radius=%3 drawInside=%4 smoothEdges=%5 glow=%6 glowRadius=%7 accentOpacity=%8"
                             " fadeDuration=%9 menuHideDelay=%10 menuShowDelay=%11 windowHideDelay=%12 windowShowDelay=%13 fadeCorners=%14")
        .arg(m_width)
        .arg(m_gap)
        .arg(m_radius)
        .arg(m_drawInside ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_smoothEdges ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_glowEnabled ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_glowRadius)
        .arg(m_accentOpacity)
        .arg(m_fadeDuration)
        .arg(m_menuHideDelay)
        .arg(m_menuShowDelay)
        .arg(m_windowHideDelay)
        .arg(m_windowShowDelay)
        .arg(m_fadeCorners ? QStringLiteral("true") : QStringLiteral("false"));
    if (parameter != QStringLiteral("windows")) {
        return result;
    }
    const auto rectText = [](const QRectF &rect) {
        return QStringLiteral("[%1,%2 %3x%4]")
            .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
    };
    result += QStringLiteral("\nblockingPopup=%1 popupBlocked=%2 visibilityOpacity=%3 targetOpacity=%4 delayedHideWindow=%5 delayedHideOpacity=%6 showTimerActive=%7 recentCount=%8 recentOpacity=%9 markedCount=%10 effectActive=%11")
        .arg(hasVisibleBlockingPopup())
        .arg(m_visibilityState.popupBlocked)
        .arg(m_visibilityState.visibilityOpacity)
        .arg(m_visibilityState.targetOpacity)
        .arg(m_delayedHideWindow != nullptr)
        .arg(m_delayedHideState.opacity)
        .arg(m_windowShowTimer && m_windowShowTimer->isActive())
        .arg(m_recentWindows.size())
        .arg(m_recentWindowOpacity)
        .arg(m_markedWindows.size())
        .arg(isActive());
    int index = 0;
    // Intentionally omit window titles, application IDs and document names.
    // Indexes describe the current stacking order, not persistent identities.
    for (EffectWindow *w : effects->stackingOrder()) {
        result += QStringLiteral("\nwindow=%1 active=%2 deleted=%3 visible=%4 dock=%5 popup=%6 tooltip=%7 grab=%8 blocking=%9")
            .arg(index++).arg(w == m_activeWindow).arg(w->isDeleted())
            .arg(w->isVisible()).arg(w->isDock()).arg(w->isPopupWindow())
            .arg(w->isTooltip())
            .arg(!w->isDeleted() && w->window() && w->window()->hasPopupGrab())
            .arg(isBlockingPopup(w));
        result += QStringLiteral(" internal=%1 frame=%2 buffer=%3")
            .arg(w->internalWindow() != nullptr)
            .arg(rectText(w->frameGeometry()), rectText(w->bufferGeometry()));
        if (w->isPopupWindow() && !w->isDeleted()) {
            const bool acceptsInput = popupAcceptsInput(w);
            result += QStringLiteral(" popupInput=%1 ownedInteractive=%2")
                .arg(acceptsInput)
                .arg(OutlinePopupFilter::isOwnedInteractivePopup(w, m_activeWindow,
                                                                acceptsInput));
        }
        if (w->isDock() && !w->isDeleted()) {
            QString source;
            const auto region = panelOcclusionRegion(w, &source);
            result += QStringLiteral(" clipSource=%1 clipBounds=%2")
                .arg(source, rectText(QRectF(region.boundingRect())));
        }
    }
    return result + QLatin1Char('\n');
}


} // namespace KWin

#include "leapk.moc"
