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
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusInterface>
#include <QDBusReply>
#include <KConfigGroup>
#include <KSharedConfig>
#include "presethelper.h"
#include <iostream>

namespace
{

QIcon createTrayIconPixmap(bool enabled, const QColor &outlineColor = QColor(61, 174, 233))
{
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    // Outer dark window card
    const QRectF outer(6, 8, 52, 48);
    QPainterPath path;
    path.addRoundedRect(outer, 10, 10);

    // Inner window fill
    p.fillPath(path, QColor(24, 28, 34, 240));

    // Titlebar divider line
    p.setPen(QPen(QColor(60, 66, 76), 1.5));
    p.drawLine(QPointF(6, 22), QPointF(58, 22));

    // Three window action dots
    p.setBrush(enabled ? QColor(231, 76, 60) : QColor(100, 105, 115));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(12, 13, 5, 5));
    p.setBrush(enabled ? QColor(241, 196, 15) : QColor(100, 105, 115));
    p.drawEllipse(QRectF(20, 13, 5, 5));
    p.setBrush(enabled ? QColor(46, 204, 113) : QColor(100, 105, 115));
    p.drawEllipse(QRectF(28, 13, 5, 5));

    // Active outline perimeter
    if (enabled) {
        // Vibrant glowing outline
        QPen outlinePen(outlineColor, 3.5);
        p.setPen(outlinePen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        // Accent badge in top right corner
        const QRectF badge(40, 28, 14, 14);
        QPainterPath bPath;
        bPath.addRoundedRect(badge, 4, 4);
        p.fillPath(bPath, outlineColor);
        p.setPen(QColor(0, 0, 0, 200));
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(9);
        p.setFont(f);
        p.drawText(badge, Qt::AlignCenter, QStringLiteral("A"));
    } else {
        // Dimmed disabled dashed outline
        QPen outlinePen(QColor(120, 125, 135, 140), 2.0, Qt::DashLine);
        p.setPen(outlinePen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    p.end();
    return QIcon(pix);
}

} // namespace

class OutlineTray : public QObject
{
    Q_OBJECT

public:
    explicit OutlineTray(QObject *parent = nullptr)
        : QObject(parent)
        , m_isPolish(QLocale().language() == QLocale::Polish)
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
    }

private slots:
    void onToggleOutline()
    {
        if (callDBusMethod(QStringLiteral("toggleOutline"))) {
            return;
        }
        // Fallback: write to kwinrc and call reconfigureEffect
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
        const bool current = group.readEntry(QStringLiteral("OutlineEnabled"), true);
        group.writeEntry(QStringLiteral("OutlineEnabled"), !current);
        group.sync();
        notifyKWinReconfigure();
        refreshStateFromConfig();
    }

    void onToggleAmbilight()
    {
        if (callDBusMethod(QStringLiteral("toggleAmbilight"))) {
            return;
        }
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
        const bool current = group.readEntry(QStringLiteral("AmbilightEnabled"), false);
        group.writeEntry(QStringLiteral("AmbilightEnabled"), !current);
        group.sync();
        notifyKWinReconfigure();
        refreshStateFromConfig();
    }

    void onToggleEnsureContrast()
    {
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
        const bool current = group.readEntry(QStringLiteral("EnsurePerimeterContrast"), true);
        group.writeEntry(QStringLiteral("EnsurePerimeterContrast"), !current);
        group.sync();
        notifyKWinReconfigure();
        refreshStateFromConfig();
    }

    void onToggleGlow()
    {
        if (callDBusMethod(QStringLiteral("toggleGlow"))) {
            return;
        }
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
        const bool current = group.readEntry(QStringLiteral("GlowEnabled"), false);
        group.writeEntry(QStringLiteral("GlowEnabled"), !current);
        group.sync();
        notifyKWinReconfigure();
        refreshStateFromConfig();
    }

    void onToggleFadeCorners()
    {
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
        const bool current = group.readEntry(QStringLiteral("FadeCorners"), true);
        group.writeEntry(QStringLiteral("FadeCorners"), !current);
        group.sync();
        notifyKWinReconfigure();
        refreshStateFromConfig();
    }

    void onApplyPreset(const QString &presetName)
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/ActiveWindowOutline"),
            QStringLiteral("org.kde.ActiveWindowOutline"),
            QStringLiteral("applyPreset"));
        msg << presetName;
        QDBusReply<void> reply = QDBusConnection::sessionBus().call(msg);
        if (reply.isValid()) {
            refreshStateFromConfig();
            return;
        }

        // Fallback: load preset info directly and write to kwinrc
        const auto presets = OutlinePreset::loadPresets(OutlinePreset::defaultPresetsDirectory());
        for (const auto &p : presets) {
            if (p.name == presetName) {
                KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
                for (auto it = p.settings.constBegin(); it != p.settings.constEnd(); ++it) {
                    group.writeEntry(it.key(), it.value().toVariant());
                }
                group.sync();
                notifyKWinReconfigure();
                refreshStateFromConfig();
                break;
            }
        }
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
        invokeShortcut(QStringLiteral("MarkWindowToggle"));
    }

    void onMarkWindowWithColor()
    {
        if (callDBusMethod(QStringLiteral("markWindowWithColor"))) {
            return;
        }
        invokeShortcut(QStringLiteral("MarkWindowWithColor"));
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

    void onOpenSettings()
    {
        QProcess::startDetached(QStringLiteral("kcmshell6"), {QStringLiteral("kwin_active-window-outline")});
    }

    void onTrayActivated(QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Trigger) {
            // Single left click: toggle outline on/off quickly
            onToggleOutline();
        }
    }

    void refreshStateFromConfig()
    {
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));

        const bool enabled = group.readEntry(QStringLiteral("OutlineEnabled"), true);
        const bool ambilight = group.readEntry(QStringLiteral("AmbilightEnabled"), false);
        const bool contrast = group.readEntry(QStringLiteral("EnsurePerimeterContrast"), true);
        const bool glow = group.readEntry(QStringLiteral("GlowEnabled"), false);
        const bool fadeCorners = group.readEntry(QStringLiteral("FadeCorners"), true);
        const QColor customColor = group.readEntry(QStringLiteral("CustomColor"), QColor(QStringLiteral("#3daee9")));

        if (m_actEnable) m_actEnable->setChecked(enabled);
        if (m_actAmbilight) m_actAmbilight->setChecked(ambilight);
        if (m_actEnsureContrast) m_actEnsureContrast->setChecked(contrast);
        if (m_actGlow) m_actGlow->setChecked(glow);
        if (m_actFadeCorners) m_actFadeCorners->setChecked(fadeCorners);

        m_trayIcon->setIcon(createTrayIconPixmap(enabled, customColor.isValid() ? customColor : QColor(61, 174, 233)));
        const QString statusText = enabled
            ? (m_isPolish ? QStringLiteral("Aktywny obrys okna: Włączony") : QStringLiteral("Active Window Outline: Enabled"))
            : (m_isPolish ? QStringLiteral("Aktywny obrys okna: Wyłączony") : QStringLiteral("Active Window Outline: Disabled"));
        m_trayIcon->setToolTip(statusText);

        rebuildPresetsMenu();
    }

private:
    bool callDBusMethod(const QString &methodName)
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/ActiveWindowOutline"),
            QStringLiteral("org.kde.ActiveWindowOutline"),
            methodName);
        QDBusReply<void> reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 500);
        return reply.isValid();
    }

    void notifyKWinReconfigure()
    {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Effects"),
            QStringLiteral("org.kde.kwin.Effects"),
            QStringLiteral("reconfigureEffect"));
        msg << QStringLiteral("active-window-outline");
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

        // 1. Title / Header
        QAction *actHeader = m_menu->addAction(QStringLiteral("🖼️ Active Window Outline v1.5"));
        QFont headerFont = actHeader->font();
        headerFont.setBold(true);
        actHeader->setFont(headerFont);
        actHeader->setEnabled(false);

        m_menu->addSeparator();

        // 2. Primary Toggle
        m_actEnable = m_menu->addAction(m_isPolish ? QStringLiteral("Włącz obrys okna") : QStringLiteral("Enable Outline"),
                                        this, &OutlineTray::onToggleOutline);
        m_actEnable->setCheckable(true);

        // 3. Presets Submenu
        m_menuPresets = m_menu->addMenu(m_isPolish ? QStringLiteral("📁 Presety") : QStringLiteral("📁 Presets"));

        m_menu->addSeparator();

        // 4. Visual Modifiers
        m_actAmbilight = m_menu->addAction(m_isPolish ? QStringLiteral("Tryb Ambilight (kolor adaptacyjny)") : QStringLiteral("Ambilight Mode (Adaptive Color)"),
                                           this, &OutlineTray::onToggleAmbilight);
        m_actAmbilight->setCheckable(true);

        m_actEnsureContrast = m_menu->addAction(m_isPolish ? QStringLiteral("Gwarantuj kontrast tła") : QStringLiteral("Ensure Perimeter Contrast"),
                                                this, &OutlineTray::onToggleEnsureContrast);
        m_actEnsureContrast->setCheckable(true);

        m_actGlow = m_menu->addAction(m_isPolish ? QStringLiteral("Miękka poświata (Glow)") : QStringLiteral("Soft Glow"),
                                      this, &OutlineTray::onToggleGlow);
        m_actGlow->setCheckable(true);

        m_actFadeCorners = m_menu->addAction(m_isPolish ? QStringLiteral("Zanikanie narożników") : QStringLiteral("Fade Out Corners"),
                                             this, &OutlineTray::onToggleFadeCorners);
        m_actFadeCorners->setCheckable(true);

        m_menu->addSeparator();

        // 5. Quick Actions
        m_menu->addAction(m_isPolish ? QStringLiteral("🔲 Skocz do okna... (Meta+Space)") : QStringLiteral("🔲 Jump to Window... (Meta+Space)"),
                          this, &OutlineTray::onTriggerJumpMode);
        m_menu->addAction(m_isPolish ? QStringLiteral("🏷️ Oznacz okno (Meta+Ctrl+M)") : QStringLiteral("🏷️ Mark Window (Meta+Ctrl+M)"),
                          this, &OutlineTray::onToggleWindowMark);
        m_menu->addAction(m_isPolish ? QStringLiteral("🎨 Ustawienia oznaczenia... (Meta+Ctrl+Shift+M)") : QStringLiteral("🎨 Custom Mark Window... (Meta+Ctrl+Shift+M)"),
                          this, &OutlineTray::onMarkWindowWithColor);
        m_menu->addAction(m_isPolish ? QStringLiteral("🧹 Wyczyść wszystkie oznaczenia") : QStringLiteral("🧹 Clear All Marks"),
                          this, &OutlineTray::onClearAllMarks);

        m_menu->addSeparator();

        // 6. Navigation
        m_menu->addAction(m_isPolish ? QStringLiteral("⏮️ Poprzednie okno (Meta+Alt+[)") : QStringLiteral("⏮️ Previous Window (Meta+Alt+[)"),
                          this, &OutlineTray::onPrevRecentWindow);
        m_menu->addAction(m_isPolish ? QStringLiteral("⏭️ Następne okno (Meta+Alt+])") : QStringLiteral("⏭️ Next Window (Meta+Alt+])"),
                          this, &OutlineTray::onNextRecentWindow);

        m_menu->addSeparator();

        // 7. Settings & Exit
        m_menu->addAction(m_isPolish ? QStringLiteral("⚙️ Ustawienia...") : QStringLiteral("⚙️ Settings..."),
                          this, &OutlineTray::onOpenSettings);
        m_menu->addSeparator();
        m_menu->addAction(m_isPolish ? QStringLiteral("❌ Zamknij tackę") : QStringLiteral("❌ Quit Tray"),
                          qApp, &QCoreApplication::quit);

        m_trayIcon->setContextMenu(m_menu);
        connect(m_trayIcon, &QSystemTrayIcon::activated, this, &OutlineTray::onTrayActivated);
    }

    void rebuildPresetsMenu()
    {
        if (!m_menuPresets) {
            return;
        }
        m_menuPresets->clear();

        const auto presets = OutlinePreset::loadPresets(OutlinePreset::defaultPresetsDirectory());
        auto *actionGroup = new QActionGroup(m_menuPresets);
        actionGroup->setExclusive(true);

        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
        const QString activeBorderWidth = group.readEntry(QStringLiteral("BorderWidth"), QStringLiteral("3"));
        const QString activeCustomColor = group.readEntry(QStringLiteral("CustomColor"), QStringLiteral("#3daee9"));

        for (const auto &p : presets) {
            QAction *act = m_menuPresets->addAction(p.name);
            act->setCheckable(true);
            actionGroup->addAction(act);

            // Heuristic check if current settings match preset
            if (p.settings.contains(QStringLiteral("BorderWidth"))
                && p.settings[QStringLiteral("BorderWidth")].toString() == activeBorderWidth
                && p.settings.contains(QStringLiteral("CustomColor"))
                && p.settings[QStringLiteral("CustomColor")].toString() == activeCustomColor) {
                act->setChecked(true);
            }

            const QString name = p.name;
            connect(act, &QAction::triggered, this, [this, name]() {
                onApplyPreset(name);
            });
        }
    }

    void initWatcher()
    {
        const QString configPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/kwinrc");
        if (QFile::exists(configPath)) {
            m_watcher = new QFileSystemWatcher(this);
            m_watcher->addPath(configPath);
            connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &OutlineTray::refreshStateFromConfig);
        }
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

    bool m_isPolish;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_menu = nullptr;
    QMenu *m_menuPresets = nullptr;
    QAction *m_actEnable = nullptr;
    QAction *m_actAmbilight = nullptr;
    QAction *m_actEnsureContrast = nullptr;
    QAction *m_actGlow = nullptr;
    QAction *m_actFadeCorners = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("active-window-outline-tray"));
    app.setApplicationDisplayName(QStringLiteral("Active Window Outline Tray"));
    app.setQuitOnLastWindowClosed(false);

    // Single-instance lock
    QLockFile lock(QDir::tempPath() + QStringLiteral("/active-window-outline-tray.lock"));
    if (!lock.tryLock(100)) {
        std::cerr << "active-window-outline-tray is already running." << std::endl;
        return 0;
    }

    OutlineTray tray;
    return app.exec();
}

#include "tray.moc"
