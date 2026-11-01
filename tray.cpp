/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QPainter>
#include <QPainterPath>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QLocale>
#include <QDateTime>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTextBrowser>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusInterface>
#include <QDBusReply>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVersionNumber>
#include <QKeySequence>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KGlobalAccel>
#include "presethelper.h"
#include "presetactivationdialog.h"
#include "customshaderhelper.h"
#include "settingsdialog.h"
#include "updatehelper.h"
#include <iostream>

#ifndef ACTIVE_WINDOW_OUTLINE_VERSION
#define ACTIVE_WINDOW_OUTLINE_VERSION "1.6.0"
#endif

namespace
{

inline KConfigGroup getKWinGroup(const KSharedConfigPtr &cfg = KSharedConfig::openConfig(QStringLiteral("kwinrc")))
{
    if (cfg->hasGroup(QStringLiteral("Effect-leapk"))) {
        return cfg->group(QStringLiteral("Effect-leapk"));
    }
    if (cfg->hasGroup(QStringLiteral("Effect-active-window-outline"))) {
        return cfg->group(QStringLiteral("Effect-active-window-outline"));
    }
    return cfg->group(QStringLiteral("Effect-leapk"));
}

QIcon createTrayIconPixmap(bool enabled, const QColor &outlineColor = QColor(61, 174, 233))
{
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF outer(8, 8, 48, 48);
    QPainterPath path;
    path.addRoundedRect(outer, 9, 9);

    if (enabled) {
        // 1. Prominent multi-layer glowing halo around window card
        for (int r = 5; r >= 1; --r) {
            QColor glow = outlineColor;
            glow.setAlpha(18 + (6 - r) * 12);
            p.setPen(QPen(glow, 2.0 + r * 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }

        // 2. Window card background fill with subtle gradient
        QLinearGradient cardGrad(outer.topLeft(), outer.bottomLeft());
        cardGrad.setColorAt(0.0, QColor(32, 38, 48, 245));
        cardGrad.setColorAt(1.0, QColor(20, 24, 30, 252));
        p.fillPath(path, cardGrad);

        // 3. Titlebar separator
        p.setPen(QPen(QColor(56, 64, 76), 1.2));
        p.drawLine(QPointF(outer.left(), 22), QPointF(outer.right(), 22));

        // 4. KDE titlebar buttons on the RIGHT side (Minimize, Maximize, Close)
        p.setPen(Qt::NoPen);
        // Minimize (blue)
        p.setBrush(QColor(52, 152, 219));
        p.drawEllipse(QRectF(34, 13, 4.5, 4.5));
        // Maximize (green)
        p.setBrush(QColor(46, 204, 113));
        p.drawEllipse(QRectF(40.5, 13, 4.5, 4.5));
        // Close (red)
        p.setBrush(QColor(235, 75, 75));
        p.drawEllipse(QRectF(47, 13, 4.5, 4.5));

        // 5. Left titlebar dash
        p.setBrush(QColor(115, 125, 140));
        p.drawRoundedRect(QRectF(13, 14, 12, 2.5), 1.25, 1.25);

        // 6. Window content preview lines
        p.setBrush(QColor(48, 55, 68));
        p.drawRoundedRect(QRectF(13, 27, 16, 2.5), 1.25, 1.25);
        p.drawRoundedRect(QRectF(13, 33, 12, 2.5), 1.25, 1.25);
        p.drawRoundedRect(QRectF(13, 39, 14, 2.5), 1.25, 1.25);

        // 7. Glowing letter badge
        const QRectF badge(33, 27, 18, 18);
        QPainterPath bPath;
        bPath.addRoundedRect(badge, 4.5, 4.5);

        // Badge outer glow
        QColor bGlow = outlineColor;
        bGlow.setAlpha(80);
        p.setPen(QPen(bGlow, 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(bPath);

        // Badge fill
        p.fillPath(bPath, outlineColor);
        const QColor textColor = outlineColor.lightness() > 140 ? QColor(18, 20, 24) : QColor(255, 255, 255);
        p.setPen(textColor);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(11);
        p.setFont(f);
        p.drawText(badge, Qt::AlignCenter, QStringLiteral("A"));

        // 8. Solid glowing perimeter outline
        QPen outlinePen(outlineColor, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(outlinePen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    } else {
        // Disabled / Inactive State
        // 1. Dim card fill
        p.fillPath(path, QColor(22, 24, 28, 200));

        // 2. Titlebar separator
        p.setPen(QPen(QColor(42, 45, 52, 140), 1.2));
        p.drawLine(QPointF(outer.left(), 22), QPointF(outer.right(), 22));

        // 3. Muted gray KDE titlebar buttons on the RIGHT side
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(80, 85, 95));
        p.drawEllipse(QRectF(34, 13, 4.5, 4.5));
        p.drawEllipse(QRectF(40.5, 13, 4.5, 4.5));
        p.drawEllipse(QRectF(47, 13, 4.5, 4.5));

        // 4. Muted left titlebar dash
        p.setBrush(QColor(60, 65, 75));
        p.drawRoundedRect(QRectF(13, 14, 12, 2.5), 1.25, 1.25);

        // 5. Muted window content lines
        p.setBrush(QColor(36, 40, 48));
        p.drawRoundedRect(QRectF(13, 27, 16, 2.5), 1.25, 1.25);
        p.drawRoundedRect(QRectF(13, 33, 12, 2.5), 1.25, 1.25);
        p.drawRoundedRect(QRectF(13, 39, 14, 2.5), 1.25, 1.25);

        // 6. Muted letter badge
        const QRectF badge(33, 27, 18, 18);
        p.setPen(QPen(QColor(70, 75, 85), 1.2, Qt::DashLine));
        p.setBrush(QColor(28, 30, 36, 180));
        p.drawRoundedRect(badge, 4.5, 4.5);
        p.setPen(QColor(95, 100, 110));
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(11);
        p.setFont(f);
        p.drawText(badge, Qt::AlignCenter, QStringLiteral("A"));

        // 7. Dashed muted perimeter outline
        QPen outlinePen(QColor(105, 110, 120, 140), 1.8, Qt::DashLine);
        p.setPen(outlinePen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    p.end();
    return QIcon(pix);
}

QString getGlobalShortcutText(const QString &actionId, const QString &defaultShortcut = QString())
{
    // 1. Try live KGlobalAccel API
    const QList<QKeySequence> seqs = KGlobalAccel::self()->globalShortcut(QStringLiteral("kwin"), actionId);
    if (!seqs.isEmpty() && !seqs.first().isEmpty()) {
        return seqs.first().toString(QKeySequence::NativeText);
    }

    // 2. Direct read from kglobalshortcutsrc on disk
    auto config = KSharedConfig::openConfig(QStringLiteral("kglobalshortcutsrc"));
    config->reparseConfiguration();
    KConfigGroup kwinGroup(config, QStringLiteral("kwin"));
    if (kwinGroup.hasKey(actionId)) {
        const QString entry = kwinGroup.readEntry(actionId, QString());
        const QString currentPart = entry.section(QLatin1Char(','), 0, 0).trimmed();
        if (currentPart.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
            return QString();
        }
        const QString firstSeq = currentPart.section(QLatin1Char('\t'), 0, 0).trimmed();
        if (!firstSeq.isEmpty()) {
            return QKeySequence(firstSeq).toString(QKeySequence::NativeText);
        }
    }

    return defaultShortcut;
}

} // namespace

class OutlineTray : public QObject
{
    Q_OBJECT

public:
    explicit OutlineTray(QObject *parent = nullptr)
        : QObject(parent)
    {
        initTray();
        refreshStateFromConfig();
        initWatcher();
        connectToDBus();
        m_trayIcon->show();
    }

    ~OutlineTray() override
    {
        if (m_trayIcon) {
            m_trayIcon->hide();
        }
        if (m_settingsDialog) {
            m_settingsDialog->close();
            delete m_settingsDialog;
        }
    }

private slots:
    void onToggleOutline()
    {
        if (callDBusMethod(QStringLiteral("toggleOutline"))) {
            refreshStateFromConfig();
            QTimer::singleShot(40, this, &OutlineTray::refreshStateFromConfig);
            return;
        }
        // Fallback: invoke shortcut ToggleActiveWindowOutline
        invokeShortcut(QStringLiteral("ToggleActiveWindowOutline"));
        notifyKWinReconfigure();
        QTimer::singleShot(60, this, &OutlineTray::refreshStateFromConfig);
    }

    void onTriggerJumpMode()
    {
        if (callDBusMethod(QStringLiteral("triggerJumpMode"))) {
            return;
        }
        invokeShortcut(QStringLiteral("JumpToWindow"));
    }

    void onToggleWindowMark()
    {
        if (callDBusMethod(QStringLiteral("toggleWindowMark"))) {
            return;
        }
        invokeShortcut(QStringLiteral("MarkActiveWindow"));
    }

    void onMarkWindowWithColor()
    {
        if (callDBusMethod(QStringLiteral("markWindowWithColor"))) {
            return;
        }
        invokeShortcut(QStringLiteral("MarkActiveWindowWithColor"));
    }

    void onClearAllMarks()
    {
        if (callDBusMethod(QStringLiteral("clearAllMarks"))) {
            return;
        }
        notifyKWinReconfigure();
    }

    void onPrevRecentWindow()
    {
        if (callDBusMethod(QStringLiteral("prevRecentWindow"))) {
            return;
        }
        invokeShortcut(QStringLiteral("PrevRecentWindow"));
    }

    void onNextRecentWindow()
    {
        if (callDBusMethod(QStringLiteral("nextRecentWindow"))) {
            return;
        }
        invokeShortcut(QStringLiteral("NextRecentWindow"));
    }

    void onJumpToRecentWindow()
    {
        if (callDBusMethod(QStringLiteral("slotJumpToRecentWindow"))) {
            return;
        }
        invokeShortcut(QStringLiteral("RecentWindow"));
    }

    void onShowShortcutsCheatSheet()
    {
        const QString scJump = getGlobalShortcutText(QStringLiteral("JumpToWindow"), QStringLiteral("Meta+Space"));
        const QString scRecent = getGlobalShortcutText(QStringLiteral("RecentWindow"), QString());
        const QString scJumpRecent = QStringLiteral("%1 Space").arg(scJump);
        const QString scMark = getGlobalShortcutText(QStringLiteral("MarkActiveWindow"), QString());
        const QString scMarkColor = getGlobalShortcutText(QStringLiteral("MarkActiveWindowWithColor"), QStringLiteral("Meta+Ctrl+Space"));
        const QString scToggleGroup = getGlobalShortcutText(QStringLiteral("ToggleHudGrouping"), QStringLiteral("Meta+Ctrl+G"));
        const QString scToggle = getGlobalShortcutText(QStringLiteral("ToggleActiveWindowOutline"), QStringLiteral("Meta+Ctrl+O"));
        const QString scPrev = getGlobalShortcutText(QStringLiteral("PrevRecentWindow"), QStringLiteral("Meta+Ctrl+["));
        const QString scNext = getGlobalShortcutText(QStringLiteral("NextRecentWindow"), QStringLiteral("Meta+Ctrl+]"));

        auto formatRow = [](const QString &sc, const QString &desc) -> QString {
            const QString dispSc = (sc.isEmpty() || sc.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0)
                ? QStringLiteral("<i>(none)</i>")
                : QStringLiteral("<b><kbd>%1</kbd></b>").arg(sc.toHtmlEscaped());
            return QStringLiteral("<tr><td style='padding: 4px 12px 4px 0;'>%1</td><td>&nbsp;—&nbsp;</td><td style='padding: 4px 0;'>%2</td></tr>")
                .arg(dispSc, desc);
        };

        QString rows;
        rows += formatRow(scJump, QStringLiteral("Jump to Window (interactive letter jump)"));
        rows += formatRow(QStringLiteral("Hold Meta + Space / Shift+Space"), QStringLiteral("Traverse recent windows (Alt+Tab style; release Meta to switch)"));
        rows += formatRow(scJumpRecent, QStringLiteral("Jump to last active window (recent)"));
        if (!scRecent.isEmpty() && scRecent.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0) {
            rows += formatRow(scRecent, QStringLiteral("Switch to recent window (direct shortcut)"));
        }
        rows += formatRow(scMark, QStringLiteral("Quick mark active window"));
        rows += formatRow(scMarkColor, QStringLiteral("Custom Mark Window (Color, Letter, Label)"));
        rows += formatRow(scToggleGroup, QStringLiteral("Toggle HUD Window Grouping (or Tab key during Leap)"));
        rows += formatRow(scToggle, QStringLiteral("Toggle LeapK window highlight on/off"));
        rows += formatRow(scPrev, QStringLiteral("Navigate to Previous Window in focus history"));
        rows += formatRow(scNext, QStringLiteral("Navigate to Next Window in focus history"));

        QMessageBox msgBox(nullptr);
        msgBox.setWindowTitle(QStringLiteral("Keyboard Shortcuts — LeapK"));
        msgBox.setIcon(QMessageBox::Information);
        const QString text = QStringLiteral(
            "<h3>⌨️ Available Keyboard Shortcuts (System-synchronized):</h3>"
            "<table cellpadding='2' style='border-collapse: collapse;'>%1</table>"
            "<p><small>Shortcuts can be customized in <i>System Settings → Shortcuts → KWin</i>.</small></p>")
            .arg(rows);
        msgBox.setText(text);
        msgBox.exec();
    }

    void onCheckForUpdates()
    {
        if (!m_networkManager) {
            m_networkManager = new QNetworkAccessManager(this);
        }

        m_trayIcon->showMessage(
            QStringLiteral("LeapK"),
            QStringLiteral("Checking for updates on GitHub…"),
            QSystemTrayIcon::Information,
            2000
        );

        QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/gmalinowski/leapk/releases")));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("LeapK-Tray"));
        request.setRawHeader("Accept", "application/vnd.github.v3+json");

        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                const QString err = reply->errorString();
                QMessageBox::warning(nullptr,
                    QStringLiteral("Update Check Error"),
                    QStringLiteral("Failed to connect to GitHub:\n%1").arg(err));
                return;
            }

            const QByteArray responseData = reply->readAll();
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
                QMessageBox::warning(nullptr,
                    QStringLiteral("Update Check Error"),
                    QStringLiteral("Invalid response received from GitHub API."));
                return;
            }

            const QJsonArray releases = doc.array();
            if (releases.isEmpty()) {
                m_trayIcon->showMessage(
                    QStringLiteral("Active Window Outline"),
                    QStringLiteral("No releases found on GitHub."),
                    QSystemTrayIcon::Information,
                    3000
                );
                return;
            }

            const QJsonObject latestRelease = releases.first().toObject();
            const QString tagName = latestRelease.value(QStringLiteral("tag_name")).toString();
            const bool isPrerelease = latestRelease.value(QStringLiteral("prerelease")).toBool(false);
            const QString htmlUrl = latestRelease.value(QStringLiteral("html_url")).toString();
            const QString body = latestRelease.value(QStringLiteral("body")).toString();

            QString cleanTag = tagName;
            if (cleanTag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
                cleanTag.remove(0, 1);
            }

            const QString currentVerStr = QStringLiteral(ACTIVE_WINDOW_OUTLINE_VERSION);
            const QVersionNumber currentVer = QVersionNumber::fromString(currentVerStr);
            const QVersionNumber remoteVer = QVersionNumber::fromString(cleanTag);

            const bool isDev = currentVer.isNull();
            const bool hasUpdate = isDev || (remoteVer > currentVer);

            if (!hasUpdate) {
                LeapK::showUpToDateDialog(nullptr, currentVerStr, htmlUrl);
                return;
            }

            LeapK::showNewVersionDialog(nullptr, tagName, currentVerStr, isPrerelease, htmlUrl, body);
        });
    }

    void onOpenSettings()
    {
        if (!m_settingsDialog) {
            m_settingsDialog = new SettingsDialog();
            connect(m_settingsDialog, &QDialog::finished, this, [this]() {
                refreshStateFromConfig();
            });
        }
        m_settingsDialog->show();
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
    }

    void onTrayActivated(QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Trigger) {
            // Single left click: toggle outline immediately
            onToggleOutline();
        }
    }

    bool isOutlineActive() const
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/LeapK"),
            QStringLiteral("org.kde.LeapK"),
            QStringLiteral("isOutlineEnabled"));
        QDBusReply<bool> reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 250);
        if (reply.isValid()) {
            return reply.value();
        }
        msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/KLeap"),
            QStringLiteral("org.kde.KLeap"),
            QStringLiteral("isOutlineEnabled"));
        reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 250);
        if (reply.isValid()) {
            return reply.value();
        }
        msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/ActiveWindowOutline"),
            QStringLiteral("org.kde.ActiveWindowOutline"),
            QStringLiteral("isOutlineEnabled"));
        reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 250);
        if (reply.isValid()) {
            return reply.value();
        }

        auto cfg = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
        cfg->reparseConfiguration();
        KConfigGroup group = getKWinGroup(cfg);
        return group.readEntry(QStringLiteral("OutlineEnabled"), true);
    }

    void updateShortcutsMenu()
    {
        if (!m_menuShortcuts) {
            return;
        }

        const QString scJump = getGlobalShortcutText(QStringLiteral("JumpToWindow"), QStringLiteral("Meta+Space"));
        const QString scRecent = getGlobalShortcutText(QStringLiteral("RecentWindow"), QString());
        const QString scJumpRecent = QStringLiteral("%1 Space").arg(scJump);
        const QString scMark = getGlobalShortcutText(QStringLiteral("MarkActiveWindow"), QString());
        const QString scMarkColor = getGlobalShortcutText(QStringLiteral("MarkActiveWindowWithColor"), QStringLiteral("Meta+Ctrl+Space"));
        const QString scToggleGroup = getGlobalShortcutText(QStringLiteral("ToggleHudGrouping"), QStringLiteral("Meta+Ctrl+G"));
        const QString scToggle = getGlobalShortcutText(QStringLiteral("ToggleActiveWindowOutline"), QStringLiteral("Meta+Ctrl+O"));
        const QString scPrev = getGlobalShortcutText(QStringLiteral("PrevRecentWindow"), QStringLiteral("Meta+Ctrl+["));
        const QString scNext = getGlobalShortcutText(QStringLiteral("NextRecentWindow"), QStringLiteral("Meta+Ctrl+]"));

        auto formatMenuText = [](const QString &label, const QString &sc) -> QString {
            if (sc.isEmpty() || sc.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
                return label;
            }
            return QStringLiteral("%1\t%2").arg(label, sc);
        };

        if (m_actJump) {
            m_actJump->setText(formatMenuText(QStringLiteral("🔲 Jump to Window..."), scJump));
        }
        if (m_actRecent) {
            const QString recentShortcutDisp = (!scRecent.isEmpty() && scRecent.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0)
                ? QStringLiteral("%1 / %2").arg(scJumpRecent, scRecent)
                : scJumpRecent;
            m_actRecent->setText(formatMenuText(QStringLiteral("🔀 Jump to Recent Window"), recentShortcutDisp));
        }
        if (m_actMark) {
            m_actMark->setText(formatMenuText(QStringLiteral("🏷️ Mark / Unmark Window"), scMark));
        }
        if (m_actMarkColor) {
            m_actMarkColor->setText(formatMenuText(QStringLiteral("🎨 Custom Mark (Color, Letter, Label)..."), scMarkColor));
        }
        if (m_actToggle) {
            m_actToggle->setText(formatMenuText(QStringLiteral("🖼️ Toggle Outline"), scToggle));
        }
        if (m_actPrev) {
            m_actPrev->setText(formatMenuText(QStringLiteral("⏮️ Previous Window"), scPrev));
        }
        if (m_actNext) {
            m_actNext->setText(formatMenuText(QStringLiteral("⏭️ Next Window"), scNext));
        }
    }

    void refreshStateFromConfig()
    {
        const bool enabled = isOutlineActive();

        auto cfg = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
        cfg->reparseConfiguration();
        KConfigGroup group = getKWinGroup(cfg);

        const bool useAccent = group.readEntry(QStringLiteral("UseAccentColor"), true);
        QColor outlineColor = group.readEntry(QStringLiteral("CustomColor"), QColor(QStringLiteral("#3daee9")));

        if (useAccent) {
            auto generalCfg = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
            generalCfg->reparseConfiguration();
            KConfigGroup generalGroup(generalCfg, QStringLiteral("General"));
            outlineColor = generalGroup.readEntry(QStringLiteral("AccentColor"), qApp->palette().color(QPalette::Highlight));
        }
        if (!outlineColor.isValid()) {
            outlineColor = QColor(61, 174, 233);
        }

        const QString scToggle = getGlobalShortcutText(QStringLiteral("ToggleActiveWindowOutline"), QStringLiteral("Meta+Ctrl+O"));
        const QString scSuffix = (scToggle.isEmpty() || scToggle.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0)
            ? QString()
            : QStringLiteral(" (%1)").arg(scToggle);

        if (m_actEnable) {
            const bool wasBlocked = m_actEnable->blockSignals(true);
            m_actEnable->setChecked(enabled);
            m_actEnable->blockSignals(wasBlocked);
            m_actEnable->setText(enabled
                ? QStringLiteral("✔ Outline is Enabled%1").arg(scSuffix)
                : QStringLiteral("✖ Enable Outline%1").arg(scSuffix));
        }

        m_trayIcon->setIcon(createTrayIconPixmap(enabled, outlineColor));
        const QString statusText = enabled
            ? QStringLiteral("LeapK: Enabled\n(Click left mouse button to disable)")
            : QStringLiteral("LeapK: Disabled\n(Click left mouse button to enable)");
        m_trayIcon->setToolTip(statusText);

        updateShortcutsMenu();
    }

private:
    bool callDBusMethod(const QString &methodName)
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/LeapK"),
            QStringLiteral("org.kde.LeapK"),
            methodName);
        QDBusReply<void> reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 500);
        if (reply.isValid()) {
            return true;
        }
        msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/KLeap"),
            QStringLiteral("org.kde.KLeap"),
            methodName);
        reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 500);
        if (reply.isValid()) {
            return true;
        }
        msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/ActiveWindowOutline"),
            QStringLiteral("org.kde.ActiveWindowOutline"),
            methodName);
        reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 500);
        return reply.isValid();
    }

    void notifyKWinReconfigure()
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Effects"),
            QStringLiteral("org.kde.kwin.Effects"),
            QStringLiteral("reconfigureEffect"));
        msg << QStringLiteral("leapk");
        QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
    }

    void invokeShortcut(const QString &actionName)
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.kglobalaccel"),
            QStringLiteral("/component/kwin"),
            QStringLiteral("org.kde.kglobalaccel.Component"),
            QStringLiteral("invokeShortcut"));
        msg << actionName;
        QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
    }

    void initTray()
    {
        m_trayIcon = new QSystemTrayIcon(this);
        m_menu = new QMenu();
        connect(m_menu, &QMenu::aboutToShow, this, &OutlineTray::refreshStateFromConfig);

        // 1. Title / Header
        QAction *actHeader = m_menu->addAction(QStringLiteral("🚀 LeapK v%1").arg(QStringLiteral(ACTIVE_WINDOW_OUTLINE_VERSION)));
        QFont headerFont = actHeader->font();
        headerFont.setBold(true);
        actHeader->setFont(headerFont);
        actHeader->setEnabled(false);

        m_menu->addSeparator();

        // 2. Primary Toggle
        m_actEnable = m_menu->addAction(QStringLiteral("Enable Outline"),
                                        this, &OutlineTray::onToggleOutline);
        m_actEnable->setCheckable(true);

        m_menu->addSeparator();

        // 3. Keyboard Shortcuts Submenu
        m_menuShortcuts = m_menu->addMenu(QStringLiteral("⌨️ Keyboard Shortcuts"));
        connect(m_menuShortcuts, &QMenu::aboutToShow, this, &OutlineTray::updateShortcutsMenu);

        m_actJump = m_menuShortcuts->addAction(QString(), this, &OutlineTray::onTriggerJumpMode);
        m_actRecent = m_menuShortcuts->addAction(QString(), this, &OutlineTray::onJumpToRecentWindow);
        m_actMark = m_menuShortcuts->addAction(QString(), this, &OutlineTray::onToggleWindowMark);
        m_actMarkColor = m_menuShortcuts->addAction(QString(), this, &OutlineTray::onMarkWindowWithColor);
        m_actToggle = m_menuShortcuts->addAction(QString(), this, &OutlineTray::onToggleOutline);
        m_actPrev = m_menuShortcuts->addAction(QString(), this, &OutlineTray::onPrevRecentWindow);
        m_actNext = m_menuShortcuts->addAction(QString(), this, &OutlineTray::onNextRecentWindow);

        m_menuShortcuts->addSeparator();

        m_menuShortcuts->addAction(
            QStringLiteral("🧹 Clear All Marks"),
            this, &OutlineTray::onClearAllMarks);

        m_menuShortcuts->addAction(
            QStringLiteral("📋 Shortcuts Cheat Sheet..."),
            this, &OutlineTray::onShowShortcutsCheatSheet);

        m_menu->addSeparator();

        m_menu->addAction(QStringLiteral("📁 Workspace Manager…"),
                          this, []() {
            QProcess::startDetached(QStringLiteral("leapk-workspace-manager"));
        });

        m_menu->addAction(QStringLiteral("🏷️ Marker & Group Manager…"),
                          this, []() {
            QProcess::startDetached(QStringLiteral("leapk-marker-manager"));
        });

        m_menu->addSeparator();

        // 5. Updates & Settings
        m_menu->addAction(QStringLiteral("🔄 Check for Updates…"),
                          this, &OutlineTray::onCheckForUpdates);

        m_menu->addAction(QStringLiteral("⚙️ Settings..."),
                          this, &OutlineTray::onOpenSettings);

        m_trayIcon->setContextMenu(m_menu);
        connect(m_trayIcon, &QSystemTrayIcon::activated, this, &OutlineTray::onTrayActivated);

        updateShortcutsMenu();
    }

    void initWatcher()
    {
        const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        m_watcher = new QFileSystemWatcher(this);

        const QString kwinrcPath = configDir + QStringLiteral("/kwinrc");
        if (QFile::exists(kwinrcPath)) {
            m_watcher->addPath(kwinrcPath);
        }

        const QString shortcutsPath = configDir + QStringLiteral("/kglobalshortcutsrc");
        if (QFile::exists(shortcutsPath)) {
            m_watcher->addPath(shortcutsPath);
        }

        connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
            if (path.endsWith(QStringLiteral("kglobalshortcutsrc"))) {
                updateShortcutsMenu();
            }
            refreshStateFromConfig();
        });
    }

    void connectToDBus()
    {
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/ActiveWindowOutline"),
            QStringLiteral("org.kde.ActiveWindowOutline"),
            QStringLiteral("stateChanged"),
            this,
            SLOT(refreshStateFromConfig()));
    }

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
    QMenu *m_menuShortcuts = nullptr;
    QAction *m_actEnable = nullptr;
    QAction *m_actJump = nullptr;
    QAction *m_actRecent = nullptr;
    QAction *m_actMark = nullptr;
    QAction *m_actMarkColor = nullptr;
    QAction *m_actToggle = nullptr;
    QAction *m_actPrev = nullptr;
    QAction *m_actNext = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("leapk-tray"));
    app.setApplicationDisplayName(QStringLiteral("LeapK Tray"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("leapk"),
                                       QIcon(QStringLiteral(":/icons/leapk.svg"))));
    app.setQuitOnLastWindowClosed(false);

    if (app.arguments().contains(QStringLiteral("--settings"))
        || app.arguments().contains(QStringLiteral("-s"))
        || app.arguments().contains(QStringLiteral("settings"))
        || app.applicationFilePath().endsWith(QStringLiteral("leapk-settings"))
        || app.applicationFilePath().endsWith(QStringLiteral("active-window-outline-settings"))) {
        SettingsDialog dialog;
        dialog.show();
        return app.exec();
    }

    // Single-instance lock
    QLockFile lock(QDir::tempPath() + QStringLiteral("/leapk-tray.lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100)) {
        std::cerr << "leapk-tray is already running." << std::endl;
        return 0;
    }

    OutlineTray tray;
    return app.exec();
}

#include "tray.moc"
