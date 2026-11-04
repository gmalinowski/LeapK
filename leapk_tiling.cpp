/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk_internal.h"

namespace KWin
{

void ActiveWindowOutlineEffect::slotOpenKdeTileEditor()
{
    if (m_tilingEditModeActive) {
        exitTilingEditMode();
    }
    if (m_tilingPresetsModeActive) {
        exitTilingPresetsMode();
    }
    if (m_jumpMode) {
        cancelJumpMode();
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/component/kwin"),
        QStringLiteral("org.kde.kglobalaccel.Component"),
        QStringLiteral("invokeShortcut"));
    msg << QStringLiteral("Edit Tiles");
    QDBusConnection::sessionBus().send(msg);
}

void ActiveWindowOutlineEffect::refreshTilingPresetsWithKdeCustom()
{
    m_tilingPresets = LeapKTiling::loadPresets();
    m_tilingPresets.removeIf([](const LeapKTiling::TilingPreset &p) {
        return p.id == QStringLiteral("kde-custom");
    });

    LogicalOutput *activeOutput = effects ? effects->activeScreen() : nullptr;
    VirtualDesktop *currentDesktop = effects ? effects->currentDesktop() : nullptr;
    TileManager *tileMgr = (workspace() && activeOutput) ? workspace()->tileManager(activeOutput) : nullptr;
    RootTile *rootTile = (tileMgr && currentDesktop) ? tileMgr->rootTile(currentDesktop) : nullptr;

    const QString outUuid = activeOutput ? activeOutput->uuid() : QString();
    const QString deskId = currentDesktop ? currentDesktop->id() : QString();

    LeapKTiling::TilingPreset kdePreset;

    const bool isLeapkTilingWithOtherPreset = m_tilingActive && (m_activeTilingPreset.id != QStringLiteral("kde-custom"));

    if (isLeapkTilingWithOtherPreset) {
        QJsonObject backupJson = m_savedKdeCustomTilesJson;
        if (backupJson.isEmpty() && effects && effects->config() && !deskId.isEmpty() && !outUuid.isEmpty()) {
            KConfigGroup leapkCg = effects->config()->group(QStringLiteral("Effect-leapk"));
            const QString backupStr = leapkCg.readEntry(QStringLiteral("CustomTilingBackup_%1_%2").arg(deskId, outUuid), QString());
            if (!backupStr.isEmpty()) {
                backupJson = QJsonDocument::fromJson(backupStr.toUtf8()).object();
            }
        }
        if (!backupJson.isEmpty()) {
            kdePreset = LeapKTiling::createKdeCustomPreset(backupJson);
        }
    } else {
        // Priority 1: live rootTile leaf tiles
        QList<QRectF> liveRects;
        if (rootTile) {
            for (Tile *t : rootTile->descendants()) {
                if (!t->isLayout()) {
                    const QRectF rel = t->relativeGeometry();
                    if (rel.isValid() && rel.width() > 0.02 && rel.height() > 0.02) {
                        liveRects.append(rel);
                    }
                }
            }
        }
        if (!liveRects.isEmpty()) {
            std::stable_sort(liveRects.begin(), liveRects.end(), [](const QRectF &a, const QRectF &b) {
                if (std::abs(a.top() - b.top()) > 0.04) {
                    return a.top() < b.top();
                }
                return a.left() < b.left();
            });
            kdePreset = LeapKTiling::createKdeCustomPreset(liveRects);
        }

        // Priority 2: kwinrc [Tiling]
        if (kdePreset.tileSlots.isEmpty()) {
            QJsonObject customTileJson;
            if (effects && effects->config()) {
                effects->config()->reparseConfiguration();
                KConfigGroup tilingCg = effects->config()->group(QStringLiteral("Tiling"));
                QString tilesStr;
                if (!deskId.isEmpty() && !outUuid.isEmpty() && tilingCg.hasGroup(deskId)) {
                    KConfigGroup deskGroup(&tilingCg, deskId);
                    if (deskGroup.hasGroup(outUuid)) {
                        tilesStr = deskGroup.group(outUuid).readEntry("tiles", QString());
                    }
                }
                if (tilesStr.isEmpty() && !outUuid.isEmpty() && !deskId.isEmpty() && tilingCg.hasGroup(outUuid)) {
                    KConfigGroup outGroup(&tilingCg, outUuid);
                    if (outGroup.hasGroup(deskId)) {
                        tilesStr = outGroup.group(deskId).readEntry("tiles", QString());
                    }
                }
                if (tilesStr.isEmpty() && !outUuid.isEmpty() && tilingCg.hasGroup(outUuid)) {
                    tilesStr = tilingCg.group(outUuid).readEntry("tiles", QString());
                }
                if (!tilesStr.isEmpty()) {
                    customTileJson = QJsonDocument::fromJson(tilesStr.toUtf8()).object();
                }
            }
            if (!customTileJson.isEmpty()) {
                kdePreset = LeapKTiling::createKdeCustomPreset(customTileJson);
            }
        }

        // Priority 3: in-memory or persisted backup
        if (kdePreset.tileSlots.isEmpty()) {
            if (!m_savedKdeCustomTilesJson.isEmpty()) {
                kdePreset = LeapKTiling::createKdeCustomPreset(m_savedKdeCustomTilesJson);
            } else if (effects && effects->config() && !deskId.isEmpty() && !outUuid.isEmpty()) {
                KConfigGroup leapkCg = effects->config()->group(QStringLiteral("Effect-leapk"));
                const QString backupStr = leapkCg.readEntry(QStringLiteral("CustomTilingBackup_%1_%2").arg(deskId, outUuid), QString());
                if (!backupStr.isEmpty()) {
                    QJsonObject bObj = QJsonDocument::fromJson(backupStr.toUtf8()).object();
                    if (!bObj.isEmpty()) {
                        kdePreset = LeapKTiling::createKdeCustomPreset(bObj);
                    }
                }
            }
        }
    }

    if (kdePreset.tileSlots.isEmpty()) {
        kdePreset = LeapKTiling::createKdeCustomPreset({
            QRectF(0.0, 0.0, 0.5, 1.0),
            QRectF(0.5, 0.0, 0.5, 1.0)
        });
    }

    kdePreset.id = QStringLiteral("kde-custom");
    kdePreset.name = QStringLiteral("KDE Custom");
    kdePreset.triggerKey = QLatin1Char('K');

    // Insert [K] KDE Custom immediately before Float [X] action
    int insertIdx = m_tilingPresets.size();
    for (int i = 0; i < m_tilingPresets.size(); ++i) {
        if (m_tilingPresets[i].isFloatAction() || m_tilingPresets[i].triggerKey.toUpper() == QLatin1Char('X')) {
            insertIdx = i;
            break;
        }
    }
    m_tilingPresets.insert(insertIdx, kdePreset);
}

void ActiveWindowOutlineEffect::slotToggleActiveWindowFloating()
{
    EffectWindow *w = effects ? effects->activeWindow() : m_activeWindow;
    if (!w || w->isDeleted()) {
        return;
    }

    if (m_floatingWindows.contains(w)) {
        // Unfloat window (return to tiling)
        m_floatingWindows.remove(w);
        if (m_tilingActive) {
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
                const QRectF slotGeo = m_activeTilingSlots[emptyIdx].geometry;
                applyWindowToTilingSlot(w, slotGeo);
            } else {
                activateTilingPreset(m_activeTilingPreset);
            }
        }
        playThemedSound(OutlineSound::SoundEffect::SwitchWorkspace);
    } else {
        // Float window (remove from active tile slot)
        const int slotIdx = findTileSlotIndexForWindow(w);
        if (slotIdx >= 0 && slotIdx < m_activeTilingSlots.size()) {
            m_activeTilingSlots[slotIdx].window = nullptr;
            if (w->window()) {
                if (Tile *tile = w->window()->tile()) {
                    tile->unmanage(w->window());
                }
                w->window()->setTileCompatibility(nullptr);
                w->window()->requestTile(nullptr);
            }
            const QRectF origGeo = m_preTilingGeometries.value(w, QRectF());
            if (origGeo.isValid() && !origGeo.isEmpty() && w->window()) {
                w->window()->moveResize(origGeo);
            }
        }
        m_floatingWindows.insert(w);
        playThemedSound(OutlineSound::SoundEffect::Dismiss);
    }

    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::enterTilingPresetsMode()
{
    m_tilingPresetsModeActive = true;
    m_tilingPresets = LeapKTiling::loadPresets();
    refreshTilingPresetsWithKdeCustom();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_badgeTextureCache.clear();
    m_hudTilingPresetPillRects.clear();
    playThemedSound(OutlineSound::SoundEffect::NavTab);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::exitTilingPresetsMode()
{
    m_tilingPresetsModeActive = false;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_badgeTextureCache.clear();
    m_hudTilingPresetPillRects.clear();
    if (effects) {
        effects->addRepaintFull();
    }
}

QList<LeapKTiling::TileSlot> ActiveWindowOutlineEffect::captureCurrentLayoutSlots() const
{
    QRectF workArea;
    if (effects) {
        workArea = LeapKCompat::clientArea(effects, MaximizeArea, effects->activeScreen(), effects->currentDesktop());
        if (workArea.isEmpty()) {
            workArea = LeapKCompat::virtualScreenGeometry(effects);
        }
    }
    if (workArea.isEmpty()) {
        workArea = QRectF(0, 0, 1920, 1080);
    }
    const qreal waW = workArea.width() > 0 ? workArea.width() : 1920.0;
    const qreal waH = workArea.height() > 0 ? workArea.height() : 1080.0;

    // 1. If tiling is currently active and slots are populated
    if (m_tilingActive && !m_activeTilingSlots.isEmpty()) {
        QList<LeapKTiling::TileSlot> tileSlotList;
        for (const auto &s : m_activeTilingSlots) {
            QRectF rel(
                qBound(0.0, (s.geometry.x() - workArea.x()) / waW, 1.0),
                qBound(0.0, (s.geometry.y() - workArea.y()) / waH, 1.0),
                qBound(0.05, s.geometry.width() / waW, 1.0),
                qBound(0.05, s.geometry.height() / waH, 1.0)
            );
            tileSlotList.append(LeapKTiling::TileSlot{rel});
        }
        return tileSlotList;
    }

    // 2. If KDE Custom JSON is available
    if (!m_savedKdeCustomTilesJson.isEmpty()) {
        const QList<LeapKTiling::TileSlot> parsed = LeapKTiling::parseKdeTileJson(m_savedKdeCustomTilesJson);
        if (!parsed.isEmpty()) {
            return parsed;
        }
    }

    // 3. Fallback: check live KDE TileManager rootTile
    LogicalOutput *activeOutput = effects ? effects->activeScreen() : nullptr;
    VirtualDesktop *currentDesktop = effects ? effects->currentDesktop() : nullptr;
    TileManager *tileMgr = (workspace() && activeOutput) ? workspace()->tileManager(activeOutput) : nullptr;
    RootTile *rootTile = (tileMgr && currentDesktop) ? tileMgr->rootTile(currentDesktop) : nullptr;
    if (rootTile) {
        QList<LeapKTiling::TileSlot> tileSlotList;
        for (Tile *t : rootTile->descendants()) {
            if (!t->isLayout()) {
                const QRectF rel = t->relativeGeometry();
                if (rel.isValid() && rel.width() > 0.02 && rel.height() > 0.02) {
                    tileSlotList.append(LeapKTiling::TileSlot{rel});
                }
            }
        }
        if (!tileSlotList.isEmpty()) {
            return tileSlotList;
        }
    }

    // 4. Default: fallback to first default preset (Two Columns)
    return LeapKTiling::defaultPresets().first().tileSlots;
}

void ActiveWindowOutlineEffect::slotSaveCurrentTilingLayout()
{
    QStringList candidateDirs;
    candidateDirs << QDir::homePath() + QStringLiteral("/.local/bin")
                  << QDir::homePath() + QStringLiteral("/.local/libexec")
                  << QCoreApplication::applicationDirPath()
                  << QStringLiteral("/tmp/build-leapk")
                  << QStringLiteral("/usr/local/bin")
                  << QStringLiteral("/usr/bin")
                  << QStringLiteral("/usr/libexec")
                  << QStringLiteral("/usr/lib/libexec");

    QString layoutExe = QStandardPaths::findExecutable(QStringLiteral("leapk-custom-layout"), candidateDirs);
    if (layoutExe.isEmpty()) {
        qWarning() << "leapk-custom-layout helper not found in:" << candidateDirs;
        return;
    }

    const QList<LeapKTiling::TileSlot> tileSlotList = captureCurrentLayoutSlots();

    // Release keyboard grab before launching the dialog
    if (m_tilingEditModeActive) {
        exitTilingEditMode();
    }
    if (m_tilingPresetsModeActive) {
        exitTilingPresetsMode();
    }
    if (m_jumpMode) {
        cancelJumpMode();
    }

    // Collect existing keys
    QJsonObject existingKeys;
    for (const auto &p : m_tilingPresets) {
        if (!p.triggerKey.isNull() && p.id != QStringLiteral("kde-custom") && p.triggerKey.toUpper() != QLatin1Char('K')) {
            existingKeys[QString(p.triggerKey.toUpper())] = p.name;
        }
    }

    // Find suggested key: if active preset is set and has a key, use that; else find first free key
    QChar defaultKey;
    if (m_tilingActive && !m_activeTilingPreset.triggerKey.isNull()
        && m_activeTilingPreset.triggerKey.toUpper() != QLatin1Char('X')
        && m_activeTilingPreset.triggerKey.toUpper() != QLatin1Char('K')
        && m_activeTilingPreset.id != QStringLiteral("kde-custom")) {
        defaultKey = m_activeTilingPreset.triggerKey.toUpper();
    } else {
        for (char c = 'A'; c <= 'Z'; ++c) {
            if (c == 'X' || c == 'K') continue;
            QChar qch = QLatin1Char(c);
            if (!existingKeys.contains(QString(qch))) {
                defaultKey = qch;
                break;
            }
        }
        if (defaultKey.isNull()) {
            defaultKey = QLatin1Char('H');
        }
    }

    QString defaultName;
    if (m_tilingActive && !m_activeTilingPreset.name.isEmpty() && !m_activeTilingPreset.isFloatAction()) {
        defaultName = m_activeTilingPreset.name;
    } else {
        defaultName = QStringLiteral("Layout %1 Slots").arg(tileSlotList.size());
    }

    QJsonArray slotsArr;
    for (const auto &s : tileSlotList) {
        QJsonObject sObj;
        sObj[QStringLiteral("x")] = s.relativeRect.x();
        sObj[QStringLiteral("y")] = s.relativeRect.y();
        sObj[QStringLiteral("w")] = s.relativeRect.width();
        sObj[QStringLiteral("h")] = s.relativeRect.height();
        slotsArr.append(sObj);
    }

    QStringList args;
    args << QStringLiteral("--slots") << QString::fromUtf8(QJsonDocument(slotsArr).toJson(QJsonDocument::Compact));
    args << QStringLiteral("--name") << defaultName;
    args << QStringLiteral("--key") << QString(defaultKey);
    args << QStringLiteral("--existing") << QString::fromUtf8(QJsonDocument(existingKeys).toJson(QJsonDocument::Compact));

    QProcess *process = new QProcess(this);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process, tileSlotList](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
            const QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8());
            if (doc.isObject()) {
                const QJsonObject obj = doc.object();
                const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
                const QString keyStr = obj.value(QStringLiteral("triggerKey")).toString().trimmed();
                const QChar trigKey = keyStr.isEmpty() ? QChar() : keyStr.at(0).toUpper();

                if (!name.isEmpty() && !trigKey.isNull()) {
                    upsertTilingPreset(name, trigKey, tileSlotList);
                }
            }
        }
    });

    process->start(layoutExe, args);
}

void ActiveWindowOutlineEffect::upsertTilingPreset(const QString &name, QChar triggerKey, const QList<LeapKTiling::TileSlot> &tileSlots)
{
    if (name.isEmpty() || triggerKey.isNull()) {
        return;
    }

    m_tilingPresets.removeIf([](const LeapKTiling::TilingPreset &p) {
        return p.id == QStringLiteral("kde-custom");
    });

    LeapKTiling::TilingPreset newPreset;
    newPreset.id = QStringLiteral("custom-") + QString(triggerKey.toUpper());
    newPreset.name = name;
    newPreset.triggerKey = triggerKey.toUpper();
    newPreset.iconDiagram = LeapKTiling::generateIconDiagram(tileSlots);
    newPreset.description = QStringLiteral("%1 slots • Custom").arg(tileSlots.size());
    newPreset.tileSlots = tileSlots;
    newPreset.kdeJson = LeapKTiling::toKdeJson(newPreset);
    newPreset.isCustom = true;

    LeapKTiling::upsertPreset(m_tilingPresets, newPreset);
    LeapKTiling::savePresets(m_tilingPresets);

    if (m_tilingActive) {
        m_activeTilingPreset = newPreset;
        LogicalOutput *activeOutput = effects ? effects->activeScreen() : nullptr;
        VirtualDesktop *currentDesktop = effects ? effects->currentDesktop() : nullptr;
        TileManager *tileMgr = (workspace() && activeOutput) ? workspace()->tileManager(activeOutput) : nullptr;
        RootTile *rootTile = (tileMgr && currentDesktop) ? tileMgr->rootTile(currentDesktop) : nullptr;

        if (activeOutput && currentDesktop && effects && effects->config()) {
            const QString outUuid = activeOutput->uuid();
            const QString deskId = currentDesktop->id();
            const QByteArray jsonBytes = QJsonDocument(newPreset.toKdeJson()).toJson(QJsonDocument::Compact);

            KConfigGroup cg = effects->config()->group(QStringLiteral("Tiling"));
            KConfigGroup deskGroup(&cg, deskId);
            KConfigGroup outTileGroup(&deskGroup, outUuid);
            outTileGroup.writeEntry("tiles", jsonBytes);
            outTileGroup.writeEntry("padding", m_tilingGaps);
            cg.sync();
        }
        if (tileMgr && rootTile) {
            while (rootTile->childCount() > 0) {
                rootTile->destroyChild(rootTile->childTile(0));
            }
            tileMgr->readSettings(rootTile);
        }
    }

    playThemedSound(OutlineSound::SoundEffect::NavTab);
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_badgeTextureCache.clear();
    m_hudTilingPresetPillRects.clear();
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::deleteTilingPreset(QChar triggerKey)
{
    if (triggerKey.isNull() || triggerKey.toUpper() == QLatin1Char('X') || triggerKey.toUpper() == QLatin1Char('K')) {
        return; // Float ('X') and KDE Custom bridge ('K') are protected
    }

    m_tilingPresets.removeIf([](const LeapKTiling::TilingPreset &p) {
        return p.id == QStringLiteral("kde-custom");
    });

    bool removed = LeapKTiling::removePresetByKey(m_tilingPresets, triggerKey);
    if (removed) {
        LeapKTiling::savePresets(m_tilingPresets);
        refreshTilingPresetsWithKdeCustom();
        playThemedSound(OutlineSound::SoundEffect::Dismiss);
        m_hudHeaderTexture.reset();
        m_hudHeaderCacheKey = -1;
        m_hudHintBarTexture.reset();
        m_hudHintBarCacheKey = -1;
        m_badgeTextureCache.clear();
        m_hudTilingPresetPillRects.clear();
        if (effects) {
            effects->addRepaintFull();
        }
    }
}

void ActiveWindowOutlineEffect::resetTilingPresetsToDefaults()
{
    m_tilingPresets = LeapKTiling::resetToDefaultPresets();
    refreshTilingPresetsWithKdeCustom();
    playThemedSound(OutlineSound::SoundEffect::NavTab);
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_badgeTextureCache.clear();
    m_hudTilingPresetPillRects.clear();
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::activateTilingPreset(const LeapKTiling::TilingPreset &preset, bool preserveJumpMode)
{
    if (preset.isFloatAction()) {
        deactivateTiling();
        return;
    }

    QRectF workArea;
    if (effects) {
        workArea = LeapKCompat::clientArea(effects, MaximizeArea, effects->activeScreen(), effects->currentDesktop());
        if (workArea.isEmpty()) {
            workArea = LeapKCompat::virtualScreenGeometry(effects);
        }
    }
    if (workArea.isEmpty()) {
        workArea = QRectF(0, 0, 1920, 1080);
    }

    const QList<QRectF> slotGeometries = LeapKTiling::computeSlotGeometries(preset, workArea, m_tilingGaps, m_tilingMargins);
    const int slotCount = slotGeometries.size();
    if (slotCount == 0) {
        return;
    }

    // 1. Select windows ONLY from the current virtual desktop and workspace
    QList<EffectWindow *> targetWindows;
    VirtualDesktop *currentDesktop = effects ? effects->currentDesktop() : nullptr;

    auto isEligibleForCurrentWorkspaceTiling = [this, currentDesktop](EffectWindow *win) -> bool {
        if (!win || win->isDeleted() || !isEligibleOutlineWindow(win)) {
            return false;
        }
        if (m_floatingWindows.contains(win) || m_stashedWindows.contains(win) || m_workspaceMinimizedWindows.contains(win)) {
            return false;
        }
        if (win->isMinimized() || win->isSkipSwitcher() || !win->acceptsFocus() || isBlockingPopup(win)) {
            return false;
        }
        if (currentDesktop && !win->isOnDesktop(currentDesktop)) {
            return false;
        }

        // Workspace isolation: only windows belonging to the active workspace (or pinned across all workspaces)
        if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
            if (!m_pinnedWindows.contains(win)) {
                const auto &ws = m_workspaces.at(m_activeHudView - 1);
                const auto desktops = win->desktops();
                const int dNum = (!desktops.isEmpty() && !win->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
                if (!LeapKWorkspace::matchesWorkspace(ws, win->windowClass(), win->caption(), dNum)) {
                    return false;
                }
            }
        } else if (m_activeHudView == 0) {
            // Standard view: exclude windows assigned to named workspaces
            if (!m_pinnedWindows.contains(win) && findWorkspaceForWindow(win) != 0) {
                return false;
            }
        }
        return true;
    };

    // 1a. First priority: active window, if on current desktop and workspace
    if (m_activeWindow && isEligibleForCurrentWorkspaceTiling(m_activeWindow)) {
        targetWindows.append(m_activeWindow);
    }

    // 1b. Second priority: windows that were already tiled in active slots (preserves current tiling layout windows)
    for (const auto &slot : m_activeTilingSlots) {
        if (targetWindows.size() >= slotCount) {
            break;
        }
        EffectWindow *w = slot.window.data();
        if (w && isEligibleForCurrentWorkspaceTiling(w) && !targetWindows.contains(w)) {
            targetWindows.append(w);
        }
    }

    // 1c. Third priority: windows from m_windowHistory that belong to the CURRENT WORKSPACE
    for (EffectWindow *w : m_windowHistory) {
        if (targetWindows.size() >= slotCount) {
            break;
        }
        if (isEligibleForCurrentWorkspaceTiling(w) && !targetWindows.contains(w)) {
            targetWindows.append(w);
        }
    }

    // 1d. Fourth priority: remaining windows on the CURRENT WORKSPACE from stackingOrder (top-to-bottom)
    if (targetWindows.size() < slotCount && effects) {
        const auto stacking = effects->stackingOrder();
        for (auto it = stacking.rbegin(); it != stacking.rend(); ++it) {
            if (targetWindows.size() >= slotCount) {
                break;
            }
            EffectWindow *w = *it;
            if (isEligibleForCurrentWorkspaceTiling(w) && !targetWindows.contains(w)) {
                targetWindows.append(w);
            }
        }
    }

    // 1e. Fifth priority: if slots remain unfilled, include minimized windows on the CURRENT WORKSPACE
    if (targetWindows.size() < slotCount && effects) {
        auto isEligibleMinimized = [this, currentDesktop](EffectWindow *win) -> bool {
            if (!win || win->isDeleted() || !isEligibleOutlineWindow(win)) {
                return false;
            }
            if (m_floatingWindows.contains(win) || m_stashedWindows.contains(win) || m_workspaceMinimizedWindows.contains(win)) {
                return false;
            }
            if (win->isSkipSwitcher() || !win->acceptsFocus() || isBlockingPopup(win)) {
                return false;
            }
            if (currentDesktop && !win->isOnDesktop(currentDesktop)) {
                return false;
            }
            if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
                if (!m_pinnedWindows.contains(win)) {
                    const auto &ws = m_workspaces.at(m_activeHudView - 1);
                    const auto desktops = win->desktops();
                    const int dNum = (!desktops.isEmpty() && !win->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
                    if (!LeapKWorkspace::matchesWorkspace(ws, win->windowClass(), win->caption(), dNum)) {
                        return false;
                    }
                }
            } else if (m_activeHudView == 0) {
                if (!m_pinnedWindows.contains(win) && findWorkspaceForWindow(win) != 0) {
                    return false;
                }
            }
            return true;
        };

        const auto stacking = effects->stackingOrder();
        for (auto it = stacking.rbegin(); it != stacking.rend(); ++it) {
            if (targetWindows.size() >= slotCount) {
                break;
            }
            EffectWindow *w = *it;
            if (w->isMinimized() && isEligibleMinimized(w) && !targetWindows.contains(w)) {
                targetWindows.append(w);
            }
        }
    }

    // 2. Save original geometries before tiling (Fixes Item 2)
    for (EffectWindow *w : targetWindows) {
        if (w && !m_preTilingGeometries.contains(w)) {
            m_preTilingGeometries.insert(w, w->frameGeometry());
        }
    }

    // 3. Synchronize layout with KDE's built-in Custom Tiling
    LogicalOutput *activeOutput = effects ? effects->activeScreen() : nullptr;
    TileManager *tileMgr = (workspace() && activeOutput) ? workspace()->tileManager(activeOutput) : nullptr;
    RootTile *rootTile = (tileMgr && currentDesktop) ? tileMgr->rootTile(currentDesktop) : nullptr;

    if (preset.id != QStringLiteral("kde-custom")) {
        // Clear existing child tiles before reloading to prevent duplicate tile accumulation
        if (rootTile) {
            while (rootTile->childCount() > 0) {
                Tile *child = rootTile->childTile(0);
                rootTile->destroyChild(child);
            }
        }

        if (activeOutput && currentDesktop && effects && effects->config()) {
            const QString outUuid = activeOutput->uuid();
            const QString deskId = currentDesktop->id();
            const QByteArray jsonBytes = QJsonDocument(preset.toKdeJson()).toJson(QJsonDocument::Compact);

            KConfigGroup cg = effects->config()->group(QStringLiteral("Tiling"));
            KConfigGroup deskGroup(&cg, deskId);
            KConfigGroup outTileGroup(&deskGroup, outUuid);
            outTileGroup.writeEntry("tiles", jsonBytes);
            outTileGroup.writeEntry("padding", m_tilingGaps);
            cg.sync();
        }

        // Reload settings into live KDE TileManager
        if (tileMgr && rootTile) {
            while (rootTile->childCount() > 0) {
                rootTile->destroyChild(rootTile->childTile(0));
            }
            tileMgr->readSettings(rootTile);
        }
    } else {
        // When activating kde-custom: restore custom layout into kwinrc and live rootTile if not already matching!
        if (activeOutput && currentDesktop && effects && effects->config()) {
            const QString outUuid = activeOutput->uuid();
            const QString deskId = currentDesktop->id();
            QJsonObject kdeObj = preset.kdeJson;
            if (kdeObj.isEmpty()) {
                kdeObj = m_savedKdeCustomTilesJson;
            }
            if (kdeObj.isEmpty()) {
                KConfigGroup leapkCg = effects->config()->group(QStringLiteral("Effect-leapk"));
                const QString backup = leapkCg.readEntry(QStringLiteral("CustomTilingBackup_%1_%2").arg(deskId, outUuid), QString());
                if (!backup.isEmpty()) {
                    kdeObj = QJsonDocument::fromJson(backup.toUtf8()).object();
                }
            }
            if (kdeObj.isEmpty() || (kdeObj.value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("floating") && !preset.tileSlots.isEmpty())) {
                kdeObj = preset.toKdeJson();
            }

            // Check if live rootTile already matches preset slots to avoid destroying fresh tiles from KDE tiles editor
            bool rootMatches = false;
            if (rootTile && rootTile->childCount() > 0 && rootTile->layoutDirection() != Tile::LayoutDirection::Floating) {
                QList<QRectF> curLeafRects;
                for (Tile *t : rootTile->descendants()) {
                    if (!t->isLayout() && t->relativeGeometry().isValid()) {
                        curLeafRects.append(t->relativeGeometry());
                    }
                }
                if (curLeafRects.size() == preset.tileSlots.size()) {
                    bool allMatch = true;
                    for (int s = 0; s < curLeafRects.size(); ++s) {
                        const QRectF &r1 = curLeafRects[s];
                        const QRectF &r2 = preset.tileSlots[s].relativeRect;
                        if (qAbs(r1.x() - r2.x()) > 0.03 || qAbs(r1.y() - r2.y()) > 0.03
                            || qAbs(r1.width() - r2.width()) > 0.03 || qAbs(r1.height() - r2.height()) > 0.03) {
                            allMatch = false;
                            break;
                        }
                    }
                    rootMatches = allMatch;
                }
            }

            if (!rootMatches && !kdeObj.isEmpty()) {
                if (rootTile) {
                    while (rootTile->childCount() > 0) {
                        Tile *child = rootTile->childTile(0);
                        rootTile->destroyChild(child);
                    }
                }
                const QByteArray jsonBytes = QJsonDocument(kdeObj).toJson(QJsonDocument::Compact);
                KConfigGroup cg = effects->config()->group(QStringLiteral("Tiling"));
                KConfigGroup deskGroup(&cg, deskId);
                KConfigGroup outTileGroup(&deskGroup, outUuid);
                outTileGroup.writeEntry("tiles", jsonBytes);
                outTileGroup.writeEntry("padding", m_tilingGaps);
                cg.sync();
                if (tileMgr && rootTile) {
                    tileMgr->readSettings(rootTile);
                }
            }
        }
    }

    ensureTilingNoMinSizeRule();

    // 4. Place windows into active tiling slots using priority order
    m_tilingActive = true;
    m_activeTilingPreset = preset;
    m_activeTilingSlots.clear();

    const QString curWsId = workspaceIdForView(m_activeHudView);
    m_workspaceTilingActive[curWsId] = true;
    m_workspaceTilingPreset[curWsId] = preset;
    if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
        m_workspaces[m_activeHudView - 1].tilingPresetId = preset.id;
        LeapKWorkspace::saveWorkspaces(m_workspaces);
    } else if (m_activeHudView == 0 && effects && effects->config()) {
        KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
        primaryConfig.writeEntry("StandardTilingPresetId", preset.id);
        primaryConfig.sync();
    }

    const QList<int> priorityOrder = LeapKTiling::computeSlotPriorityOrder(preset.tileSlots);

    m_activeTilingSlots.resize(slotCount);
    for (int i = 0; i < slotCount; ++i) {
        m_activeTilingSlots[i].geometry = slotGeometries[i];
        m_activeTilingSlots[i].window = nullptr;
    }

    for (int wIdx = 0; wIdx < targetWindows.size() && wIdx < priorityOrder.size(); ++wIdx) {
        const int slotIdx = priorityOrder[wIdx];
        if (slotIdx < 0 || slotIdx >= slotCount) {
            continue;
        }

        EffectWindow *w = targetWindows[wIdx];
        m_activeTilingSlots[slotIdx].window = w;
        m_activeTilingSlots[slotIdx].originalGeometry = m_preTilingGeometries.value(w, w->frameGeometry());

        applyWindowToTilingSlot(w, m_activeTilingSlots[slotIdx].geometry);
    }

    // Raise tiled windows above background windows, while keeping floating windows on top
    if (workspace()) {
        for (int i = targetWindows.size() - 1; i >= 0; --i) {
            EffectWindow *w = targetWindows[i];
            if (w && !w->isDeleted() && w->window()) {
                workspace()->raiseWindow(w->window());
            }
        }
        for (EffectWindow *w : m_floatingWindows) {
            if (w && !w->isDeleted() && w->window()) {
                workspace()->raiseWindow(w->window());
            }
        }
    }

    if (!targetWindows.isEmpty() && effects) {
        effects->activateWindow(targetWindows.first());
    }

    playThemedSound(OutlineSound::SoundEffect::SwitchWorkspace);
    exitTilingPresetsMode();
    if (m_jumpMode && !preserveJumpMode) {
        cancelJumpMode();
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

QList<Tile *> ActiveWindowOutlineEffect::getLiveKdeLeafTiles() const
{
    QList<Tile *> leafTiles;
    LogicalOutput *activeOutput = effects ? effects->activeScreen() : nullptr;
    VirtualDesktop *currentDesktop = effects ? effects->currentDesktop() : nullptr;
    TileManager *tileMgr = (workspace() && activeOutput) ? workspace()->tileManager(activeOutput) : nullptr;
    RootTile *rootTile = (tileMgr && currentDesktop) ? tileMgr->rootTile(currentDesktop) : nullptr;
    if (rootTile) {
        for (Tile *t : rootTile->descendants()) {
            if (!t->isLayout()) {
                leafTiles.append(t);
            }
        }
    }
    return leafTiles;
}

void ActiveWindowOutlineEffect::applyWindowToTilingSlot(EffectWindow *ew, const QRectF &geo, Tile * /*targetTile*/)
{
    if (!ew || ew->isDeleted() || !ew->window()) {
        return;
    }
    Window *win = ew->window();
    if (ew->isMinimized()) {
        ew->setMinimized(false);
    }
    if (win->maximizeMode() != MaximizeRestore) {
        win->setMaximize(false, false);
    }
    win->evaluateWindowRules();

    // Detach window from KWin's native quick-tiling state machine so KWin doesn't
    // override or snap window geometry back to static native tile bounds.
    if (Tile *tile = win->tile()) {
        tile->unmanage(win);
    }
    win->setTileCompatibility(nullptr);
    win->requestTile(nullptr);

    win->moveResize(geo);
}

void ActiveWindowOutlineEffect::handleTiledWindowResized(EffectWindow *w, const QRectF &newGeo)
{
    if (!m_tilingActive || !w || m_activeTilingSlots.isEmpty()) {
        return;
    }

    const int slotIdx = findTileSlotIndexForWindow(w);
    if (slotIdx < 0 || slotIdx >= m_activeTilingSlots.size()) {
        return;
    }

    // Windows in LeapK tiling are decoupled from KWin's static native tile objects.
    // LeapK co-operatively resizes adjacent slots and neighboring windows in real-time.
    QList<QRectF> geos;
    for (const auto &s : m_activeTilingSlots) {
        geos.append(s.geometry);
    }

    LeapKTiling::ResizeDirection dir;
    qreal delta = 0.0;
    LeapKTiling::TileEdge edge = LeapKTiling::TileEdge::Auto;
    if (LeapKTiling::calculateDividerMove(geos, slotIdx, newGeo, dir, delta, edge)) {
        if (LeapKTiling::resizeSlotGeometries(geos, slotIdx, dir, delta, 100.0, true, edge)) {
            for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
                m_activeTilingSlots[i].geometry = geos[i];
                if (i != slotIdx) {
                    EffectWindow *neighbor = m_activeTilingSlots[i].window.data();
                    if (neighbor && !neighbor->isDeleted() && neighbor->window()) {
                        applyWindowToTilingSlot(neighbor, geos[i]);
                    }
                }
            }
        }
    }

    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::syncActiveTilingSlotsToKdeTiles()
{
    LogicalOutput *activeOutput = effects ? effects->activeScreen() : nullptr;
    VirtualDesktop *currentDesktop = effects ? effects->currentDesktop() : nullptr;
    TileManager *tileMgr = (workspace() && activeOutput) ? workspace()->tileManager(activeOutput) : nullptr;

    if (tileMgr) {
        tileMgr->saveSettings();
    }

    // Always synchronize our internal slot geometries from actual window frame geometries
    for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
        if (m_activeTilingSlots[i].window && !m_activeTilingSlots[i].window->isDeleted()) {
            m_activeTilingSlots[i].geometry = m_activeTilingSlots[i].window->frameGeometry();
        }
    }

    // Update active tiling preset slot proportions so state is kept in sync
    QRectF workArea;
    if (effects) {
        workArea = LeapKCompat::clientArea(effects, MaximizeArea, effects->activeScreen(), effects->currentDesktop());
    }
    if (workArea.isEmpty()) {
        workArea = QRectF(0, 0, 1920, 1080);
    }
    const qreal waW = workArea.width() > 0 ? workArea.width() : 1920.0;
    const qreal waH = workArea.height() > 0 ? workArea.height() : 1080.0;
    for (int i = 0; i < m_activeTilingSlots.size() && i < m_activeTilingPreset.tileSlots.size(); ++i) {
        m_activeTilingPreset.tileSlots[i].relativeRect = QRectF(
            qBound(0.0, (m_activeTilingSlots[i].geometry.x() - workArea.x()) / waW, 1.0),
            qBound(0.0, (m_activeTilingSlots[i].geometry.y() - workArea.y()) / waH, 1.0),
            qBound(0.05, m_activeTilingSlots[i].geometry.width() / waW, 1.0),
            qBound(0.05, m_activeTilingSlots[i].geometry.height() / waH, 1.0)
        );
    }
    const QString curWsId = workspaceIdForView(m_activeHudView);
    m_workspaceTilingPreset[curWsId] = m_activeTilingPreset;

    // If active preset is kde-custom, reload and persist custom JSON backup
    if (m_activeTilingPreset.id == QStringLiteral("kde-custom") && activeOutput && currentDesktop && effects && effects->config()) {
        const QString outUuid = activeOutput->uuid();
        const QString deskId = currentDesktop->id();
        KConfigGroup cg = effects->config()->group(QStringLiteral("Tiling"));
        KConfigGroup deskGroup(&cg, deskId);
        KConfigGroup outTileGroup(&deskGroup, outUuid);
        const QString tilesJsonStr = outTileGroup.readEntry("tiles", QString());
        if (!tilesJsonStr.isEmpty()) {
            QJsonObject kdeObj = QJsonDocument::fromJson(tilesJsonStr.toUtf8()).object();
            if (!kdeObj.isEmpty() && kdeObj.value(QStringLiteral("layoutDirection")).toString() != QStringLiteral("floating")) {
                m_savedKdeCustomTilesJson = kdeObj;
                m_activeTilingPreset.kdeJson = kdeObj;
                KConfigGroup leapkCg = effects->config()->group(QStringLiteral("Effect-leapk"));
                leapkCg.writeEntry(QStringLiteral("CustomTilingBackup_%1_%2").arg(deskId, outUuid), tilesJsonStr);
                leapkCg.sync();
            } else if (!kdeObj.isEmpty() && kdeObj.value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("floating")) {
                const QJsonObject treeJson = m_activeTilingPreset.toKdeJson();
                m_savedKdeCustomTilesJson = treeJson;
                m_activeTilingPreset.kdeJson = treeJson;
                const QByteArray treeBytes = QJsonDocument(treeJson).toJson(QJsonDocument::Compact);
                KConfigGroup leapkCg = effects->config()->group(QStringLiteral("Effect-leapk"));
                leapkCg.writeEntry(QStringLiteral("CustomTilingBackup_%1_%2").arg(deskId, outUuid), treeBytes);
                leapkCg.sync();
                outTileGroup.writeEntry("tiles", treeBytes);
                cg.sync();
            }
        }
    } else if (activeOutput && currentDesktop && effects && effects->config()) {
        // Synchronize updated proportions into kwinrc and live KDE TileManager
        const QString outUuid = activeOutput->uuid();
        const QString deskId = currentDesktop->id();
        RootTile *rootTile = (tileMgr && currentDesktop) ? tileMgr->rootTile(currentDesktop) : nullptr;
        const QByteArray jsonBytes = QJsonDocument(m_activeTilingPreset.toKdeJson()).toJson(QJsonDocument::Compact);

        KConfigGroup cg = effects->config()->group(QStringLiteral("Tiling"));
        KConfigGroup deskGroup(&cg, deskId);
        KConfigGroup outTileGroup(&deskGroup, outUuid);
        outTileGroup.writeEntry("tiles", jsonBytes);
        outTileGroup.writeEntry("padding", m_tilingGaps);
        cg.sync();
        if (tileMgr && rootTile) {
            while (rootTile->childCount() > 0) {
                rootTile->destroyChild(rootTile->childTile(0));
            }
            tileMgr->readSettings(rootTile);
        }
    }

    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::deactivateTiling(EffectWindow *skipRestoreWindow, bool preserveWorkspaceState)
{
    if (!m_tilingActive) {
        return;
    }

    if (!preserveWorkspaceState) {
        const QString curWsId = workspaceIdForView(m_activeHudView);
        m_workspaceTilingActive[curWsId] = false;
        m_workspaceTilingPreset.remove(curWsId);
        if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
            m_workspaces[m_activeHudView - 1].tilingPresetId = QString();
            LeapKWorkspace::saveWorkspaces(m_workspaces);
        } else if (m_activeHudView == 0 && effects && effects->config()) {
            KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
            primaryConfig.writeEntry("StandardTilingPresetId", QString());
            primaryConfig.sync();
        }
    }

    // Restore windows back to their sizes and positions from before tiling (Fixes Item 2)
    for (auto it = m_preTilingGeometries.constBegin(); it != m_preTilingGeometries.constEnd(); ++it) {
        EffectWindow *w = it.key().data();
        if (!w || w->isDeleted() || w == skipRestoreWindow) {
            continue;
        }

        const QRectF origGeo = it.value();
        if (w->window()) {
            if (Tile *tile = w->window()->tile()) {
                tile->unmanage(w->window());
            }
            w->window()->setTileCompatibility(nullptr);
            w->window()->requestTile(nullptr);

            if (origGeo.isValid() && !origGeo.isEmpty()) {
                w->window()->moveResize(origGeo);
            }
        }
    }

    m_preTilingGeometries.clear();
    m_tilingActive = false;
    m_activeTilingPreset = LeapKTiling::TilingPreset();
    m_activeTilingSlots.clear();
    playThemedSound(OutlineSound::SoundEffect::Dismiss);
    if (m_tilingEditModeActive) {
        exitTilingEditMode();
    }
    if (m_tilingPresetsModeActive) {
        exitTilingPresetsMode();
    }
    if (m_jumpMode && !preserveWorkspaceState) {
        cancelJumpMode();
    }
    if (effects) {
        effects->addRepaintFull();
    }
}

bool ActiveWindowOutlineEffect::isWindowInTileSlot(EffectWindow *w) const
{
    if (!w) {
        return false;
    }
    for (const auto &slot : m_activeTilingSlots) {
        if (slot.window == w) {
            return true;
        }
    }
    return false;
}

int ActiveWindowOutlineEffect::findTileSlotIndexForWindow(EffectWindow *w) const
{
    if (!w) {
        return -1;
    }
    for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
        if (m_activeTilingSlots[i].window == w) {
            return i;
        }
    }
    // Geometric fallback if pointer is unlinked or detached
    const QRectF wGeo = w->frameGeometry();
    const QPointF wCenter = wGeo.center();
    int bestIdx = -1;
    qreal maxOverlap = 0.0;
    for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
        const QRectF &slotGeo = m_activeTilingSlots[i].geometry;
        if (slotGeo.contains(wCenter)) {
            return i;
        }
        const QRectF inter = slotGeo.intersected(wGeo);
        const qreal area = inter.width() * inter.height();
        if (area > maxOverlap) {
            maxOverlap = area;
            bestIdx = i;
        }
    }
    return bestIdx;
}

EffectWindow *ActiveWindowOutlineEffect::findWindowInSlot(int slotIdx) const
{
    if (slotIdx < 0 || slotIdx >= m_activeTilingSlots.size()) {
        return nullptr;
    }
    EffectWindow *ew = m_activeTilingSlots[slotIdx].window.data();
    if (ew && !ew->isDeleted() && ew->isVisible() && !ew->isMinimized() && ew->isOnCurrentDesktop()) {
        return ew;
    }
    if (!effects) {
        return nullptr;
    }
    const QRectF slotGeo = m_activeTilingSlots[slotIdx].geometry;
    EffectWindow *bestWin = nullptr;
    qreal maxOverlap = 0.0;
    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        if (!w || w->isDeleted() || !w->isVisible() || w->isMinimized() || !w->isOnCurrentDesktop() || !isEligibleTargetWindow(w)) {
            continue;
        }
        const QRectF inter = slotGeo.intersected(w->frameGeometry());
        const qreal area = inter.width() * inter.height();
        if (area > maxOverlap) {
            maxOverlap = area;
            bestWin = w;
        }
    }
    return bestWin;
}

void ActiveWindowOutlineEffect::refreshTilingSlotWindows()
{
    if (!effects || m_activeTilingSlots.isEmpty()) {
        return;
    }

    const auto windows = effects->stackingOrder();
    QList<EffectWindow *> candidates;
    for (EffectWindow *w : windows) {
        if (!w || w->isDeleted() || !w->isVisible() || w->isMinimized() || !w->isOnCurrentDesktop() || !isEligibleTargetWindow(w)) {
            continue;
        }
        if (m_floatingWindows.contains(w)) {
            continue;
        }
        candidates.append(w);
    }

    QSet<EffectWindow *> assigned;
    for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
        EffectWindow *current = m_activeTilingSlots[i].window.data();
        if (current && candidates.contains(current) && !assigned.contains(current)) {
            assigned.insert(current);
        } else {
            m_activeTilingSlots[i].window = nullptr;
        }
    }

    for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
        if (m_activeTilingSlots[i].window) {
            continue;
        }
        const QRectF slotGeo = m_activeTilingSlots[i].geometry;
        EffectWindow *bestWin = nullptr;
        qreal maxOverlap = 0.0;
        for (EffectWindow *w : candidates) {
            if (assigned.contains(w)) {
                continue;
            }
            const QRectF inter = slotGeo.intersected(w->frameGeometry());
            const qreal area = inter.width() * inter.height();
            if (area > maxOverlap) {
                maxOverlap = area;
                bestWin = w;
            }
        }
        if (bestWin) {
            m_activeTilingSlots[i].window = bestWin;
            assigned.insert(bestWin);
        }
    }
}

void ActiveWindowOutlineEffect::ensureTilingNoMinSizeRule()
{
    using namespace LeapKTiling;
    if (!m_tilingRemoveMinSizeLimit) {
        return;
    }

    const QString ruleId = QString::fromLatin1(kNoMinSizeRuleId);
    auto rulesConfig = KSharedConfig::openConfig(QStringLiteral("kwinrulesrc"));
    KConfigGroup generalGroup(rulesConfig, QStringLiteral("General"));
    QStringList rulesList = generalGroup.readEntry(QStringLiteral("rules"), QStringList());

    bool needsWrite = false;
    if (!rulesList.contains(ruleId)) {
        rulesList.prepend(ruleId);
        generalGroup.writeEntry(QStringLiteral("rules"), rulesList);
        generalGroup.writeEntry(QStringLiteral("count"), rulesList.size());
        needsWrite = true;
    }

    KConfigGroup ruleGroup(rulesConfig, ruleId);
    TilingWindowRuleConfig defRule;
    if (!ruleGroup.exists()
        || ruleGroup.readEntry(QStringLiteral("minsize"), QString()) != defRule.minsize
        || ruleGroup.readEntry(QStringLiteral("minsizerule"), 0) != defRule.minsizerule
        || ruleGroup.readEntry(QStringLiteral("ignoregeometry"), false) != defRule.ignoregeometry
        || ruleGroup.readEntry(QStringLiteral("ignoregeometryrule"), 0) != defRule.ignoregeometryrule
        || ruleGroup.readEntry(QStringLiteral("strictgeometry"), true) != defRule.strictgeometry
        || ruleGroup.readEntry(QStringLiteral("strictgeometryrule"), 0) != defRule.strictgeometryrule
        || ruleGroup.readEntry(QStringLiteral("types"), 0) != defRule.types
        || !ruleGroup.hasKey(QStringLiteral("wmclass"))
        || ruleGroup.readEntry(QStringLiteral("wmclasscomplete"), true) != defRule.wmclasscomplete
        || ruleGroup.readEntry(QStringLiteral("wmclassmatch"), -1) != defRule.wmclassmatch) {

        ruleGroup.writeEntry(QStringLiteral("description"), defRule.description);
        ruleGroup.writeEntry(QStringLiteral("minsize"), defRule.minsize);
        ruleGroup.writeEntry(QStringLiteral("minsizerule"), defRule.minsizerule);
        ruleGroup.writeEntry(QStringLiteral("ignoregeometry"), defRule.ignoregeometry);
        ruleGroup.writeEntry(QStringLiteral("ignoregeometryrule"), defRule.ignoregeometryrule);
        ruleGroup.writeEntry(QStringLiteral("strictgeometry"), defRule.strictgeometry);
        ruleGroup.writeEntry(QStringLiteral("strictgeometryrule"), defRule.strictgeometryrule);
        ruleGroup.writeEntry(QStringLiteral("types"), defRule.types);
        ruleGroup.writeEntry(QStringLiteral("wmclass"), defRule.wmclass);
        ruleGroup.writeEntry(QStringLiteral("wmclasscomplete"), defRule.wmclasscomplete);
        ruleGroup.writeEntry(QStringLiteral("wmclassmatch"), defRule.wmclassmatch);
        needsWrite = true;
    }

    if (needsWrite) {
        rulesConfig->sync();
        if (workspace() && workspace()->rulebook()) {
            workspace()->rulebook()->load();
        }
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/KWin"),
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("reconfigure"));
        QDBusConnection::sessionBus().send(msg);

        if (effects) {
            for (EffectWindow *w : effects->stackingOrder()) {
                if (w && !w->isDeleted() && w->window()) {
                    w->window()->evaluateWindowRules();
                }
            }
        }
    }
}

void ActiveWindowOutlineEffect::performTilingSwap(EffectWindow *target)
{
    if (!m_tilingActive || !target || target->isDeleted()) {
        if (effects && target && !target->isDeleted()) {
            effects->activateWindow(target);
        }
        return;
    }

    if (m_activeTilingSlots.isEmpty()) {
        if (effects) {
            effects->activateWindow(target);
        }
        return;
    }

    // 1. If target is ALREADY participating in tiling:
    // Simply activate / jump to that window (DO NOT SWAP)! (Fixes Item 5)
    if (isWindowInTileSlot(target)) {
        if (target->isMinimized()) {
            target->setMinimized(false);
        }
        if (effects) {
            effects->activateWindow(target);
        }
        warpCursorToWindow(target);
        return;
    }

    ensureTilingNoMinSizeRule();

    // 2. Target is OUTSIDE of tiling (background / minimized / stash):
    // If there is an empty tile slot, populate it first without displacing existing tiled windows!
    // Otherwise, swap it into the currently active tile slot.
    int targetSlotIdx = -1;
    for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
        if (!m_activeTilingSlots[i].window || m_activeTilingSlots[i].window->isDeleted()) {
            targetSlotIdx = i;
            break;
        }
    }

    if (targetSlotIdx < 0) {
        targetSlotIdx = findTileSlotIndexForWindow(m_activeWindow);
        if (targetSlotIdx < 0 || targetSlotIdx >= m_activeTilingSlots.size()) {
            targetSlotIdx = 0;
        }
    }

    EffectWindow *previousWin = m_activeTilingSlots[targetSlotIdx].window.data();
    m_activeTilingSlots[targetSlotIdx].window = target;

    // Save target's original geometry before tiling if not already saved
    if (!m_preTilingGeometries.contains(target)) {
        m_preTilingGeometries.insert(target, target->frameGeometry());
    }

    // Unmanage previousWin from tile, restore its original geometry, and send it underneath tiled windows
    if (previousWin && !previousWin->isDeleted() && previousWin->window()) {
        if (Tile *tile = previousWin->window()->tile()) {
            tile->unmanage(previousWin->window());
        }
        previousWin->window()->setTileCompatibility(nullptr);
        previousWin->window()->requestTile(nullptr);
        const QRectF prevOrig = m_preTilingGeometries.value(previousWin);
        if (prevOrig.isValid() && !prevOrig.isEmpty()) {
            previousWin->window()->moveResize(prevOrig);
        }
        m_preTilingGeometries.remove(previousWin);

        if (workspace()) {
            workspace()->lowerWindow(previousWin->window());
        }
    }

    // Ensure all other currently active tiled windows stay raised above any background windows
    if (workspace()) {
        for (const auto &slot : m_activeTilingSlots) {
            if (slot.window && !slot.window->isDeleted() && slot.window != target && slot.window->window()) {
                if (slot.window->isMinimized()) {
                    slot.window->setMinimized(false);
                }
                workspace()->raiseWindow(slot.window->window());
            }
        }
    }

    // Place target into target slot
    const QRectF slotGeo = m_activeTilingSlots[targetSlotIdx].geometry;
    applyWindowToTilingSlot(target, slotGeo);

    if (workspace() && target->window()) {
        workspace()->raiseWindow(target->window());
    }
    if (effects) {
        effects->activateWindow(target);
    }
    warpCursorToWindow(target);
}

void ActiveWindowOutlineEffect::slotToggleTilingEditMode()
{
    if (m_tilingEditModeActive) {
        exitTilingEditMode();
    } else {
        enterTilingEditMode();
    }
}

void ActiveWindowOutlineEffect::enterTilingEditMode()
{
    if (!m_tilingActive || m_activeTilingSlots.isEmpty()) {
        if (!m_tilingPresets.isEmpty()) {
            activateTilingPreset(m_tilingPresets.first());
        }
    }
    if (!m_tilingActive || m_activeTilingSlots.isEmpty()) {
        playThemedSound(OutlineSound::SoundEffect::Dismiss);
        return;
    }

    if (m_jumpMode) {
        cancelJumpMode();
    }

    refreshTilingSlotWindows();

    if (!effects->grabKeyboard(this)) {
        return;
    }
    m_keyboardGrabActive = true;
    m_tilingEditModeActive = true;
    m_tilingMergeModeActive = false;
    m_tilingEditMultiplier = 0;
    playThemedSound(OutlineSound::SoundEffect::EnterJump);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::exitTilingEditMode()
{
    if (!m_tilingEditModeActive) {
        return;
    }
    m_tilingEditModeActive = false;
    m_tilingMergeModeActive = false;
    m_tilingEditMultiplier = 0;
    finishKeyboardGrab();

    // Synchronize resized layout to KWin native tiles and kwinrc
    LogicalOutput *activeOutput = effects ? effects->activeScreen() : nullptr;
    VirtualDesktop *currentDesktop = effects ? effects->currentDesktop() : nullptr;
    TileManager *tileMgr = (workspace() && activeOutput) ? workspace()->tileManager(activeOutput) : nullptr;
    RootTile *rootTile = (tileMgr && currentDesktop) ? tileMgr->rootTile(currentDesktop) : nullptr;

    if (activeOutput && currentDesktop && effects && effects->config()) {
        const QString outUuid = activeOutput->uuid();
        const QString deskId = currentDesktop->id();
        const QByteArray jsonBytes = QJsonDocument(m_activeTilingPreset.toKdeJson()).toJson(QJsonDocument::Compact);

        KConfigGroup cg = effects->config()->group(QStringLiteral("Tiling"));
        KConfigGroup deskGroup(&cg, deskId);
        KConfigGroup outTileGroup(&deskGroup, outUuid);
        outTileGroup.writeEntry("tiles", jsonBytes);
        outTileGroup.writeEntry("padding", m_tilingGaps);
        cg.sync();
        if (tileMgr && rootTile) {
            while (rootTile->childCount() > 0) {
                rootTile->destroyChild(rootTile->childTile(0));
            }
            tileMgr->readSettings(rootTile);
        }
    }

    playThemedSound(OutlineSound::SoundEffect::Dismiss);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::performTileSlotSwap(int idx1, int idx2)
{
    if (idx1 < 0 || idx1 >= m_activeTilingSlots.size() || idx2 < 0 || idx2 >= m_activeTilingSlots.size() || idx1 == idx2) {
        return;
    }

    refreshTilingSlotWindows();

    EffectWindow *w1 = m_activeTilingSlots[idx1].window.data();
    EffectWindow *w2 = m_activeTilingSlots[idx2].window.data();

    if (!w1) {
        w1 = findWindowInSlot(idx1);
    }
    if (!w2) {
        w2 = findWindowInSlot(idx2);
    }

    m_activeTilingSlots[idx1].window = w2;
    m_activeTilingSlots[idx2].window = w1;

    const QRectF geo1 = m_activeTilingSlots[idx1].geometry;
    const QRectF geo2 = m_activeTilingSlots[idx2].geometry;

    if (w1) {
        applyWindowToTilingSlot(w1, geo2);
    }
    if (w2) {
        applyWindowToTilingSlot(w2, geo1);
    }

    if (workspace()) {
        if (w2 && !w2->isDeleted() && w2->window()) {
            workspace()->raiseWindow(w2->window());
        }
        if (w1 && !w1->isDeleted() && w1->window()) {
            workspace()->raiseWindow(w1->window());
        }
    }

    if (w1) {
        if (effects) {
            effects->activateWindow(w1);
        }
        warpCursorToWindow(w1);
    }

    playThemedSound(OutlineSound::SoundEffect::LandWindow);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::splitActiveTileSlot(Qt::Orientation orientation)
{
    if (m_activeTilingSlots.isEmpty()) {
        return;
    }

    refreshTilingSlotWindows();

    int activeSlotIdx = findTileSlotIndexForWindow(m_activeWindow);
    if (activeSlotIdx < 0 || activeSlotIdx >= m_activeTilingSlots.size()) {
        activeSlotIdx = 0;
    }

    const QRectF r = (activeSlotIdx < m_activeTilingPreset.tileSlots.size())
        ? m_activeTilingPreset.tileSlots[activeSlotIdx].relativeRect
        : QRectF(0.0, 0.0, 1.0, 1.0);
    const QRectF g = m_activeTilingSlots[activeSlotIdx].geometry;
    EffectWindow *origWin = m_activeTilingSlots[activeSlotIdx].window.data();

    QRectF r1, r2, g1, g2;
    if (orientation == Qt::Vertical) {
        // Vertical split: Top and bottom halves
        r1 = QRectF(r.left(), r.top(), r.width(), r.height() / 2.0);
        r2 = QRectF(r.left(), r.top() + r.height() / 2.0, r.width(), r.height() / 2.0);
        g1 = QRectF(g.left(), g.top(), g.width(), g.height() / 2.0);
        g2 = QRectF(g.left(), g.top() + g.height() / 2.0, g.width(), g.height() / 2.0);
    } else {
        // Horizontal split: Left and right halves
        r1 = QRectF(r.left(), r.top(), r.width() / 2.0, r.height());
        r2 = QRectF(r.left() + r.width() / 2.0, r.top(), r.width() / 2.0, r.height());
        g1 = QRectF(g.left(), g.top(), g.width() / 2.0, g.height());
        g2 = QRectF(g.left() + g.width() / 2.0, g.top(), g.width() / 2.0, g.height());
    }

    if (activeSlotIdx < m_activeTilingPreset.tileSlots.size()) {
        m_activeTilingPreset.tileSlots[activeSlotIdx].relativeRect = r1;
    } else {
        m_activeTilingPreset.tileSlots.append(LeapKTiling::TileSlot{r1});
    }
    m_activeTilingSlots[activeSlotIdx].geometry = g1;

    LeapKTiling::TileSlot newSlot;
    newSlot.relativeRect = r2;
    m_activeTilingPreset.tileSlots.insert(activeSlotIdx + 1, newSlot);

    ActiveTilingSlot newActiveSlot;
    newActiveSlot.geometry = g2;
    newActiveSlot.window = nullptr;
    m_activeTilingSlots.insert(activeSlotIdx + 1, newActiveSlot);

    m_activeTilingPreset.id = QStringLiteral("custom-layout");
    m_activeTilingPreset.name = QStringLiteral("Custom Layout");
    m_activeTilingPreset.kdeJson = QJsonObject();

    if (origWin && !origWin->isDeleted()) {
        applyWindowToTilingSlot(origWin, g1);
    }

    refreshTilingSlotWindows();
    if (m_activeTilingSlots[activeSlotIdx + 1].window) {
        applyWindowToTilingSlot(m_activeTilingSlots[activeSlotIdx + 1].window.data(), g2);
    }

    syncActiveTilingSlotsToKdeTiles();
    playThemedSound(OutlineSound::SoundEffect::LandWindow);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::mergeTileSlots(int idx1, int idx2)
{
    if (idx1 < 0 || idx1 >= m_activeTilingSlots.size() || idx2 < 0 || idx2 >= m_activeTilingSlots.size() || idx1 == idx2) {
        return;
    }

    refreshTilingSlotWindows();

    const QRectF r1 = (idx1 < m_activeTilingPreset.tileSlots.size()) ? m_activeTilingPreset.tileSlots[idx1].relativeRect : QRectF();
    const QRectF r2 = (idx2 < m_activeTilingPreset.tileSlots.size()) ? m_activeTilingPreset.tileSlots[idx2].relativeRect : QRectF();
    const QRectF g1 = m_activeTilingSlots[idx1].geometry;
    const QRectF g2 = m_activeTilingSlots[idx2].geometry;

    EffectWindow *w1 = m_activeTilingSlots[idx1].window.data();
    EffectWindow *w2 = m_activeTilingSlots[idx2].window.data();

    if (!w1) w1 = findWindowInSlot(idx1);
    if (!w2) w2 = findWindowInSlot(idx2);

    if (w2 && !w2->isDeleted()) {
        if (w2->window()) {
            if (Tile *tile = w2->window()->tile()) {
                tile->unmanage(w2->window());
            }
            w2->window()->setTileCompatibility(nullptr);
            w2->window()->requestTile(nullptr);
        }
        m_floatingWindows.insert(w2);
    }

    const QRectF mergedRel = r1.united(r2);
    const QRectF mergedGeo = g1.united(g2);

    if (idx1 < m_activeTilingPreset.tileSlots.size()) {
        m_activeTilingPreset.tileSlots[idx1].relativeRect = mergedRel;
    }
    m_activeTilingSlots[idx1].geometry = mergedGeo;

    if (idx2 < m_activeTilingPreset.tileSlots.size()) {
        m_activeTilingPreset.tileSlots.removeAt(idx2);
    }
    m_activeTilingSlots.removeAt(idx2);

    m_activeTilingPreset.id = QStringLiteral("custom-layout");
    m_activeTilingPreset.name = QStringLiteral("Custom Layout");
    m_activeTilingPreset.kdeJson = QJsonObject();

    const int adjIdx1 = (idx1 > idx2) ? (idx1 - 1) : idx1;
    if (w1 && !w1->isDeleted()) {
        applyWindowToTilingSlot(w1, m_activeTilingSlots[adjIdx1].geometry);
    }

    syncActiveTilingSlotsToKdeTiles();
    playThemedSound(OutlineSound::SoundEffect::LandWindow);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::deleteTileSlot(int slotIdx)
{
    if (m_activeTilingSlots.size() <= 1 || slotIdx < 0 || slotIdx >= m_activeTilingSlots.size()) {
        return;
    }

    refreshTilingSlotWindows();

    const QRectF delRel = (slotIdx < m_activeTilingPreset.tileSlots.size())
        ? m_activeTilingPreset.tileSlots[slotIdx].relativeRect
        : QRectF();
    const QRectF delGeo = m_activeTilingSlots[slotIdx].geometry;
    EffectWindow *delWin = m_activeTilingSlots[slotIdx].window.data();
    if (!delWin) {
        delWin = findWindowInSlot(slotIdx);
    }

    if (delWin && !delWin->isDeleted()) {
        if (delWin->window()) {
            if (Tile *tile = delWin->window()->tile()) {
                tile->unmanage(delWin->window());
            }
            delWin->window()->setTileCompatibility(nullptr);
            delWin->window()->requestTile(nullptr);
        }
        m_floatingWindows.insert(delWin);
    }

    // Find best adjacent neighbor to absorb delRel
    int bestNeighbor = -1;
    qreal bestScore = -1.0;
    const qreal tol = 0.03;

    for (int i = 0; i < m_activeTilingPreset.tileSlots.size(); ++i) {
        if (i == slotIdx) continue;
        const QRectF &nRel = m_activeTilingPreset.tileSlots[i].relativeRect;

        // Vertical stack neighbor (same X bounds, touching vertically)
        const bool sameXSpan = (std::abs(nRel.left() - delRel.left()) < tol && std::abs(nRel.width() - delRel.width()) < tol);
        const bool touchesY = (std::abs(nRel.bottom() - delRel.top()) < tol || std::abs(nRel.top() - delRel.bottom()) < tol);
        if (sameXSpan && touchesY) {
            bestNeighbor = i;
            break;
        }

        // Horizontal column neighbor (same Y bounds, touching horizontally)
        const bool sameYSpan = (std::abs(nRel.top() - delRel.top()) < tol && std::abs(nRel.height() - delRel.height()) < tol);
        const bool touchesX = (std::abs(nRel.right() - delRel.left()) < tol || std::abs(nRel.left() - delRel.right()) < tol);
        if (sameYSpan && touchesX) {
            bestNeighbor = i;
            break;
        }

        qreal sharedEdge = 0.0;
        if (std::abs(nRel.bottom() - delRel.top()) < tol || std::abs(nRel.top() - delRel.bottom()) < tol) {
            sharedEdge = std::max(0.0, std::min(nRel.right(), delRel.right()) - std::max(nRel.left(), delRel.left()));
        }
        if (std::abs(nRel.right() - delRel.left()) < tol || std::abs(nRel.left() - delRel.right()) < tol) {
            sharedEdge = std::max(sharedEdge, std::max(0.0, std::min(nRel.bottom(), delRel.bottom()) - std::max(nRel.top(), delRel.top())));
        }
        if (sharedEdge > bestScore) {
            bestScore = sharedEdge;
            bestNeighbor = i;
        }
    }

    if (bestNeighbor < 0) {
        bestNeighbor = (slotIdx == 0) ? 1 : 0;
    }

    QRectF newRel = m_activeTilingPreset.tileSlots[bestNeighbor].relativeRect.united(delRel);
    QRectF newGeo = m_activeTilingSlots[bestNeighbor].geometry.united(delGeo);

    m_activeTilingPreset.tileSlots[bestNeighbor].relativeRect = newRel;
    m_activeTilingSlots[bestNeighbor].geometry = newGeo;

    if (slotIdx < m_activeTilingPreset.tileSlots.size()) {
        m_activeTilingPreset.tileSlots.removeAt(slotIdx);
    }
    m_activeTilingSlots.removeAt(slotIdx);

    m_activeTilingPreset.id = QStringLiteral("custom-layout");
    m_activeTilingPreset.name = QStringLiteral("Custom Layout");
    m_activeTilingPreset.kdeJson = QJsonObject();

    const int adjNeighbor = (bestNeighbor > slotIdx) ? (bestNeighbor - 1) : bestNeighbor;
    EffectWindow *nWin = m_activeTilingSlots[adjNeighbor].window.data();
    if (nWin && !nWin->isDeleted()) {
        applyWindowToTilingSlot(nWin, m_activeTilingSlots[adjNeighbor].geometry);
    }

    syncActiveTilingSlotsToKdeTiles();
    playThemedSound(OutlineSound::SoundEffect::Dismiss);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::performTilingResize(LeapKTiling::ResizeDirection dir, int count, LeapKTiling::TileEdge edge)
{
    if (m_activeTilingSlots.size() < 2) {
        return;
    }

    int activeSlotIdx = findTileSlotIndexForWindow(m_activeWindow);
    if (activeSlotIdx < 0) {
        activeSlotIdx = 0;
    }

    QRectF workArea;
    if (effects) {
        workArea = LeapKCompat::clientArea(effects, MaximizeArea, effects->activeScreen(), effects->currentDesktop());
    }
    if (workArea.isEmpty()) {
        workArea = QRectF(0, 0, 1920, 1080);
    }

    // Base step size: 5% of screen width / height per count unit
    const qreal delta = (dir == LeapKTiling::ResizeDirection::Left || dir == LeapKTiling::ResizeDirection::Right)
        ? workArea.width() * 0.05 * count
        : workArea.height() * 0.05 * count;

    QList<QRectF> geos;
    for (const auto &slot : m_activeTilingSlots) {
        geos.append(slot.geometry);
    }

    if (LeapKTiling::resizeSlotGeometries(geos, activeSlotIdx, dir, delta, 100.0, false, edge)) {
        for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
            m_activeTilingSlots[i].geometry = geos[i];
            EffectWindow *w = m_activeTilingSlots[i].window.data();
            if (w && !w->isDeleted() && w->window()) {
                applyWindowToTilingSlot(w, geos[i]);
            }
        }

        // Update active tiling preset slot proportions so state is kept in sync
        const qreal waW = workArea.width() > 0 ? workArea.width() : 1920.0;
        const qreal waH = workArea.height() > 0 ? workArea.height() : 1080.0;
        for (int i = 0; i < m_activeTilingSlots.size() && i < m_activeTilingPreset.tileSlots.size(); ++i) {
            m_activeTilingPreset.tileSlots[i].relativeRect = QRectF(
                qBound(0.0, (geos[i].x() - workArea.x()) / waW, 1.0),
                qBound(0.0, (geos[i].y() - workArea.y()) / waH, 1.0),
                qBound(0.05, geos[i].width() / waW, 1.0),
                qBound(0.05, geos[i].height() / waH, 1.0)
            );
        }
        const QString curWsId = workspaceIdForView(m_activeHudView);
        m_workspaceTilingPreset[curWsId] = m_activeTilingPreset;

        playThemedSound(OutlineSound::SoundEffect::NavTab);
        if (effects) {
            effects->addRepaintFull();
        }
    }
}

void ActiveWindowOutlineEffect::renderTilingEditOverlay(const RenderTarget &renderTarget,
                                                        const RenderViewport &viewport)
{
    if (m_activeTilingSlots.isEmpty()) {
        return;
    }

    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;
    const QRectF screenRect = viewport.renderRect();
    int activeSlotIdx = findTileSlotIndexForWindow(m_activeWindow);
    if (activeSlotIdx < 0 || activeSlotIdx >= m_activeTilingSlots.size()) {
        activeSlotIdx = 0;
    }

    const QColor themeAccent = theme.primaryAccentColor();
    const QColor themeSecondary = theme.secondaryAccentColor();
    const QString &badgeShape = theme.windowBadge.shape;

    // 1. Render badge on tile slots (active slot indicator & swap targets)
    const int numKeys = 14;
    for (int i = 0; i < m_activeTilingSlots.size(); ++i) {
        const QRectF geo = m_activeTilingSlots[i].geometry;
        if (i == activeSlotIdx) {
            // Active slot indicator: crisp themed floating pill
            const QString label = m_tilingMergeModeActive
                ? QStringLiteral("MERGE")
                : ((m_tilingEditMultiplier > 0)
                    ? QString::number(m_tilingEditMultiplier) + QLatin1Char('x')
                    : QStringLiteral("EDIT"));
            const QColor bgCol = m_tilingMergeModeActive
                ? QColor(230, 80, 0, 250)
                : ((m_tilingEditMultiplier > 0)
                    ? QColor(255, 150, 0, 245)
                    : (themeSecondary.isValid() ? themeSecondary : QColor(0, 160, 220, 245)));
            const QColor textCol = OutlineBadge::contrastingTextColor(bgCol);
            renderFloatingPill(renderTarget, viewport, label, geo.center(), bgCol, textCol, 16, 0.0, badgeShape);
        } else {
            // Other slot: swap target badge with distinct bold letter (themed shape floating pill)
            const QChar letter = LeapKTiling::kTileEditKeys[i % numKeys];
            const QColor bgCol = themeAccent.isValid() ? themeAccent : QColor(0, 122, 255, 240);
            const QColor textCol = OutlineBadge::contrastingTextColor(bgCol);
            renderFloatingPill(renderTarget, viewport, QString(letter), geo.center(), bgCol, textCol, 20, 0.0, badgeShape);
        }
    }

    // 2. Render bottom hint bar pill with crisp 1:1 pixel rendering and active theme styling
    QString hintText;
    if (m_tilingMergeModeActive) {
        hintText = QStringLiteral("Merge with tile: [Press Letter of adjacent tile to merge]  •  [Esc: Cancel]");
    } else if (m_tilingEditMultiplier > 0) {
        hintText = QStringLiteral("Multiplier: %1x  •  [H/←: Narrower  L/→: Wider  K/↑: Shorter  J/↓: Taller]  •  [Ctrl+S: Save]  •  [Enter: Finish]").arg(m_tilingEditMultiplier);
    } else {
        hintText = QStringLiteral("[H/←: Narrower  L/→: Wider  K/↑: Shorter  J/↓: Taller]  •  [Ctrl+V/H: Split V/H]  •  [Ctrl+M: Merge  Ctrl+F: Float  Ctrl+D: Delete]  •  [Letters: Swap]  •  [Enter: Done]");
    }
    const QPointF barCenter(screenRect.center().x(), screenRect.bottom() - 36.0);
    const QColor barBg = theme.surfaceBackgroundColor();
    const QColor barText = theme.textSecondaryColor();
    renderFloatingPill(renderTarget, viewport, hintText, barCenter, barBg, barText, 12, screenRect.width() - 40.0, theme.hudContainer.shape);
}

void ActiveWindowOutlineEffect::renderTilingPresetPills(const RenderTarget &renderTarget,
                                                        const RenderViewport &viewport,
                                                        const QList<LeapKTiling::TilingPreset> &presets)
{
    if (presets.isEmpty()) {
        return;
    }

    const qreal scale = viewport.scale();
    const QRectF screenRect = viewport.renderRect();
    const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;

    struct PillItem {
        std::shared_ptr<GLTexture> texture;
        qreal width;
        qreal height;
    };
    QList<PillItem> pills;

    for (int i = 0; i < presets.size(); ++i) {
        const auto &preset = presets.at(i);
        const bool isActive = m_tilingActive && (m_activeTilingPreset.id == preset.id);

        const QImage img = OutlineBadge::renderTilingPresetPill(cfg, preset, scale, isActive, false);
        auto uploaded = GLTexture::upload(img);
        if (!uploaded) {
            continue;
        }
        uploaded->setFilter(GL_LINEAR);
        uploaded->setWrapMode(GL_CLAMP_TO_EDGE);

        const qreal w = img.width() / scale;
        const qreal h = img.height() / scale;
        pills.append({std::shared_ptr<GLTexture>(uploaded.release()), w, h});
    }

    if (pills.isEmpty()) {
        return;
    }

    QList<QSizeF> pillSizes;
    pillSizes.reserve(pills.size());
    for (const PillItem &item : pills) {
        pillSizes.append(QSizeF(item.width, item.height));
    }

    const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
    const qreal headerH = ensureHudHeaderTexture(scale).height();
    const qreal effMargin = computeHudEffectiveMargin(hudPos, scale, screenRect, headerH);
    const QList<QRectF> rects = OutlineBadge::computeHudPillPositions(pillSizes, screenRect, hudPos, effMargin, 12.0, 12.0, -1, 28.0, nullptr);

    m_hudTilingPresetPillRects.clear();
    for (int i = 0; i < presets.size() && i < rects.size(); ++i) {
        m_hudTilingPresetPillRects.append({rects.at(i), presets.at(i)});
    }

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLint previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousDstAlpha);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);

    ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::TransformColorspace);
    GLShader *shader = binder.shader();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    for (int i = 0; i < pills.size(); ++i) {
        const PillItem &item = pills.at(i);
        const QRectF rect = rects.at(i);

        const float x0 = rect.left() * scale;
        const float y0 = rect.top() * scale;
        const float x1 = rect.right() * scale;
        const float y1 = rect.bottom() * scale;

        const std::array<GLVertex2D, 6> verts = {{
            {QVector2D(x0, y0), QVector2D(0.0f, 0.0f)},
            {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
            {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
            {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
            {QVector2D(x1, y1), QVector2D(1.0f, 1.0f)},
            {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
        }};

        auto vbo = GLVertexBuffer::streamingBuffer();
        vbo->setVertices(verts);

        shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, item.texture->matrix(NormalizedCoordinates));
        item.texture->bind();
        vbo->render(GL_TRIANGLES);
        item.texture->unbind();
    }

    if (scissorWasEnabled) {
        glEnable(GL_SCISSOR_TEST);
    }

    glBlendFuncSeparate(previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha);
    if (blendWasEnabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }

    QRectF allPillsBounds;
    for (const QRectF &r : rects) {
        allPillsBounds = allPillsBounds.isEmpty() ? r : allPillsBounds.united(r);
    }
    const QRectF hintRect = renderHudHintBar(renderTarget, viewport, allPillsBounds);
    renderHudModeHeader(renderTarget, viewport, allPillsBounds, hintRect);
}

void ActiveWindowOutlineEffect::checkKdeTilesEditorState()
{
    const bool isActive = (effects && effects->isEffectActive(QStringLiteral("tileseditor")));
    if (isActive) {
        if (!m_kdeTilesEditorWasActive) {
            m_kdeTilesEditorWasActive = true;
            if (m_tilingEditModeActive) {
                exitTilingEditMode();
            }
            if (m_tilingPresetsModeActive) {
                exitTilingPresetsMode();
            }
            if (m_jumpMode) {
                cancelJumpMode();
            }
        }
    } else {
        if (m_kdeTilesEditorWasActive) {
            m_kdeTilesEditorWasActive = false;
            onKdeTilesEditorClosed();
        }
    }
}

void ActiveWindowOutlineEffect::onKdeTilesEditorClosed()
{
    LogicalOutput *activeOutput = effects ? effects->activeScreen() : nullptr;
    VirtualDesktop *currentDesktop = effects ? effects->currentDesktop() : nullptr;
    TileManager *tileMgr = (workspace() && activeOutput) ? workspace()->tileManager(activeOutput) : nullptr;
    RootTile *rootTile = (tileMgr && currentDesktop) ? tileMgr->rootTile(currentDesktop) : nullptr;
    const QString outUuid = activeOutput ? activeOutput->uuid() : QString();
    const QString deskId = currentDesktop ? currentDesktop->id() : QString();

    if (tileMgr) {
        tileMgr->saveSettings();
    }

    QList<QRectF> liveRects;
    if (rootTile) {
        for (Tile *t : rootTile->descendants()) {
            if (!t->isLayout()) {
                const QRectF rel = t->relativeGeometry();
                if (rel.isValid() && rel.width() > 0.02 && rel.height() > 0.02) {
                    liveRects.append(rel);
                }
            }
        }
    }

    std::stable_sort(liveRects.begin(), liveRects.end(), [](const QRectF &a, const QRectF &b) {
        if (std::abs(a.top() - b.top()) > 0.04) {
            return a.top() < b.top();
        }
        return a.left() < b.left();
    });

    QJsonObject liveJson;
    if (effects && effects->config()) {
        effects->config()->reparseConfiguration();
        KConfigGroup tilingCg = effects->config()->group(QStringLiteral("Tiling"));
        QString tilesStr;
        if (!deskId.isEmpty() && !outUuid.isEmpty() && tilingCg.hasGroup(deskId)) {
            KConfigGroup deskGroup(&tilingCg, deskId);
            if (deskGroup.hasGroup(outUuid)) {
                tilesStr = deskGroup.group(outUuid).readEntry("tiles", QString());
            }
        }
        if (tilesStr.isEmpty() && !outUuid.isEmpty() && !deskId.isEmpty() && tilingCg.hasGroup(outUuid)) {
            KConfigGroup outGroup(&tilingCg, outUuid);
            if (outGroup.hasGroup(deskId)) {
                tilesStr = outGroup.group(deskId).readEntry("tiles", QString());
            }
        }
        if (tilesStr.isEmpty() && !outUuid.isEmpty() && tilingCg.hasGroup(outUuid)) {
            tilesStr = tilingCg.group(outUuid).readEntry("tiles", QString());
        }
        if (!tilesStr.isEmpty()) {
            liveJson = QJsonDocument::fromJson(tilesStr.toUtf8()).object();
        }
    }

    // Sanitize: never accept floating layout from kwinrc or live editor
    if (!liveJson.isEmpty()) {
        const QString dir = liveJson.value(QStringLiteral("layoutDirection")).toString();
        if (dir != QStringLiteral("horizontal") && dir != QStringLiteral("vertical")) {
            liveJson = QJsonObject();
        }
    }

    LeapKTiling::TilingPreset kdePreset;
    if (!liveRects.isEmpty()) {
        kdePreset = LeapKTiling::createKdeCustomPreset(liveRects, liveJson);
    } else if (!liveJson.isEmpty()) {
        kdePreset = LeapKTiling::createKdeCustomPreset(liveJson);
    } else {
        kdePreset = LeapKTiling::createKdeCustomPreset({
            QRectF(0.0, 0.0, 0.5, 1.0),
            QRectF(0.5, 0.0, 0.5, 1.0)
        });
    }

    kdePreset.id = QStringLiteral("kde-custom");
    kdePreset.name = QStringLiteral("KDE Custom");
    kdePreset.triggerKey = QLatin1Char('K');

    m_savedKdeCustomTilesJson = !liveJson.isEmpty() ? liveJson : kdePreset.toKdeJson();
    kdePreset.kdeJson = m_savedKdeCustomTilesJson;

    if (effects && effects->config() && !deskId.isEmpty() && !outUuid.isEmpty()) {
        const QByteArray jsonBytes = QJsonDocument(m_savedKdeCustomTilesJson).toJson(QJsonDocument::Compact);
        KConfigGroup leapkCg = effects->config()->group(QStringLiteral("Effect-leapk"));
        leapkCg.writeEntry(QStringLiteral("CustomTilingBackup_%1_%2").arg(deskId, outUuid), jsonBytes);
        leapkCg.sync();

        // Also sanitize [Tiling][deskId][outUuid] if it was floating
        KConfigGroup tilingCg = effects->config()->group(QStringLiteral("Tiling"));
        KConfigGroup deskGroup(&tilingCg, deskId);
        KConfigGroup outTileGroup(&deskGroup, outUuid);
        const QString curTilesStr = outTileGroup.readEntry("tiles", QString());
        if (curTilesStr.contains(QStringLiteral("\"floating\""))) {
            outTileGroup.writeEntry("tiles", jsonBytes);
            tilingCg.sync();
        }
    }

    if (rootTile && rootTile->layoutDirection() == Tile::LayoutDirection::Floating && tileMgr) {
        while (rootTile->childCount() > 0) {
            rootTile->destroyChild(rootTile->childTile(0));
        }
        tileMgr->readSettings(rootTile);
    }

    refreshTilingPresetsWithKdeCustom();

    if (!kdePreset.tileSlots.isEmpty()) {
        activateTilingPreset(kdePreset, false);
        showTilingToast(QStringLiteral("▤ Layout updated: [K] KDE Custom  •  Press [Ctrl+S] in edit mode to save"));
    }
}

void ActiveWindowOutlineEffect::showTilingToast(const QString &message)
{
    m_tilingToastMessage = message;
    m_tilingToastTimer.restart();
    if (!m_tilingToastClearTimer) {
        m_tilingToastClearTimer = new QTimer(this);
        m_tilingToastClearTimer->setSingleShot(true);
        connect(m_tilingToastClearTimer, &QTimer::timeout, this, [this]() {
            m_tilingToastMessage.clear();
            if (effects) {
                effects->addRepaintFull();
            }
        });
    }
    m_tilingToastClearTimer->start(4500);
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::renderTilingToast(const RenderTarget &renderTarget,
                                                  const RenderViewport &viewport)
{
    if (m_tilingToastMessage.isEmpty()) {
        return;
    }

    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;
    const QRectF screenRect = viewport.renderRect();
    const QPointF toastCenter(screenRect.center().x(), screenRect.bottom() - 48.0);
    const QColor toastBg = theme.surfaceBackgroundColor();
    const QColor toastText = theme.textPrimaryColor();
    renderFloatingPill(renderTarget, viewport, m_tilingToastMessage, toastCenter, toastBg, toastText, 12, screenRect.width() - 40.0, theme.hudContainer.shape);
}

} // namespace KWin
