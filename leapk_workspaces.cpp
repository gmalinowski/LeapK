/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk_internal.h"

namespace KWin
{

void ActiveWindowOutlineEffect::applyPersistentMarksToWindow(EffectWindow *w)
{
    if (!w || w->isDeleted() || m_markedWindows.contains(w) || !isEligibleTargetWindow(w)) {
        return;
    }
    const auto desktops = w->desktops();
    const int desktopNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
    const auto *rule = LeapKMarks::findBestMatchingRule(m_persistentMarks, w->windowClass(), w->caption(), desktopNum, w->pid());
    if (rule) {
        setWindowMark(w, rule->color, rule->letter, rule->label);
    } else if (w->windowClass().isEmpty()) {
        // Wayland app_id may not be set when the window is first added; re-check after brief delay
        QPointer<EffectWindow> pw(w);
        QTimer::singleShot(100, this, [this, pw]() {
            if (pw && !pw->isDeleted() && !m_markedWindows.contains(pw.data())) {
                applyPersistentMarksToWindow(pw.data());
            }
        });
    }
}

void ActiveWindowOutlineEffect::refreshPersistentMarksForWindows()
{
    if (m_persistentMarks.isEmpty() || !effects) {
        return;
    }
    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        if (!w || w->isDeleted() || !isEligibleTargetWindow(w)) {
            continue;
        }
        const auto desktops = w->desktops();
        const int desktopNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
        const auto *rule = LeapKMarks::findBestMatchingRule(m_persistentMarks, w->windowClass(), w->caption(), desktopNum, w->pid());
        if (rule) {
            const bool marked = m_markedWindows.contains(w);
            const QChar curLetter = m_markedWindowLetters.value(w);
            const QColor curColor = m_markedWindows.value(w);
            const QString curLabel = m_markedWindowLabels.value(w);
            if (!marked || curLetter != rule->letter || curColor != rule->color || curLabel != rule->label) {
                setWindowMark(w, rule->color, rule->letter, rule->label);
            }
        }
    }
}

void ActiveWindowOutlineEffect::setWindowMark(EffectWindow *w, const QColor &color, const QChar &letter, const QString &label)
{
    if (!isEligibleOutlineWindow(w) || !color.isValid()) {
        return;
    }
    m_markedWindows[w] = color;
    m_lastMarkColor = color;

    QChar finalLetter = letter.toUpper();
    if (finalLetter.isNull() || !finalLetter.isLetterOrNumber()) {
        m_markedWindowLetters.remove(w);
    } else {
        m_markedWindowLetters[w] = finalLetter;
    }

    const QString cleanLabel = label.trimmed();
    if (cleanLabel.isEmpty()) {
        m_markedWindowLabels.remove(w);
    } else {
        m_markedWindowLabels[w] = cleanLabel;
    }

    effects->addRepaint(expandedGeometryFor(w));
    if (m_badgeDisplayMode == 1 || m_jumpMode) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::setWindowMarkColor(EffectWindow *w, const QColor &color)
{
    setWindowMark(w, color, m_markedWindowLetters.value(w), m_markedWindowLabels.value(w));
}

QChar ActiveWindowOutlineEffect::nextAvailableLetter() const
{
    QList<QChar> used;
    for (auto it = m_markedWindowLetters.constBegin(); it != m_markedWindowLetters.constEnd(); ++it) {
        used.append(it.value());
    }
    return OutlineBadge::findNextAvailableLetter(used);
}

QColor ActiveWindowOutlineEffect::generateRandomMarkColor() const
{
    return OutlineBadge::generateRandomMarkColor(m_markedWindows.values());
}

QColor ActiveWindowOutlineEffect::detectSmartAccentForWindow(EffectWindow *w) const
{
    if (!w) {
        return generateRandomMarkColor();
    }

    QColor candidate;

    // 1. Cached live Smart Adaptive color sampled from window pixels
    if (m_smartAccentCache.contains(w) && m_smartAccentCache.value(w).isValid()) {
        candidate = m_smartAccentCache.value(w);
    }

    // 2. Extract smart adaptive accent from window icon using OutlineBadge helper
    if (!candidate.isValid()) {
        const QIcon icon = w->icon();
        if (!icon.isNull()) {
            const QPixmap pix = icon.pixmap(QSize(32, 32));
            if (!pix.isNull()) {
                const QColor iconColor = OutlineBadge::extractSmartAccentFromImage(
                    pix.toImage(),
                    m_ambilightSaturationLeveling / 100.0,
                    m_ambilightFallbackStrength / 100.0,
                    m_ambilightMinBrightness / 100.0,
                    m_ambilightMaxBrightness / 100.0);
                if (iconColor.isValid() && iconColor.saturationF() > 0.10) {
                    m_smartAccentCache[w] = iconColor;
                    candidate = iconColor;
                }
            }
        }
    }

    // 3. Window live ambilight color
    if (!candidate.isValid()) {
        if (w == m_activeWindow && m_ambilightCurrentColor.isValid()) {
            candidate = m_ambilightCurrentColor;
        } else if (m_recentAmbilightColors.contains(w) && m_recentAmbilightColors.value(w).isValid()) {
            candidate = m_recentAmbilightColors.value(w);
        }
    }

    // Check candidate against existing marked window colors to prevent duplicate/colliding accents (e.g. repeated Tuxedo green)
    const auto existingColors = m_markedWindows.values();
    if (candidate.isValid()) {
        bool collides = false;
        for (const QColor &used : existingColors) {
            if (OutlineBadge::isColorTooClose(candidate, used)) {
                collides = true;
                break;
            }
        }
        if (!collides) {
            return candidate;
        }

        // Collision detected: try shifting hue in steps of 60 degrees to find a contrasting distinct hue
        if (candidate.saturationF() > 0.15) {
            for (int step = 1; step <= 5; ++step) {
                const int shiftedHue = (candidate.hue() + step * 60) % 360;
                const QColor shifted = QColor::fromHsv(shiftedHue, candidate.saturation(), candidate.value());
                bool shiftCollision = false;
                for (const QColor &used : existingColors) {
                    if (OutlineBadge::isColorTooClose(shifted, used)) {
                        shiftCollision = true;
                        break;
                    }
                }
                if (!shiftCollision) {
                    return shifted;
                }
            }
        }
    }

    // 4. Fallback: random mark color guaranteed not to collide with used colors
    return generateRandomMarkColor();
}

void ActiveWindowOutlineEffect::toggleWindowMark(EffectWindow *w)
{
    if (!isEligibleOutlineWindow(w)) {
        return;
    }
    if (m_markedWindows.contains(w)) {
        m_markedWindows.remove(w);
        m_markedWindowLetters.remove(w);
        m_markedWindowLabels.remove(w);
        effects->addRepaint(expandedGeometryFor(w));
    } else {
        const QColor markCol = m_markedWindows.contains(w)
            ? m_markedWindows.value(w)
            : (m_markRandomColor
                ? generateRandomMarkColor()
                : (m_markSmartAdaptive
                    ? detectSmartAccentForWindow(w)
                    : (m_lastMarkColor.isValid() ? m_lastMarkColor : m_markDefaultColor)));
        const QChar letter = nextAvailableLetter();
        setWindowMark(w, markCol, letter);
    }
}

void ActiveWindowOutlineEffect::slotMarkWindowToggle()
{
    EffectWindow *w = effects->activeWindow();
    if (!isEligibleOutlineWindow(w)) {
        w = m_activeWindow;
    }
    if (isEligibleOutlineWindow(w)) {
        toggleWindowMark(w);
    }
}

void ActiveWindowOutlineEffect::slotMarkWindowWithColor()
{
    EffectWindow *target = effects->activeWindow();
    if (!isEligibleOutlineWindow(target)) {
        target = m_activeWindow;
    }
    if (!isEligibleOutlineWindow(target)) {
        return;
    }

    QPointer<EffectWindow> targetWindow(target);
    const QColor smartCol = detectSmartAccentForWindow(target);
    const QColor initialColor = m_markedWindows.contains(target)
        ? m_markedWindows.value(target)
        : (m_markRandomColor
            ? generateRandomMarkColor()
            : ((m_markSmartAdaptive && smartCol.isValid())
                ? smartCol
                : (m_lastMarkColor.isValid() ? m_lastMarkColor : m_markDefaultColor)));
    const QChar initialLetter = m_markedWindowLetters.contains(target)
        ? m_markedWindowLetters.value(target)
        : nextAvailableLetter();
    const QString initialLabel = m_markedWindowLabels.value(target);

    const auto applyMark = [this, targetWindow](const QColor &pickedCol, const QChar &pickedLetter, const QString &pickedLabel, bool useColor = true, bool useLetter = true) {
        if (!targetWindow) {
            return;
        }
        EffectWindow *w = targetWindow.data();
        const QColor markCol = (useColor && pickedCol.isValid())
            ? pickedCol
            : (m_markedWindows.contains(w)
                ? m_markedWindows.value(w)
                : (m_markRandomColor
                    ? generateRandomMarkColor()
                    : (m_markSmartAdaptive
                        ? detectSmartAccentForWindow(w)
                        : (m_lastMarkColor.isValid() ? m_lastMarkColor : m_markDefaultColor))));

        const QChar letter = (useLetter && pickedLetter.isLetterOrNumber())
            ? pickedLetter.toUpper()
            : QChar();

        setWindowMark(w, markCol, letter, pickedLabel);
    };

    // 1. Dedicated Qt6 color picker helper (natively supports alpha channel, letter badge, and custom label)
    QStringList candidateDirs;
    candidateDirs << QDir::homePath() + QStringLiteral("/.local/bin");
#ifdef COLORPICKER_INSTALL_DIR
    candidateDirs << QStringLiteral(COLORPICKER_INSTALL_DIR);
#endif
    candidateDirs << QCoreApplication::applicationDirPath()
                  << QStringLiteral("/usr/libexec")
                  << QStringLiteral("/usr/lib/libexec")
                  << QStringLiteral("/usr/local/bin")
                  << QStringLiteral("/usr/bin");

    QString customPicker = QStandardPaths::findExecutable(QStringLiteral("leapk-custom-marker"), candidateDirs);
    if (customPicker.isEmpty()) {
        customPicker = QStandardPaths::findExecutable(QStringLiteral("leapk-colorpicker"), candidateDirs);
    }
    if (!customPicker.isEmpty()) {
        QProcess *process = new QProcess(this);
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [this, process, applyMark, targetWindow](int exitCode, QProcess::ExitStatus exitStatus) {
            process->deleteLater();
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                if (!out.isEmpty()) {
                    QString colorStr;
                    QString letterStr;
                    QString labelStr;
                    QString targetAppClass;
                    bool persist = true;
                    bool groupByApp = true;
                    QString matchCaption;

                    int matchDesktop = 0;
                    bool reassign = false;

                    if (out.startsWith(QLatin1Char('{'))) {
                        const QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8());
                        if (doc.isObject()) {
                            const QJsonObject obj = doc.object();
                            if (obj.value(QStringLiteral("action")).toString() == QStringLiteral("remove")) {
                                const bool removePersistent = obj.value(QStringLiteral("removePersistent")).toBool(true);
                                if (targetWindow) {
                                    EffectWindow *w = targetWindow.data();
                                    m_markedWindows.remove(w);
                                    m_markedWindowLetters.remove(w);
                                    m_markedWindowLabels.remove(w);
                                    if (removePersistent) {
                                        const auto desktops = w->desktops();
                                        const int dNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
                                        for (int i = m_persistentMarks.size() - 1; i >= 0; --i) {
                                            const auto &r = m_persistentMarks.at(i);
                                            if (r.matchApp.compare(w->windowClass(), Qt::CaseInsensitive) == 0) {
                                                if (r.matchDesktop == 0 || r.matchDesktop == dNum) {
                                                    m_persistentMarks.removeAt(i);
                                                }
                                            }
                                        }
                                        LeapKMarks::savePersistentMarks(m_persistentMarks);
                                    }
                                    if (effects) {
                                        effects->addRepaint(expandedGeometryFor(w));
                                        effects->addRepaintFull();
                                    }
                                }
                                return;
                            }
                            colorStr = obj.value(QStringLiteral("color")).toString();
                            letterStr = obj.value(QStringLiteral("letter")).toString();
                            labelStr = obj.value(QStringLiteral("label")).toString();
                            persist = obj.value(QStringLiteral("persist")).toBool(true);
                            groupByApp = obj.value(QStringLiteral("groupByApp")).toBool(true);
                            matchCaption = obj.value(QStringLiteral("matchCaption")).toString().trimmed();
                            matchDesktop = obj.value(QStringLiteral("matchDesktop")).toInt(0);
                            reassign = obj.value(QStringLiteral("reassign")).toBool(false);
                            targetAppClass = obj.value(QStringLiteral("appClass")).toString().trimmed();
                        }
                    } else {
                        // Legacy space-separated fallback
                        const int firstSpace = out.indexOf(QLatin1Char(' '));
                        if (firstSpace != -1) {
                            colorStr = out.left(firstSpace).trimmed();
                            const int secondSpace = out.indexOf(QLatin1Char(' '), firstSpace + 1);
                            if (secondSpace != -1) {
                                letterStr = out.mid(firstSpace + 1, secondSpace - firstSpace - 1).trimmed();
                                const int thirdSpace = out.lastIndexOf(QLatin1Char(' '));
                                if (thirdSpace > secondSpace) {
                                    labelStr = out.mid(secondSpace + 1, thirdSpace - secondSpace - 1).trimmed();
                                    const QString persistStr = out.mid(thirdSpace + 1).trimmed();
                                    persist = (persistStr == QStringLiteral("1") || persistStr == QStringLiteral("true"));
                                } else {
                                    labelStr = out.mid(secondSpace + 1).trimmed();
                                }
                            } else {
                                letterStr = out.mid(firstSpace + 1).trimmed();
                            }
                        }
                    }

                    if (labelStr == QStringLiteral("-")) {
                        labelStr.clear();
                    }

                    const bool useColor = (colorStr.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0
                                        && colorStr.compare(QStringLiteral("auto"), Qt::CaseInsensitive) != 0
                                        && colorStr != QStringLiteral("-"));
                    const bool useLetter = (!letterStr.isEmpty()
                                         && letterStr.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0
                                         && letterStr.compare(QStringLiteral("auto"), Qt::CaseInsensitive) != 0
                                         && letterStr != QStringLiteral("-"));

                    const QColor picked = useColor ? QColor(colorStr) : QColor();
                    const QChar pickedLetter = useLetter ? letterStr.at(0).toUpper() : QChar();

                    applyMark(picked, pickedLetter, labelStr, useColor, useLetter);

                    EffectWindow *w = targetWindow ? targetWindow.data() : nullptr;
                    const QString appClass = !targetAppClass.isEmpty()
                        ? targetAppClass
                        : (w ? w->windowClass() : QString());

                    if (reassign && !pickedLetter.isNull() && !appClass.isEmpty()) {
                        // Reassign: if any other window belonging to a DIFFERENT application held this letter, release it
                        if (effects) {
                            for (auto it = m_markedWindowLetters.begin(); it != m_markedWindowLetters.end(); ) {
                                EffectWindow *other = it.key();
                                if (other && other != w && it.value() == pickedLetter) {
                                    if (other->windowClass().compare(appClass, Qt::CaseInsensitive) != 0) {
                                        it = m_markedWindowLetters.erase(it);
                                        effects->addRepaint(expandedGeometryFor(other));
                                        continue;
                                    }
                                }
                                ++it;
                            }
                        }

                        // Also clean up any persistent mark rule for a different app that claimed this letter
                        for (int i = m_persistentMarks.size() - 1; i >= 0; --i) {
                            const auto &r = m_persistentMarks.at(i);
                            if (r.letter == pickedLetter && r.matchApp.compare(appClass, Qt::CaseInsensitive) != 0) {
                                m_persistentMarks.removeAt(i);
                            }
                        }
                    }

                    const QColor effectiveMarkColor = w
                        ? m_markedWindows.value(w, picked.isValid() ? picked : m_markDefaultColor)
                        : (picked.isValid() ? picked : m_markDefaultColor);

                    if (groupByApp && effects && !appClass.isEmpty()) {
                        // Apply mark to all open windows of this application matching matchCaption
                        const auto allWindows = effects->stackingOrder();
                        for (EffectWindow *other : allWindows) {
                            if (other && other != w && isEligibleTargetWindow(other)) {
                                if (other->windowClass().compare(appClass, Qt::CaseInsensitive) == 0) {
                                    if (matchCaption.isEmpty() || other->caption().contains(matchCaption, Qt::CaseInsensitive)) {
                                        setWindowMark(other, effectiveMarkColor, pickedLetter, labelStr);
                                    }
                                }
                            }
                        }
                    }

                    if (persist && (!appClass.isEmpty() || !matchCaption.isEmpty())) {
                        LeapKMarks::PersistentMarkRule rule;
                        rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                        rule.letter = pickedLetter;
                        rule.color = effectiveMarkColor;
                        rule.label = labelStr;
                        rule.matchApp = appClass;
                        rule.matchCaption = matchCaption;
                        rule.matchDesktop = matchDesktop;
                        rule.enabled = true;
                        rule.createdAt = QDateTime::currentMSecsSinceEpoch();

                        bool updated = false;
                        for (auto &r : m_persistentMarks) {
                            if (r.matchApp.compare(rule.matchApp, Qt::CaseInsensitive) == 0
                                && r.matchCaption.compare(rule.matchCaption, Qt::CaseInsensitive) == 0
                                && r.matchDesktop == rule.matchDesktop) {
                                r.letter = rule.letter;
                                r.color = rule.color;
                                r.label = rule.label;
                                r.enabled = true;
                                updated = true;
                                break;
                            }
                        }
                        if (!updated) {
                            m_persistentMarks.append(rule);
                        }
                        LeapKMarks::savePersistentMarks(m_persistentMarks);
                    }

                    if (effects) {
                        effects->addRepaintFull();
                    }
                }
            }
        });
        connect(process, &QProcess::errorOccurred, this, [process]() {
            process->deleteLater();
        });

        const auto desktops = target->desktops();
        const int desktopNum = (!desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
        const QString desktopName = (!desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->name() : QString();
        const bool isAlreadyMarked = m_markedWindows.contains(target);

        QJsonObject usedLettersObj;
        for (auto it = m_markedWindowLetters.constBegin(); it != m_markedWindowLetters.constEnd(); ++it) {
            EffectWindow *ew = it.key();
            const QChar ch = it.value().toUpper();
            if (ew && !ch.isNull()) {
                QString app = ew->windowClass();
                if (app.isEmpty()) {
                    app = ew->caption().left(25);
                }
                usedLettersObj[QString(ch)] = app;
            }
        }
        for (const auto &r : m_persistentMarks) {
            if (!r.letter.isNull() && !r.matchApp.isEmpty()) {
                const QString chStr = QString(r.letter).toUpper();
                if (!usedLettersObj.contains(chStr)) {
                    usedLettersObj[chStr] = r.matchApp;
                }
            }
        }

        QStringList args;
        if (!m_badgeConfig.activeThemeId.isEmpty()) {
            args << QStringLiteral("--theme") << m_badgeConfig.activeThemeId;
        }
        args << initialColor.name(QColor::HexArgb) << QString(initialLetter);
        if (isAlreadyMarked) {
            args << (m_markedWindowLetters.contains(target) ? QStringLiteral("1") : QStringLiteral("0"));
            args << QStringLiteral("1");
            args << initialLabel;
        } else {
            args << QStringLiteral("1");
            args << QStringLiteral("1");
            args << initialLabel;
        }
        args << QStringLiteral("-1"); // Pass -1 to preserve saved persistence state from QSettings
        args << target->windowClass();
        args << target->caption();
        args << QString::number(desktopNum);
        args << desktopName;
        args << (isAlreadyMarked ? QStringLiteral("1") : QStringLiteral("0"));
        args << QString::fromUtf8(QJsonDocument(usedLettersObj).toJson(QJsonDocument::Compact));
        process->start(customPicker, args);
        return;
    }

    // 2. Zenity color picker with alpha support
    const QString zenityPath = QStandardPaths::findExecutable(QStringLiteral("zenity"));
    if (!zenityPath.isEmpty()) {
        QProcess *process = new QProcess(this);
        QStringList args;
        args << QStringLiteral("--color-selection")
             << QStringLiteral("--show-palette")
             << QStringLiteral("--color=%1").arg(initialColor.name(QColor::HexArgb))
             << QStringLiteral("--title") << tr("Select Window Mark Color");

        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [process, applyMark, initialLetter, initialLabel](int exitCode, QProcess::ExitStatus exitStatus) {
            process->deleteLater();
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                if (!out.isEmpty()) {
                    applyMark(QColor(out), initialLetter, initialLabel, true, true);
                }
            }
        });
        connect(process, &QProcess::errorOccurred, this, [process]() {
            process->deleteLater();
        });
        process->start(zenityPath, args);
        return;
    }

    // 3. KDialog fallback
    const QString kdialogPath = QStandardPaths::findExecutable(QStringLiteral("kdialog"));
    if (!kdialogPath.isEmpty()) {
        QProcess *process = new QProcess(this);
        QStringList args;
        args << QStringLiteral("--getcolor")
             << QStringLiteral("--default") << initialColor.name(QColor::HexArgb)
             << QStringLiteral("--title") << tr("Select Window Mark Color");

        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [process, applyMark, initialLetter, initialLabel](int exitCode, QProcess::ExitStatus exitStatus) {
            process->deleteLater();
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                if (!out.isEmpty()) {
                    applyMark(QColor(out), initialLetter, initialLabel, true, true);
                }
            }
        });
        connect(process, &QProcess::errorOccurred, this, [process]() {
            process->deleteLater();
        });
        process->start(kdialogPath, args);
        return;
    }

    // 4. Non-modal QColorDialog with ShowAlphaChannel fallback
    QColorDialog *dialog = new QColorDialog(initialColor);
    dialog->setOption(QColorDialog::ShowAlphaChannel, true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Select Window Mark Color"));
    connect(dialog, &QColorDialog::colorSelected, this, [applyMark, initialLetter, initialLabel](const QColor &color) {
        applyMark(color, initialLetter, initialLabel, true, true);
    });
    dialog->show();
}

void ActiveWindowOutlineEffect::refreshWorkspaces()
{
    m_workspaces = LeapKWorkspace::loadWorkspaces();
    for (const auto &ws : m_workspaces) {
        if (!ws.lastVirtualDesktopId.isEmpty()) {
            m_workspaceLastVirtualDesktopId[ws.id] = ws.lastVirtualDesktopId;
        }
        if (!ws.tilingPresetId.isEmpty()) {
            for (const auto &p : m_tilingPresets) {
                if (p.id == ws.tilingPresetId) {
                    m_workspaceTilingActive[ws.id] = true;
                    m_workspaceTilingPreset[ws.id] = p;
                    break;
                }
            }
        }
    }
    for (auto it = m_windowWorkspace.begin(); it != m_windowWorkspace.end(); ++it) {
        if (it.value() > m_workspaces.size()) {
            *it = 0;
        }
    }
    const int totalViews = LeapKWorkspace::computeTotalWorkspaceViews(m_workspaces.size());
    if (m_activeHudView >= totalViews || m_activeHudView < 0) {
        m_activeHudView = 0;
        if (effects && effects->config()) {
            KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
            primaryConfig.writeEntry("ActiveHudView", m_activeHudView);
            primaryConfig.sync();
        }
    }
}

QString ActiveWindowOutlineEffect::workspaceIdForView(int hudViewIndex) const
{
    return LeapKWorkspace::workspaceIdForHudView(hudViewIndex, m_workspaces);
}

void ActiveWindowOutlineEffect::switchToWorkspaceView(int newHudViewIndex)
{
    refreshWorkspaces();
    const int totalViews = LeapKWorkspace::computeTotalWorkspaceViews(m_workspaces.size());
    if (newHudViewIndex < 0 || newHudViewIndex >= totalViews) {
        newHudViewIndex = 0;
    }

    if (m_activeHudView == newHudViewIndex) {
        return;
    }

    const bool wasInJumpMode = m_jumpMode;
    const int oldHudView = m_activeHudView;
    const QString oldWsId = workspaceIdForView(oldHudView);
    const QString newWsId = workspaceIdForView(newHudViewIndex);

    // 1. Save outgoing workspace state
    if (effects && effects->currentDesktop()) {
        m_workspaceLastVirtualDesktopId[oldWsId] = effects->currentDesktop()->id();
        if (oldHudView >= 1 && oldHudView <= m_workspaces.size()) {
            m_workspaces[oldHudView - 1].lastVirtualDesktopId = effects->currentDesktop()->id();
            LeapKWorkspace::saveWorkspaces(m_workspaces);
        } else if (oldHudView == 0 && effects->config()) {
            KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
            primaryConfig.writeEntry("StandardLastVirtualDesktopId", effects->currentDesktop()->id());
            primaryConfig.sync();
        }
    }

    m_workspaceTilingActive[oldWsId] = m_tilingActive;
    if (m_tilingActive) {
        m_workspaceTilingPreset[oldWsId] = m_activeTilingPreset;
        if (oldHudView >= 1 && oldHudView <= m_workspaces.size()) {
            m_workspaces[oldHudView - 1].tilingPresetId = m_activeTilingPreset.id;
            LeapKWorkspace::saveWorkspaces(m_workspaces);
        } else if (oldHudView == 0 && effects->config()) {
            KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
            primaryConfig.writeEntry("StandardTilingPresetId", m_activeTilingPreset.id);
            primaryConfig.sync();
        }
        deactivateTiling(nullptr, true);
    } else {
        if (oldHudView >= 1 && oldHudView <= m_workspaces.size()) {
            m_workspaces[oldHudView - 1].tilingPresetId = QString();
            LeapKWorkspace::saveWorkspaces(m_workspaces);
        } else if (oldHudView == 0 && effects->config()) {
            KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
            primaryConfig.writeEntry("StandardTilingPresetId", QString());
            primaryConfig.sync();
        }
    }

    // 2. Set new active view
    m_activeHudView = newHudViewIndex;
    if (effects && effects->config()) {
        KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
        primaryConfig.writeEntry("ActiveHudView", m_activeHudView);
        primaryConfig.sync();
    }

    // 3. Restore remembered virtual desktop for incoming workspace
    VirtualDesktop *targetDesktop = nullptr;
    QString targetDeskId;

    if (m_workspaceLastVirtualDesktopId.contains(newWsId)) {
        targetDeskId = m_workspaceLastVirtualDesktopId.value(newWsId);
    } else if (newHudViewIndex >= 1 && newHudViewIndex <= m_workspaces.size()) {
        targetDeskId = m_workspaces.at(newHudViewIndex - 1).lastVirtualDesktopId;
    } else if (newHudViewIndex == 0 && effects && effects->config()) {
        KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
        targetDeskId = primaryConfig.readEntry("StandardLastVirtualDesktopId", QString());
    }

    if (!targetDeskId.isEmpty()) {
        targetDesktop = VirtualDesktopManager::self()->desktopForId(targetDeskId);
    }

    if (!targetDesktop && newHudViewIndex >= 1 && newHudViewIndex <= m_workspaces.size()) {
        for (EffectWindow *w : effects->stackingOrder()) {
            if (!w || w->isDeleted() || isBlockingPopup(w)) continue;
            if (findWorkspaceForWindow(w) == newHudViewIndex) {
                const auto desktops = w->desktops();
                if (!desktops.isEmpty() && !w->isOnAllDesktops()) {
                    targetDesktop = desktops.first();
                    break;
                }
            }
        }
    }

    // 4. Apply workspace window isolation FIRST (minimizes non-matching windows and unminimizes matching ones)
    applyWorkspaceViewIsolation(m_activeHudView, targetDesktop);

    // 5. Switch to target virtual desktop if needed
    if (targetDesktop && effects && effects->currentDesktop() != targetDesktop) {
        effects->setCurrentDesktop(targetDesktop);
    }
    if (effects && effects->currentDesktop()) {
        m_workspaceLastVirtualDesktopId[newWsId] = effects->currentDesktop()->id();
    }

    // 6. Ensure top matching window on current desktop is active
    if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
        EffectWindow *topWin = nullptr;
        for (EffectWindow *w : effects->stackingOrder()) {
            if (!w || w->isDeleted() || w->isMinimized() || !isEligibleOutlineWindow(w) || isBlockingPopup(w)) continue;
            if (w->isOnCurrentDesktop()) {
                if (findWorkspaceForWindow(w) == m_activeHudView) {
                    topWin = w;
                }
            }
        }
        if (topWin) {
            effects->activateWindow(topWin);
            m_activeWindow = topWin;
        }
    }

    // 5. Restore tiling state for incoming workspace
    bool shouldTile = m_workspaceTilingActive.value(newWsId, false);
    LeapKTiling::TilingPreset incomingPreset;

    if (m_workspaceTilingPreset.contains(newWsId)) {
        incomingPreset = m_workspaceTilingPreset.value(newWsId);
    } else {
        QString savedPresetId;
        if (newHudViewIndex >= 1 && newHudViewIndex <= m_workspaces.size()) {
            savedPresetId = m_workspaces.at(newHudViewIndex - 1).tilingPresetId;
        } else if (newHudViewIndex == 0 && effects && effects->config()) {
            KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
            savedPresetId = primaryConfig.readEntry("StandardTilingPresetId", QString());
        }
        if (!savedPresetId.isEmpty()) {
            for (const auto &p : m_tilingPresets) {
                if (p.id == savedPresetId) {
                    incomingPreset = p;
                    shouldTile = true;
                    m_workspaceTilingActive[newWsId] = true;
                    m_workspaceTilingPreset[newWsId] = p;
                    break;
                }
            }
        }
    }

    if (shouldTile && !incomingPreset.id.isEmpty() && !incomingPreset.isFloatAction()) {
        activateTilingPreset(incomingPreset, wasInJumpMode);
    } else if (m_tilingActive) {
        deactivateTiling(nullptr, true);
    }

    if (wasInJumpMode) {
        m_jumpMode = true;
        m_hudOriginWindow = effects ? effects->activeWindow() : m_activeWindow;
        m_hudSnapshotHistory = m_windowHistory;
        assignJumpLetters();
        if (effects) {
            effects->addRepaintFull();
        }
    }
}

void ActiveWindowOutlineEffect::slotOpenWorkspaceManager()
{
    EffectWindow *target = effects ? effects->activeWindow() : nullptr;
    if (!isEligibleOutlineWindow(target)) {
        target = m_activeWindow;
    }

    QStringList candidateDirs;
    candidateDirs << QDir::homePath() + QStringLiteral("/.local/bin");
#ifdef COLORPICKER_INSTALL_DIR
    candidateDirs << QStringLiteral(COLORPICKER_INSTALL_DIR);
#endif
    candidateDirs << QCoreApplication::applicationDirPath()
                  << QStringLiteral("/usr/libexec")
                  << QStringLiteral("/usr/lib/libexec")
                  << QStringLiteral("/usr/local/bin")
                  << QStringLiteral("/usr/bin");

    QString wsManagerExe = QStandardPaths::findExecutable(QStringLiteral("leapk-workspace-manager"), candidateDirs);
    if (wsManagerExe.isEmpty()) {
        return;
    }

    refreshWorkspaces();

    const auto desktops = target ? target->desktops() : QList<VirtualDesktop *>();
    const int desktopNum = (target && !desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
    const QString desktopName = (target && !desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->name() : QString();
    const QString appClass = target ? target->windowClass() : QString();
    const QString caption = target ? target->caption() : QString();

    QString assignedWsId;
    const int targetWsIdx = target ? findWorkspaceForWindow(target) : 0;
    if (targetWsIdx >= 1 && targetWsIdx <= m_workspaces.size()) {
        assignedWsId = m_workspaces.at(targetWsIdx - 1).id;
    } else {
        for (const auto &ws : m_workspaces) {
            if (LeapKWorkspace::matchesWorkspace(ws, appClass, caption, desktopNum)) {
                assignedWsId = ws.id;
                break;
            }
        }
    }

    QStringList args;
    if (!m_badgeConfig.activeThemeId.isEmpty()) {
        args << QStringLiteral("--theme") << m_badgeConfig.activeThemeId;
    }
    args << appClass;
    args << caption;
    args << QString::number(desktopNum);
    args << desktopName;
    args << assignedWsId;

    QProcess *process = new QProcess(this);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        const QByteArray out = process->readAllStandardOutput();
        process->deleteLater();
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            const QJsonDocument doc = QJsonDocument::fromJson(out);
            if (doc.isObject()) {
                const QJsonObject obj = doc.object();
                const QJsonArray delArr = obj.value(QStringLiteral("deletedWorkspaces")).toArray();
                for (const auto &val : delArr) {
                    if (val.isObject()) {
                        const QJsonObject delObj = val.toObject();
                        applyWorkspaceDeletion(delObj.value(QStringLiteral("id")).toString(),
                                               delObj.value(QStringLiteral("action")).toString(QStringLiteral("standard")));
                    }
                }
            }
            refreshWorkspaces();
            if (effects) {
                effects->addRepaintFull();
            }
            if (m_jumpMode) {
                m_jumpLetterToWindow = assignJumpLetters();
                m_badgeTextureCache.clear();
            }
        }
    });
    connect(process, &QProcess::errorOccurred, this, [process]() {
        process->deleteLater();
    });

    process->start(wsManagerExe, args);
}

void ActiveWindowOutlineEffect::slotOpenMarkerManager()
{
    EffectWindow *target = effects ? effects->activeWindow() : nullptr;
    if (!isEligibleOutlineWindow(target)) {
        target = m_activeWindow;
    }

    QStringList candidateDirs;
    candidateDirs << QDir::homePath() + QStringLiteral("/.local/bin");
#ifdef COLORPICKER_INSTALL_DIR
    candidateDirs << QStringLiteral(COLORPICKER_INSTALL_DIR);
#endif
    candidateDirs << QCoreApplication::applicationDirPath()
                  << QStringLiteral("/usr/libexec")
                  << QStringLiteral("/usr/lib/libexec")
                  << QStringLiteral("/usr/local/bin")
                  << QStringLiteral("/usr/bin");

    QString markerManagerExe = QStandardPaths::findExecutable(QStringLiteral("leapk-marker-manager"), candidateDirs);
    if (markerManagerExe.isEmpty()) {
        return;
    }

    const auto desktops = target ? target->desktops() : QList<VirtualDesktop *>();
    const int desktopNum = (target && !desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
    const QString desktopName = (target && !desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->name() : QString();
    const QString appClass = target ? target->windowClass() : QString();
    const QString caption = target ? target->caption() : QString();

    QStringList args;
    if (!m_badgeConfig.activeThemeId.isEmpty()) {
        args << QStringLiteral("--theme") << m_badgeConfig.activeThemeId;
    }
    if (target && !target->isDeleted()) {
        args << appClass
             << caption
             << QString::number(desktopNum)
             << desktopName;
    }

    // Collect all open windows to pass live matching window info
    QJsonArray openWindowsArr;
    if (effects) {
        for (EffectWindow *w : effects->stackingOrder()) {
            if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher()) {
                continue;
            }
            QJsonObject wObj;
            wObj[QStringLiteral("app")] = w->windowClass();
            wObj[QStringLiteral("caption")] = w->caption();
            const auto wDesktops = w->desktops();
            const int dNum = (!wDesktops.isEmpty() && !w->isOnAllDesktops()) ? wDesktops.first()->x11DesktopNumber() : 0;
            wObj[QStringLiteral("desktop")] = dNum;
            openWindowsArr.append(wObj);
        }
    }
    if (!openWindowsArr.isEmpty()) {
        args << QStringLiteral("--open-windows") << QString::fromUtf8(QJsonDocument(openWindowsArr).toJson(QJsonDocument::Compact));
    }

    QPointer<EffectWindow> targetWindow(target);
    QProcess *process = new QProcess(this);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process, targetWindow](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitCode);
        Q_UNUSED(exitStatus);
        Q_UNUSED(targetWindow);
        process->deleteLater();
        m_persistentMarks = LeapKMarks::loadPersistentMarks();
        if (effects) {
            m_activeWindow = effects->activeWindow();
        }
        refreshPersistentMarksForWindows();
        m_jumpLetterToWindow = assignJumpLetters();
        m_badgeTextureCache.clear();
        m_hudHeaderTexture.reset();
        m_hudHeaderCacheKey = -1;
        repaintAllOutlines();
        if (effects) {
            effects->addRepaintFull();
        }
    });
    connect(process, &QProcess::errorOccurred, this, [process]() {
        process->deleteLater();
    });

    process->start(markerManagerExe, args);
}

void ActiveWindowOutlineEffect::slotToggleHudGrouping()
{
    m_badgeConfig.hudGroupWindows = !m_badgeConfig.hudGroupWindows;
    KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
    primaryConfig.writeEntry("BadgeHudGroupWindows", m_badgeConfig.hudGroupWindows);
    primaryConfig.sync();

    if (m_jumpMode) {
        if (m_subLeapActive) {
            exitSubLeapMode();
        }
        m_jumpLetterToWindow = assignJumpLetters();
        m_badgeTextureCache.clear();
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotToggleHudHints()
{
    m_badgeConfig.hudShowHints = !m_badgeConfig.hudShowHints;
    m_badgeHudShowHints = m_badgeConfig.hudShowHints;

    if (effects && effects->config()) {
        KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
        primaryConfig.writeEntry("BadgeHudShowHints", m_badgeConfig.hudShowHints);
        primaryConfig.sync();
    }

    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::applyWorkspaceViewIsolation(int hudViewIndex, VirtualDesktop *targetDesktop)
{
    if (!effects) {
        return;
    }

    const auto windows = effects->stackingOrder();

    if (hudViewIndex >= 1 && hudViewIndex <= m_workspaces.size()) {
        EffectWindow *topMatchingWindow = nullptr;
        VirtualDesktop *effectiveDesk = targetDesktop ? targetDesktop : (effects ? effects->currentDesktop() : nullptr);

        for (EffectWindow *w : windows) {
            if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                continue;
            }

            // Stashed windows must always stay hidden in stash!
            if (m_stashedWindows.contains(w)) {
                continue;
            }

            // Pinned windows must stay visible in their pinned workspaces (or all workspaces)!
            if (isWindowPinned(w, hudViewIndex)) {
                if (w->isMinimized()) {
                    m_workspaceMinimizedWindows.remove(w);
                    w->setMinimized(false);
                }
                continue;
            }

            // Tiled windows currently occupying active slots must stay visible!
            if (m_tilingActive && isWindowInTileSlot(w)) {
                if (w->isMinimized()) {
                    m_workspaceMinimizedWindows.remove(w);
                    w->setMinimized(false);
                }
                continue;
            }

            const bool matches = (findWorkspaceForWindow(w) == hudViewIndex);

            if (matches) {
                // Should appear: unminimize if minimized
                if (w->isMinimized()) {
                    m_workspaceMinimizedWindows.remove(w);
                    w->setMinimized(false);
                }
                const auto desktops = w->desktops();
                if (effectiveDesk && (desktops.contains(effectiveDesk) || w->isOnAllDesktops())) {
                    topMatchingWindow = w;
                } else if (!topMatchingWindow) {
                    topMatchingWindow = w;
                }
            } else {
                // Should disappear: minimize if not minimized
                if (!w->isMinimized()) {
                    m_workspaceMinimizedWindows.insert(w);
                    w->setMinimized(true);
                }
            }
        }

        // If active window was minimized (or does not match workspace), focus top matching window
        if (topMatchingWindow && (!m_activeWindow || m_activeWindow->isMinimized() || !m_activeWindow->isOnCurrentDesktop())) {
            effects->activateWindow(topMatchingWindow);
            m_activeWindow = topMatchingWindow;
        }
    } else if (hudViewIndex == 0 || hudViewIndex == m_workspaces.size() + 1) {
        // Standard view (0) or All windows: restore windows minimized by workspace isolation
        for (EffectWindow *w : m_workspaceMinimizedWindows) {
            if (w && !w->isDeleted() && w->isMinimized() && !m_stashedWindows.contains(w)) {
                w->setMinimized(false);
            }
        }
        m_workspaceMinimizedWindows.clear();
    }
}

void ActiveWindowOutlineEffect::enterStashMode()
{
    m_stashModeActive = true;
    m_superJumpActive = false;
    m_subLeapActive = false;
    m_tilingPresetsModeActive = false;
    m_hudTilingPresetPillRects.clear();
    m_subLeapBaseKey = QChar();
    m_subLeapWindows.clear();
    m_subLeapLetterToWindow.clear();
    m_subLeapTargets.clear();

    applyWorkspaceViewIsolation(m_activeHudView);

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

void ActiveWindowOutlineEffect::exitStashMode()
{
    m_stashModeActive = false;
    applyWorkspaceViewIsolation(m_activeHudView);
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

int ActiveWindowOutlineEffect::findWorkspaceForWindow(EffectWindow *target) const
{
    if (!target || target->isDeleted()) {
        return 0; // Standard
    }

    if (m_tilingActive && isWindowInTileSlot(target) && m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
        return m_activeHudView;
    }

    if (m_windowWorkspace.contains(target)) {
        return m_windowWorkspace.value(target);
    }

    const auto desktops = target->desktops();
    const int dNum = (!desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
    const QString appClass = target->windowClass();
    const QString caption = target->caption();

    const int found = LeapKWorkspace::findWorkspaceIndexForWindow(m_workspaces, appClass, caption, dNum);
    const_cast<ActiveWindowOutlineEffect *>(this)->m_windowWorkspace[target] = found;
    return found;
}

bool ActiveWindowOutlineEffect::isWorkspacePlayingAudio(int wsIndex) const
{
    if (!effects) {
        return false;
    }
    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
            continue;
        }
        if (isWindowInStash(w)) {
            continue;
        }
        if (wsIndex >= 1 && wsIndex <= m_workspaces.size()) {
            if (findWorkspaceForWindow(w) == wsIndex && m_audioTracker.isWindowPlaying(w->pid(), w->caption())) {
                return true;
            }
        } else if (wsIndex == 0) {
            if (w->isOnCurrentDesktop() && findWorkspaceForWindow(w) == 0 && m_audioTracker.isWindowPlaying(w->pid(), w->caption())) {
                return true;
            }
        }
    }
    return false;
}

void ActiveWindowOutlineEffect::addWindowToWorkspace(EffectWindow *w, int workspaceIndex)
{
    if (!w || w->isDeleted()) {
        return;
    }

    m_stashedWindows.remove(w);
    m_workspaceMinimizedWindows.remove(w);
    m_windowWorkspace[w] = workspaceIndex;

    const QString appClass = w->windowClass().trimmed();
    const QString caption = w->caption().trimmed();

    if (workspaceIndex >= 1 && workspaceIndex <= m_workspaces.size()) {
        const QString wsId = m_workspaces.at(workspaceIndex - 1).id;

        // Check if there are other windows of this same appClass active in DIFFERENT workspaces
        bool otherWindowInOtherWs = false;
        if (effects && !appClass.isEmpty()) {
            for (EffectWindow *other : effects->stackingOrder()) {
                if (other && other != w && !other->isDeleted() && other->windowClass() == appClass) {
                    const int otherWs = m_windowWorkspace.value(other, 0);
                    if (otherWs > 0 && otherWs != workspaceIndex) {
                        otherWindowInOtherWs = true;
                        break;
                    }
                }
            }
        }

        if (otherWindowInOtherWs) {
            // Do NOT remove app from all workspaces because another instance lives in another workspace!
            // Instead, add/ensure a rule for this workspace with desktop match if available
            const auto desktops = w->desktops();
            const int dNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
            auto &ws = m_workspaces[workspaceIndex - 1];
            bool alreadyHasRule = false;
            for (const auto &r : ws.rules) {
                if (r.matchApp.compare(appClass, Qt::CaseInsensitive) == 0 && (r.matchDesktop == dNum || r.matchDesktop == 0)) {
                    alreadyHasRule = true;
                    break;
                }
            }
            if (!alreadyHasRule) {
                LeapKWorkspace::WindowMatchRule r;
                r.matchApp = appClass;
                r.matchDesktop = dNum;
                ws.rules.append(r);
            }
        } else {
            LeapKWorkspace::reassignWindowToWorkspace(m_workspaces, wsId, appClass, caption, !appClass.isEmpty(), 0);
        }

        if (effects && effects->currentDesktop()) {
            if (workspaceIndex - 1 < m_workspaces.size()) {
                m_workspaces[workspaceIndex - 1].lastVirtualDesktopId = effects->currentDesktop()->id();
            }
            m_workspaceLastVirtualDesktopId[wsId] = effects->currentDesktop()->id();
            effects->windowToDesktops(w, {effects->currentDesktop()});
        }
        LeapKWorkspace::saveWorkspaces(m_workspaces);
    } else if (workspaceIndex == 0) {
        bool otherWindowInOtherWs = false;
        if (effects && !appClass.isEmpty()) {
            for (EffectWindow *other : effects->stackingOrder()) {
                if (other && other != w && !other->isDeleted() && other->windowClass() == appClass) {
                    const int otherWs = m_windowWorkspace.value(other, 0);
                    if (otherWs > 0) {
                        otherWindowInOtherWs = true;
                        break;
                    }
                }
            }
        }
        if (!otherWindowInOtherWs && !appClass.isEmpty()) {
            LeapKWorkspace::removeWindowFromAllWorkspaces(m_workspaces, appClass);
            LeapKWorkspace::saveWorkspaces(m_workspaces);
        }
        if (effects && effects->currentDesktop()) {
            effects->windowToDesktops(w, {effects->currentDesktop()});
        }
    }

    if (w->isMinimized()) {
        w->setMinimized(false);
    }
    if (effects) {
        effects->activateWindow(w);
    }
    m_activeWindow = w;
}

void ActiveWindowOutlineEffect::createEmptyWorkspace(const QString &suggestedName)
{
    if (m_stashModeActive) {
        exitStashMode();
    }
    refreshWorkspaces();

    const int nextNum = m_workspaces.size() + 1;
    const QString name = !suggestedName.trimmed().isEmpty()
        ? suggestedName.trimmed()
        : QStringLiteral("Workspace %1").arg(nextNum);

    LeapKWorkspace::createEmptyWorkspace(m_workspaces, name);

    if (m_subLeapActive) {
        exitSubLeapMode();
    }

    // Switch active HUD view directly to the new empty workspace
    // Views: 0 = Standard, 1..N = Workspaces, N+1 = All
    switchToWorkspaceView(m_workspaces.size());

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

void ActiveWindowOutlineEffect::applyWorkspaceDeletion(const QString &deletedWorkspaceId, const QString &windowAction)
{
    if (deletedWorkspaceId.isEmpty()) {
        return;
    }

    refreshWorkspaces();

    // Find the workspace rules before deleting
    LeapKWorkspace::WorkspaceInfo targetWs;
    int foundIdx = -1;
    for (int i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces.at(i).id == deletedWorkspaceId) {
            targetWs = m_workspaces.at(i);
            foundIdx = i;
            break;
        }
    }

    const auto windows = effects ? effects->stackingOrder() : QList<EffectWindow *>();
    QList<EffectWindow *> matchingWindows;
    if (foundIdx >= 0) {
        for (EffectWindow *w : windows) {
            if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                continue;
            }
            if (findWorkspaceForWindow(w) == (foundIdx + 1)) {
                matchingWindows.append(w);
            }
        }
    }

    // Apply the chosen windowAction
    if (windowAction == QLatin1String("close")) {
        for (EffectWindow *w : matchingWindows) {
            if (w && !w->isDeleted()) {
                m_workspaceMinimizedWindows.remove(w);
                m_stashedWindows.remove(w);
                w->closeWindow();
            }
        }
    } else if (windowAction == QLatin1String("stash")) {
        for (EffectWindow *w : matchingWindows) {
            if (w && !w->isDeleted()) {
                m_workspaceMinimizedWindows.remove(w);
                m_stashedWindows.insert(w);
                w->setMinimized(true);
            }
        }
    } else { // "standard"
        for (EffectWindow *w : matchingWindows) {
            if (w && !w->isDeleted()) {
                m_workspaceMinimizedWindows.remove(w);
                m_stashedWindows.remove(w);
                if (w->isMinimized()) {
                    w->setMinimized(false);
                }
            }
        }
    }

    // Remove from workspaces list if still there
    if (foundIdx >= 0 && foundIdx < m_workspaces.size()) {
        m_workspaces.removeAt(foundIdx);
        LeapKWorkspace::saveWorkspaces(m_workspaces);
    }
    if (foundIdx >= 0) {
        for (auto it = m_windowWorkspace.begin(); it != m_windowWorkspace.end(); ) {
            if (it.value() == (foundIdx + 1)) {
                it = m_windowWorkspace.erase(it);
            } else if (it.value() > (foundIdx + 1)) {
                *it = it.value() - 1;
                ++it;
            } else {
                ++it;
            }
        }
    }
    m_workspaceLastVirtualDesktopId.remove(deletedWorkspaceId);
    m_workspaceTilingActive.remove(deletedWorkspaceId);
    m_workspaceTilingPreset.remove(deletedWorkspaceId);

    // Reset active view to Standard (0)
    if (m_activeHudView != 0) {
        switchToWorkspaceView(0);
    } else {
        applyWorkspaceViewIsolation(0);
    }

    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;

    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotCreateNewWorkspacePrompt()
{
    if (m_jumpMode) {
        cancelJumpMode();
    }

    QStringList candidateDirs;
    candidateDirs << QDir::homePath() + QStringLiteral("/.local/bin");
#ifdef COLORPICKER_INSTALL_DIR
    candidateDirs << QStringLiteral(COLORPICKER_INSTALL_DIR);
#endif
    candidateDirs << QCoreApplication::applicationDirPath()
                  << QStringLiteral("/usr/libexec")
                  << QStringLiteral("/usr/lib/libexec")
                  << QStringLiteral("/usr/local/bin")
                  << QStringLiteral("/usr/bin");

    QString wsManagerExe = QStandardPaths::findExecutable(QStringLiteral("leapk-workspace-manager"), candidateDirs);
    if (wsManagerExe.isEmpty()) {
        return;
    }

    QStringList args;
    if (!m_badgeConfig.activeThemeId.isEmpty()) {
        args << QStringLiteral("--theme") << m_badgeConfig.activeThemeId;
    }
    args << QStringLiteral("--prompt-new-workspace");

    QProcess *process = new QProcess(this);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus);
        const QByteArray out = process->readAllStandardOutput();
        process->deleteLater();
        if (exitCode == 0) {
            refreshWorkspaces();
            switchToWorkspaceView(m_workspaces.size());
            slotTriggerJumpMode();
        } else {
            slotTriggerJumpMode();
        }
    });
    connect(process, &QProcess::errorOccurred, this, [process]() {
        process->deleteLater();
    });

    process->start(wsManagerExe, args);
}

void ActiveWindowOutlineEffect::slotCloseActiveWorkspace()
{
    if (m_activeHudView < 1 || m_activeHudView > m_workspaces.size()) {
        return;
    }

    refreshWorkspaces();
    if (m_activeHudView < 1 || m_activeHudView > m_workspaces.size()) {
        return;
    }

    const auto ws = m_workspaces.at(m_activeHudView - 1);

    // Check if there are any open windows assigned to this workspace
    int matchingWindowsCount = 0;
    if (effects) {
        for (EffectWindow *win : effects->stackingOrder()) {
            if (!win || win->isDeleted() || !isEligibleOutlineWindow(win)) continue;
            if (findWorkspaceForWindow(win) == m_activeHudView) {
                matchingWindowsCount++;
            }
        }
    }

    // If no windows assigned, delete directly without prompting
    if (matchingWindowsCount == 0) {
        applyWorkspaceDeletion(ws.id, QStringLiteral("standard"));
        if (m_jumpMode) {
            m_jumpLetterToWindow = assignJumpLetters();
            m_badgeTextureCache.clear();
            m_hudHeaderTexture.reset();
            m_hudHeaderCacheKey = -1;
            m_hudHintBarTexture.reset();
            m_hudHintBarCacheKey = -1;
            effects->addRepaintFull();
        }
        return;
    }

    if (m_jumpMode) {
        cancelJumpMode();
    }

    QStringList candidateDirs;
    candidateDirs << QDir::homePath() + QStringLiteral("/.local/bin");
#ifdef COLORPICKER_INSTALL_DIR
    candidateDirs << QStringLiteral(COLORPICKER_INSTALL_DIR);
#endif
    candidateDirs << QCoreApplication::applicationDirPath()
                  << QStringLiteral("/usr/libexec")
                  << QStringLiteral("/usr/lib/libexec")
                  << QStringLiteral("/usr/local/bin")
                  << QStringLiteral("/usr/bin");

    QString wsManagerExe = QStandardPaths::findExecutable(QStringLiteral("leapk-workspace-manager"), candidateDirs);
    if (wsManagerExe.isEmpty()) {
        return;
    }

    QStringList args;
    if (!m_badgeConfig.activeThemeId.isEmpty()) {
        args << QStringLiteral("--theme") << m_badgeConfig.activeThemeId;
    }
    args << QStringLiteral("--close-workspace") << ws.id;

    QProcess *process = new QProcess(this);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus);
        const QByteArray out = process->readAllStandardOutput();
        process->deleteLater();
        if (exitCode == 0) {
            const QJsonDocument doc = QJsonDocument::fromJson(out);
            if (doc.isObject()) {
                const QJsonObject obj = doc.object();
                const QString delId = obj.value(QStringLiteral("deletedWorkspaceId")).toString();
                const QString winAction = obj.value(QStringLiteral("windowAction")).toString(QStringLiteral("standard"));
                applyWorkspaceDeletion(delId, winAction);
            }
            slotTriggerJumpMode();
        } else {
            slotTriggerJumpMode();
        }
    });
    connect(process, &QProcess::errorOccurred, this, [process]() {
        process->deleteLater();
    });

    process->start(wsManagerExe, args);
}

EffectWindow *ActiveWindowOutlineEffect::findRecentWindow() const
{
    EffectWindow *recent = OutlineHistory::findRecentWindow(
        m_windowHistory,
        m_activeWindow,
        [this](EffectWindow *win) {
            return isEligibleOutlineWindow(win);
        });
    if (recent) {
        return recent;
    }
    const auto windows = effects->stackingOrder();
    for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
        EffectWindow *candidate = *it;
        if (candidate && !candidate->isDeleted() && candidate != m_activeWindow && isEligibleOutlineWindow(candidate)) {
            return candidate;
        }
    }
    return nullptr;
}

void ActiveWindowOutlineEffect::slotJumpToRecentWindow()
{
    slotJumpToRecentWindow(0);
}

void ActiveWindowOutlineEffect::slotJumpToRecentWindow(int keyToRelease)
{
    EffectWindow *target = findRecentWindow();
    if (!target || target->isDeleted()) {
        if (m_jumpMode) {
            cancelJumpMode(keyToRelease != 0 ? keyToRelease : Qt::Key_Space);
        }
        return;
    }

    if (m_jumpMode) {
        cancelJumpMode(keyToRelease != 0 ? keyToRelease : Qt::Key_Space);
    }

    m_isHistoryNavigating = false;
    if (m_historyCommitTimer && m_historyCommitTimer->isActive()) {
        m_historyCommitTimer->stop();
    }

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
    m_activeWindow = target;
    warpCursorToWindow(target);
}

void ActiveWindowOutlineEffect::warpCursorToWindow(EffectWindow *target)
{
    if (m_soundFeedbackEnabled && target) {
        playThemedSound(OutlineSound::SoundEffect::LandWindow);
    }
    if (m_cursorWarpMode == 0 || !target) {
        return;
    }
    const QRectF frameGeo = target->frameGeometry();
    const QRectF clientGeo = target->clientGeometry();
    const bool hasDec = target->hasDecoration();
    const QPointF warpPos = OutlineBadge::computeCursorWarpPos(m_cursorWarpMode, frameGeo, clientGeo, hasDec);
    if (warpPos.isNull()) {
        return;
    }

#ifndef ACTIVE_WINDOW_OUTLINE_X11
    if (KWin::Cursors::self() && KWin::Cursors::self()->mouse()) {
        KWin::Cursors::self()->mouse()->setPos(warpPos);
    }
#endif
    QCursor::setPos(warpPos.toPoint());
}

void ActiveWindowOutlineEffect::updateRecentWindowsFromHistory()
{
    const QList<EffectWindow *> oldRecents = m_recentWindows;
    m_recentWindows = OutlineHistory::computeRecentWindows(
        m_windowHistory,
        m_activeWindow,
        m_recentWindowsCount,
        m_highlightRecentWindow,
        [this](EffectWindow *win) {
            return isEligibleOutlineWindow(win);
        });

    for (EffectWindow *oldWin : oldRecents) {
        if (oldWin && !m_recentWindows.contains(oldWin) && oldWin != m_activeWindow && !m_markedWindows.contains(oldWin)) {
            effects->addRepaint(expandedGeometryFor(oldWin));
            m_outlineCache.remove(oldWin);
            m_lastGeometry.remove(oldWin);
            m_recentAmbilightColors.remove(oldWin);
            m_recentCycleColors.remove(oldWin);
        }
    }
}

void ActiveWindowOutlineEffect::slotPrevRecentWindow()
{
    for (auto it = m_windowHistory.begin(); it != m_windowHistory.end(); ) {
        if (!(*it) || (*it)->isDeleted()) {
            it = m_windowHistory.erase(it);
        } else {
            ++it;
        }
    }

    if (!m_isHistoryNavigating) {
        EffectWindow *active = effects ? effects->activeWindow() : m_activeWindow;
        if (active && isEligibleOutlineWindow(active)) {
            m_activeWindow = active;
            m_windowHistory.removeAll(m_activeWindow);
            m_windowHistory.prepend(m_activeWindow);
            m_historyNavIndex = 0;
        }
    }

    if (m_windowHistory.size() < 2 && effects) {
        const auto windows = effects->stackingOrder();
        for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
            EffectWindow *w = *it;
            if (w && !w->isDeleted() && isEligibleOutlineWindow(w) && !m_windowHistory.contains(w)) {
                m_windowHistory.append(w);
            }
        }
    }

    if (m_windowHistory.size() < 2) {
        return;
    }

    int nextIdx = m_historyNavIndex + 1;
    while (nextIdx < m_windowHistory.size()) {
        EffectWindow *candidate = m_windowHistory.at(nextIdx);
        if (candidate && isEligibleOutlineWindow(candidate)) {
            m_historyNavIndex = nextIdx;
            m_isHistoryNavigating = true;
            m_expectedJumpTarget = candidate;
            if (m_expectedJumpTargetTimer) {
                m_expectedJumpTargetTimer->start(1000);
            }

            const int targetWs = findWorkspaceForWindow(candidate);
            if (m_activeHudView != targetWs && m_activeHudView <= m_workspaces.size()) {
                switchToWorkspaceView(targetWs);
            }

            if (!candidate->isOnCurrentDesktop()) {
                const auto desktops = candidate->desktops();
                if (!desktops.isEmpty() && !candidate->isOnAllDesktops()) {
                    effects->setCurrentDesktop(desktops.first());
                }
            }
            m_workspaceMinimizedWindows.remove(candidate);
            m_stashedWindows.remove(candidate);
            if (candidate->isMinimized()) {
                candidate->setMinimized(false);
            }
            effects->activateWindow(candidate);
            m_activeWindow = candidate;
            break;
        } else {
            nextIdx++;
        }
    }

    if (m_historyCommitTimer) {
        m_historyCommitTimer->start(600);
    }
}

void ActiveWindowOutlineEffect::slotNextRecentWindow()
{
    for (auto it = m_windowHistory.begin(); it != m_windowHistory.end(); ) {
        if (!(*it) || (*it)->isDeleted()) {
            it = m_windowHistory.erase(it);
        } else {
            ++it;
        }
    }

    if (m_windowHistory.size() < 2 || m_historyNavIndex <= 0) {
        return;
    }

    int prevIdx = m_historyNavIndex - 1;
    while (prevIdx >= 0) {
        EffectWindow *candidate = m_windowHistory.at(prevIdx);
        if (candidate && isEligibleOutlineWindow(candidate)) {
            m_historyNavIndex = prevIdx;
            m_isHistoryNavigating = true;
            m_expectedJumpTarget = candidate;
            if (m_expectedJumpTargetTimer) {
                m_expectedJumpTargetTimer->start(1000);
            }

            const int targetWs = findWorkspaceForWindow(candidate);
            if (m_activeHudView != targetWs && m_activeHudView <= m_workspaces.size()) {
                switchToWorkspaceView(targetWs);
            }

            if (!candidate->isOnCurrentDesktop()) {
                const auto desktops = candidate->desktops();
                if (!desktops.isEmpty() && !candidate->isOnAllDesktops()) {
                    effects->setCurrentDesktop(desktops.first());
                }
            }
            m_workspaceMinimizedWindows.remove(candidate);
            m_stashedWindows.remove(candidate);
            if (candidate->isMinimized()) {
                candidate->setMinimized(false);
            }
            effects->activateWindow(candidate);
            m_activeWindow = candidate;
            break;
        } else {
            prevIdx--;
        }
    }

    if (m_historyCommitTimer) {
        m_historyCommitTimer->start(600);
    }
}

void ActiveWindowOutlineEffect::slotCommitHistoryNavigation()
{
    if (m_historyCommitTimer && m_historyCommitTimer->isActive()) {
        m_historyCommitTimer->stop();
    }
    m_isHistoryNavigating = false;
    if (m_historyNavIndex >= 0 && m_historyNavIndex < m_windowHistory.size()) {
        EffectWindow *committedWindow = m_windowHistory.at(m_historyNavIndex);
        if (committedWindow && !committedWindow->isDeleted()) {
            m_windowHistory.removeAll(committedWindow);
            m_windowHistory.prepend(committedWindow);
            m_activeWindow = committedWindow;

            const int targetWs = findWorkspaceForWindow(committedWindow);
            if (m_activeHudView != targetWs && m_activeHudView <= m_workspaces.size()) {
                switchToWorkspaceView(targetWs);
            }

            if (!committedWindow->isOnCurrentDesktop()) {
                const auto desktops = committedWindow->desktops();
                if (!desktops.isEmpty() && !committedWindow->isOnAllDesktops()) {
                    effects->setCurrentDesktop(desktops.first());
                }
            }
            m_workspaceMinimizedWindows.remove(committedWindow);
            m_stashedWindows.remove(committedWindow);
            if (committedWindow->isMinimized()) {
                committedWindow->setMinimized(false);
            }
            if (m_tilingActive) {
                performTilingSwap(committedWindow);
            } else if (effects) {
                effects->activateWindow(committedWindow);
            }
            m_activeWindow = committedWindow;
        }
    }
    m_historyNavIndex = 0;
    updateRecentWindowsFromHistory();
    repaintAllOutlines();
    if (!m_jumpMode && m_keyboardGrabActive && effects) {
        effects->ungrabKeyboard();
        m_keyboardGrabActive = false;
    }
}

void ActiveWindowOutlineEffect::traverseRecentWindows(int direction)
{
    for (auto it = m_windowHistory.begin(); it != m_windowHistory.end(); ) {
        if (!(*it) || (*it)->isDeleted()) {
            it = m_windowHistory.erase(it);
        } else {
            ++it;
        }
    }

    if (m_windowHistory.size() < 2 && effects) {
        const auto windows = effects->stackingOrder();
        for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
            EffectWindow *w = *it;
            if (w && !w->isDeleted() && isEligibleOutlineWindow(w) && !m_windowHistory.contains(w)) {
                m_windowHistory.append(w);
            }
        }
    }

    if (!m_metaSpaceTraverseActive) {
        m_metaSpaceTraverseActive = true;
        m_isHistoryNavigating = true;
        if (m_historyCommitTimer && m_historyCommitTimer->isActive()) {
            m_historyCommitTimer->stop();
        }
        EffectWindow *active = effects ? effects->activeWindow() : m_activeWindow;
        if (active && isEligibleOutlineWindow(active)) {
            m_activeWindow = active;
            m_windowHistory.removeAll(m_activeWindow);
            m_windowHistory.prepend(m_activeWindow);
        }
        m_historyNavIndex = 0;
    }

    QList<EffectWindow *> validWindows;
    for (EffectWindow *w : m_windowHistory) {
        if (w && !w->isDeleted() && isEligibleOutlineWindow(w)) {
            if (!validWindows.contains(w)) {
                validWindows.append(w);
            }
        }
    }

    if (validWindows.size() < 2) {
        return;
    }

    int currentValidIdx = 0;
    if (m_historyNavIndex >= 0 && m_historyNavIndex < m_windowHistory.size()) {
        EffectWindow *currentNavWin = m_windowHistory.at(m_historyNavIndex);
        currentValidIdx = validWindows.indexOf(currentNavWin);
        if (currentValidIdx < 0) {
            currentValidIdx = 0;
        }
    }

    const int nextValidIdx = OutlineHistory::stepHistoryIndex(currentValidIdx, direction, validWindows.size());
    EffectWindow *candidate = validWindows.at(nextValidIdx);
    m_historyNavIndex = m_windowHistory.indexOf(candidate);
    m_isHistoryNavigating = true;
    m_expectedJumpTarget = candidate;
    if (m_expectedJumpTargetTimer) {
        m_expectedJumpTargetTimer->start(1000);
    }

    const int targetWs = findWorkspaceForWindow(candidate);
    if (m_activeHudView != targetWs && m_activeHudView <= m_workspaces.size()) {
        switchToWorkspaceView(targetWs);
    }

    if (!candidate->isOnCurrentDesktop()) {
        const auto desktops = candidate->desktops();
        if (!desktops.isEmpty() && !candidate->isOnAllDesktops()) {
            effects->setCurrentDesktop(desktops.first());
        }
    }
    m_workspaceMinimizedWindows.remove(candidate);
    m_stashedWindows.remove(candidate);
    if (candidate->isMinimized()) {
        candidate->setMinimized(false);
    }
    if (candidate != m_activeWindow) {
        m_hudPrevActiveWindow = m_activeWindow;
        m_hudPrevMarkerRect = m_hudLastDrawnMarkerRect.isValid() ? m_hudLastDrawnMarkerRect : m_hudCurrentMarkerRect;
        m_hudPrevActiveDockIndex = m_hudCurrentActiveDockIndex;
        m_hudSelectAnimTimer.restart();
        m_hudSelectAnimActive = true;
        updateAnimationTimer();
    }
    effects->activateWindow(candidate);
    m_activeWindow = candidate;
    playThemedSound(OutlineSound::SoundEffect::NavTab);
    effects->addRepaintFull();
}

bool ActiveWindowOutlineEffect::isWindowInStash(EffectWindow *w) const
{
    if (!w || w->isDeleted()) {
        return false;
    }
    if (m_workspaceMinimizedWindows.contains(w)) {
        return false; // Minimized purely due to workspace isolation, not in stash!
    }
    return m_stashedWindows.contains(w) || w->isMinimized();
}

bool ActiveWindowOutlineEffect::isWindowPinned(EffectWindow *w, int wsIndex) const
{
    if (!w || w->isDeleted() || !m_pinnedWindows.contains(w)) {
        return false;
    }
    if (wsIndex <= 0) {
        return true;
    }
    const auto &wsSet = m_windowPinnedWorkspaces.value(w);
    if (wsSet.isEmpty() || wsSet.contains(0) || wsSet.contains(wsIndex)) {
        return true;
    }
    return false;
}

QSet<int> ActiveWindowOutlineEffect::getWindowPinnedWorkspaces(EffectWindow *w) const
{
    if (!w || !m_pinnedWindows.contains(w)) {
        return {};
    }
    return m_windowPinnedWorkspaces.value(w, {0});
}

void ActiveWindowOutlineEffect::pinWindow(EffectWindow *w)
{
    if (!w || w->isDeleted()) {
        return;
    }
    m_pinnedWindows.insert(w);
    m_windowPinnedWorkspaces[w] = {0}; // 0 = all workspaces
    m_workspaceMinimizedWindows.remove(w);
    m_stashedWindows.remove(w);
    if (w->isMinimized()) {
        w->setMinimized(false);
    }
    // Strictly preserve KDE virtual desktop assignments - do NOT call effects->windowToDesktops(w, {})!
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    repaintAllOutlines();
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::pinWindowToWorkspace(EffectWindow *w, int wsIndex)
{
    if (!w || w->isDeleted()) {
        return;
    }
    m_pinnedWindows.insert(w);
    if (wsIndex <= 0) {
        m_windowPinnedWorkspaces[w] = {0};
    } else {
        auto &set = m_windowPinnedWorkspaces[w];
        set.remove(0);
        set.insert(wsIndex);
    }
    m_workspaceMinimizedWindows.remove(w);
    m_stashedWindows.remove(w);
    if (w->isMinimized()) {
        w->setMinimized(false);
    }
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    repaintAllOutlines();
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::pinWindowToWorkspaces(EffectWindow *w, const QSet<int> &wsIndices)
{
    if (!w || w->isDeleted()) {
        return;
    }
    m_pinnedWindows.insert(w);
    auto &set = m_windowPinnedWorkspaces[w];
    set.clear();
    for (int idx : wsIndices) {
        if (idx > 0) {
            set.insert(idx);
        }
    }
    if (set.isEmpty()) {
        set.insert(0); // 0 = all workspaces fallback
    }
    m_workspaceMinimizedWindows.remove(w);
    m_stashedWindows.remove(w);
    if (w->isMinimized()) {
        w->setMinimized(false);
    }
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    repaintAllOutlines();
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::toggleWindowPinWorkspace(EffectWindow *w, int wsIndex)
{
    if (!w || w->isDeleted()) {
        return;
    }
    if (wsIndex <= 0) {
        if (m_pinnedWindows.contains(w) && m_windowPinnedWorkspaces.value(w).contains(0)) {
            unpinWindow(w, m_activeHudView);
        } else {
            pinWindow(w);
        }
        return;
    }
    if (!m_pinnedWindows.contains(w)) {
        pinWindowToWorkspace(w, wsIndex);
        return;
    }
    auto &set = m_windowPinnedWorkspaces[w];
    if (set.contains(0)) {
        set.clear();
        set.insert(wsIndex);
    } else if (set.contains(wsIndex)) {
        set.remove(wsIndex);
        if (set.isEmpty()) {
            unpinWindow(w, m_activeHudView);
            return;
        }
    } else {
        set.insert(wsIndex);
    }
    m_workspaceMinimizedWindows.remove(w);
    m_stashedWindows.remove(w);
    if (w->isMinimized()) {
        w->setMinimized(false);
    }
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    repaintAllOutlines();
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::unpinWindow(EffectWindow *w, int targetWorkspace)
{
    if (!w || w->isDeleted()) {
        return;
    }
    m_pinnedWindows.remove(w);
    m_windowPinnedWorkspaces.remove(w);
    if (targetWorkspace >= 1 && targetWorkspace <= m_workspaces.size()) {
        addWindowToWorkspace(w, targetWorkspace);
    }
    // Strictly preserve KDE virtual desktop assignments - do NOT call effects->windowToDesktops
    m_workspaceMinimizedWindows.remove(w);
    m_stashedWindows.remove(w);
    if (w->isMinimized()) {
        w->setMinimized(false);
    }
    if (targetWorkspace >= 1 && targetWorkspace <= m_workspaces.size()) {
        applyWorkspaceViewIsolation(targetWorkspace);
    }
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    repaintAllOutlines();
    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::toggleWindowPin(EffectWindow *w)
{
    if (!w || w->isDeleted()) {
        return;
    }
    if (m_pinnedWindows.contains(w)) {
        unpinWindow(w, m_activeHudView);
    } else {
        pinWindow(w);
    }
}

void ActiveWindowOutlineEffect::slotTogglePinActiveWindow()
{
    EffectWindow *w = effects ? effects->activeWindow() : nullptr;
    if (!isEligibleOutlineWindow(w)) {
        w = m_activeWindow;
    }
    if (w) {
        toggleWindowPin(w);
    }
}


} // namespace KWin
