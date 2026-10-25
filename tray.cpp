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
#include <KConfigGroup>
#include <KSharedConfig>
#include "presethelper.h"
#include "settingsdialog.h"
#include <iostream>

#ifndef ACTIVE_WINDOW_OUTLINE_VERSION
#define ACTIVE_WINDOW_OUTLINE_VERSION "1.5.0"
#endif

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
        if (m_settingsDialog) {
            m_settingsDialog->close();
            delete m_settingsDialog;
        }
    }

private slots:
    void onToggleOutline()
    {
        if (callDBusMethod(QStringLiteral("toggleOutline"))) {
            return;
        }
        // Fallback: invoke shortcut ToggleActiveWindowOutline
        invokeShortcut(QStringLiteral("ToggleActiveWindowOutline"));
        notifyKWinReconfigure();
        refreshStateFromConfig();
    }

    void onApplyPreset(const QString &presetName)
    {
        // Remember active preset name in kwinrc
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
        group.writeEntry(QStringLiteral("ActivePreset"), presetName);
        group.sync();

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

    void onSaveCurrentPreset()
    {
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
        const QString presetName = QStringLiteral("Preset_%1").arg(timestamp);

        OutlinePreset::PresetInfo p;
        p.name = presetName;
        p.description = m_isPolish
            ? QStringLiteral("Zapisano z tacki systemowej (%1)").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
            : QStringLiteral("Saved from system tray (%1)").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        p.isBuiltIn = false;

        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
        const QJsonObject defaults = OutlinePreset::getDefaultSettings();
        for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
            const QString key = it.key();
            if (group.hasKey(key)) {
                const QVariant val = group.readEntry(key, it.value().toVariant());
                p.settings[key] = QJsonValue::fromVariant(val);
            } else {
                p.settings[key] = it.value();
            }
        }

        QString err;
        if (!OutlinePreset::savePreset(OutlinePreset::defaultPresetsDirectory(), p, &err)) {
            QMessageBox::warning(nullptr,
                m_isPolish ? QStringLiteral("Błąd zapisu") : QStringLiteral("Save Error"),
                m_isPolish ? QStringLiteral("Nie udało się zapisać presetu: %1").arg(err)
                           : QStringLiteral("Failed to save preset: %1").arg(err));
            return;
        }

        group.writeEntry(QStringLiteral("ActivePreset"), presetName);
        group.sync();

        rebuildPresetsMenu();

        m_trayIcon->showMessage(
            QStringLiteral("Active Window Outline"),
            m_isPolish ? QStringLiteral("Zapisano nowy preset:\n%1").arg(presetName)
                       : QStringLiteral("Saved new preset:\n%1").arg(presetName),
            QSystemTrayIcon::Information,
            3500
        );
    }

    void onExportPreset()
    {
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
        const QString defaultFileName = QStringLiteral("ActiveWindowOutline_Preset_%1.json").arg(timestamp);
        const QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        const QString initialPath = docsPath + QLatin1Char('/') + defaultFileName;

        const QString filePath = QFileDialog::getSaveFileName(
            nullptr,
            m_isPolish ? QStringLiteral("Eksportuj obecny preset do pliku") : QStringLiteral("Export Current Preset to File"),
            initialPath,
            QStringLiteral("JSON Files (*.json)")
        );
        if (filePath.isEmpty()) {
            return;
        }

        OutlinePreset::PresetInfo p;
        p.name = QStringLiteral("Exported_%1").arg(timestamp);
        p.description = m_isPolish
            ? QStringLiteral("Wyeksportowano z ustawień: %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
            : QStringLiteral("Exported at: %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        p.isBuiltIn = false;

        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));
        const QJsonObject defaults = OutlinePreset::getDefaultSettings();
        for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
            const QString key = it.key();
            if (group.hasKey(key)) {
                const QVariant val = group.readEntry(key, it.value().toVariant());
                p.settings[key] = QJsonValue::fromVariant(val);
            } else {
                p.settings[key] = it.value();
            }
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(nullptr,
                m_isPolish ? QStringLiteral("Błąd eksportu") : QStringLiteral("Export Error"),
                m_isPolish ? QStringLiteral("Nie można zapisać pliku: %1").arg(file.errorString())
                       : QStringLiteral("Cannot write to file: %1").arg(file.errorString()));
            return;
        }
        file.write(OutlinePreset::serializePreset(p));
        file.close();

        m_trayIcon->showMessage(
            QStringLiteral("Active Window Outline"),
            m_isPolish ? QStringLiteral("Preset pomyślnie wyeksportowany!")
                       : QStringLiteral("Preset exported successfully!"),
            QSystemTrayIcon::Information,
            3000
        );
    }

    void onImportPreset()
    {
        const QString filePath = QFileDialog::getOpenFileName(
            nullptr,
            m_isPolish ? QStringLiteral("Importuj plik presetu") : QStringLiteral("Import Preset File"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            QStringLiteral("JSON Files (*.json)")
        );
        if (filePath.isEmpty()) {
            return;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(nullptr,
                m_isPolish ? QStringLiteral("Błąd importu") : QStringLiteral("Import Error"),
                m_isPolish ? QStringLiteral("Nie można odczytać pliku: %1").arg(file.errorString())
                           : QStringLiteral("Cannot read file: %1").arg(file.errorString()));
            return;
        }

        OutlinePreset::PresetInfo preset;
        QString err;
        if (!OutlinePreset::parsePresetJson(file.readAll(), preset, &err)) {
            QMessageBox::warning(nullptr,
                m_isPolish ? QStringLiteral("Błąd importu") : QStringLiteral("Import Error"),
                m_isPolish ? QStringLiteral("Nieprawidłowy format pliku presetu: %1").arg(err)
                           : QStringLiteral("Invalid preset file format: %1").arg(err));
            return;
        }

        if (preset.name.isEmpty()) {
            preset.name = QFileInfo(filePath).baseName();
        }
        preset.isBuiltIn = false;

        if (!OutlinePreset::savePreset(OutlinePreset::defaultPresetsDirectory(), preset, &err)) {
            QMessageBox::warning(nullptr,
                m_isPolish ? QStringLiteral("Błąd importu") : QStringLiteral("Import Error"),
                m_isPolish ? QStringLiteral("Nie można zapisać presetu: %1").arg(err)
                           : QStringLiteral("Could not save preset: %1").arg(err));
            return;
        }

        onApplyPreset(preset.name);

        m_trayIcon->showMessage(
            QStringLiteral("Active Window Outline"),
            m_isPolish ? QStringLiteral("Zaimportowano i aktywowano preset:\n%1").arg(preset.name)
                       : QStringLiteral("Imported and activated preset:\n%1").arg(preset.name),
            QSystemTrayIcon::Information,
            3500
        );
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

    void onShowShortcutsCheatSheet()
    {
        QMessageBox msgBox(nullptr);
        msgBox.setWindowTitle(m_isPolish ? QStringLiteral("Skróty klawiszowe — Active Window Outline")
                                         : QStringLiteral("Keyboard Shortcuts — Active Window Outline"));
        msgBox.setIcon(QMessageBox::Information);
        const QString text = m_isPolish
            ? QStringLiteral(
                "<h3>⌨️ Dostępne skróty klawiszowe:</h3>"
                "<table cellpadding='4'>"
                "<tr><td><b>Meta + Space</b></td><td>&nbsp;—&nbsp;</td><td>Skocz do okna (Jump Mode z literami)</td></tr>"
                "<tr><td><b>Meta + Ctrl + M</b></td><td>&nbsp;—&nbsp;</td><td>Oznacz / Odznacz aktywne okno</td></tr>"
                "<tr><td><b>Meta + Ctrl + Shift + M</b></td><td>&nbsp;—&nbsp;</td><td>Oznacz okno z wyborem koloru / litery</td></tr>"
                "<tr><td><b>Meta + Ctrl + O</b></td><td>&nbsp;—&nbsp;</td><td>Włącz / Wyłącz aktywny obrys</td></tr>"
                "<tr><td><b>Meta + Alt + [</b></td><td>&nbsp;—&nbsp;</td><td>Przejdź do poprzedniego okna w historii</td></tr>"
                "<tr><td><b>Meta + Alt + ]</b></td><td>&nbsp;—&nbsp;</td><td>Przejdź do następnego okna w historii</td></tr>"
                "</table>"
                "<p><small>Skróty można w pełni dostosować w Ustawieniach systemowych → Skróty → KWin.</small></p>")
            : QStringLiteral(
                "<h3>⌨️ Available Keyboard Shortcuts:</h3>"
                "<table cellpadding='4'>"
                "<tr><td><b>Meta + Space</b></td><td>&nbsp;—&nbsp;</td><td>Jump to Window (interactive letter jump)</td></tr>"
                "<tr><td><b>Meta + Ctrl + M</b></td><td>&nbsp;—&nbsp;</td><td>Mark / Unmark Active Window</td></tr>"
                "<tr><td><b>Meta + Ctrl + Shift + M</b></td><td>&nbsp;—&nbsp;</td><td>Mark Window with Color Picker / Letter</td></tr>"
                "<tr><td><b>Meta + Ctrl + O</b></td><td>&nbsp;—&nbsp;</td><td>Toggle Active Window Outline on/off</td></tr>"
                "<tr><td><b>Meta + Alt + [</b></td><td>&nbsp;—&nbsp;</td><td>Navigate to Previous Window in focus history</td></tr>"
                "<tr><td><b>Meta + Alt + ]</b></td><td>&nbsp;—&nbsp;</td><td>Navigate to Next Window in focus history</td></tr>"
                "</table>"
                "<p><small>Shortcuts can be customized in System Settings → Shortcuts → KWin.</small></p>");
        msgBox.setText(text);
        msgBox.exec();
    }

    void onCheckForUpdates()
    {
        if (!m_networkManager) {
            m_networkManager = new QNetworkAccessManager(this);
        }

        m_trayIcon->showMessage(
            QStringLiteral("Active Window Outline"),
            m_isPolish ? QStringLiteral("Sprawdzanie dostępności aktualizacji na GitHubie…")
                       : QStringLiteral("Checking for updates on GitHub…"),
            QSystemTrayIcon::Information,
            2000
        );

        QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/gmalinowski/kwin-active-window-outline/releases")));
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("ActiveWindowOutline-Tray"));
        request.setRawHeader("Accept", "application/vnd.github.v3+json");

        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                const QString err = reply->errorString();
                QMessageBox::warning(nullptr,
                    m_isPolish ? QStringLiteral("Błąd aktualizacji") : QStringLiteral("Update Check Error"),
                    m_isPolish ? QStringLiteral("Nie udało się połączyć z GitHubem:\n%1").arg(err)
                               : QStringLiteral("Failed to connect to GitHub:\n%1").arg(err));
                return;
            }

            const QByteArray responseData = reply->readAll();
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
                QMessageBox::warning(nullptr,
                    m_isPolish ? QStringLiteral("Błąd aktualizacji") : QStringLiteral("Update Check Error"),
                    m_isPolish ? QStringLiteral("Nieprawidłowa odpowiedź z serwera GitHub.")
                               : QStringLiteral("Invalid response received from GitHub API."));
                return;
            }

            const QJsonArray releases = doc.array();
            if (releases.isEmpty()) {
                m_trayIcon->showMessage(
                    QStringLiteral("Active Window Outline"),
                    m_isPolish ? QStringLiteral("Brak wydań na GitHubie.") : QStringLiteral("No releases found on GitHub."),
                    QSystemTrayIcon::Information,
                    3000
                );
                return;
            }

            const QJsonObject latestRelease = releases.first().toObject();
            const QString tagName = latestRelease.value(QStringLiteral("tag_name")).toString();
            const bool isPrerelease = latestRelease.value(QStringLiteral("prerelease")).toBool(false);
            const QString htmlUrl = latestRelease.value(QStringLiteral("html_url")).toString();

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
                m_trayIcon->showMessage(
                    QStringLiteral("Active Window Outline"),
                    m_isPolish ? QStringLiteral("Wtyczka jest aktualna! Zainstalowana wersja: v%1").arg(currentVerStr)
                               : QStringLiteral("Plugin is up to date! Installed version: v%1").arg(currentVerStr),
                    QSystemTrayIcon::Information,
                    3000
                );
                return;
            }

            // New version available dialog
            QDialog dlg(nullptr);
            dlg.setWindowTitle(m_isPolish ? QStringLiteral("Dostępna jest nowa wersja!") : QStringLiteral("New Version Available!"));
            dlg.resize(480, 260);

            auto *vbox = new QVBoxLayout(&dlg);
            vbox->setContentsMargins(16, 16, 16, 16);
            vbox->setSpacing(12);

            auto *lbl = new QLabel(&dlg);
            lbl->setTextFormat(Qt::RichText);
            lbl->setWordWrap(true);
            lbl->setText(m_isPolish
                ? QStringLiteral("<h3>🎉 Dostępna jest nowa wersja: <b>%1</b></h3>"
                                 "<p>Obecnie zainstalowana wersja: <b>v%2</b><br>"
                                 "Najnowsze wydanie na GitHubie: <b>%1</b> %3</p>")
                    .arg(tagName, currentVerStr, isPrerelease ? QStringLiteral("(pre-release)") : QStringLiteral("(stabilne)"))
                : QStringLiteral("<h3>🎉 A new version is available: <b>%1</b></h3>"
                                 "<p>Currently installed: <b>v%2</b><br>"
                                 "Latest release on GitHub: <b>%1</b> %3</p>")
                    .arg(tagName, currentVerStr, isPrerelease ? QStringLiteral("(pre-release)") : QStringLiteral("(stable)")));
            vbox->addWidget(lbl);

            auto *btnBox = new QHBoxLayout();
            auto *btnOpen = new QPushButton(m_isPolish ? QStringLiteral("🌐 Otwórz stronę wydania") : QStringLiteral("🌐 Open Release Page"), &dlg);
            auto *btnClose = new QPushButton(m_isPolish ? QStringLiteral("Zamknij") : QStringLiteral("Close"), &dlg);
            btnBox->addWidget(btnOpen);
            btnBox->addStretch();
            btnBox->addWidget(btnClose);
            vbox->addLayout(btnBox);

            connect(btnOpen, &QPushButton::clicked, [&dlg, htmlUrl]() {
                QDesktopServices::openUrl(QUrl(htmlUrl));
                dlg.accept();
            });
            connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::reject);

            dlg.exec();
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

    void refreshStateFromConfig()
    {
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-active-window-outline"));

        const bool enabled = group.readEntry(QStringLiteral("OutlineEnabled"), true);
        const QColor customColor = group.readEntry(QStringLiteral("CustomColor"), QColor(QStringLiteral("#3daee9")));

        if (m_actEnable) {
            m_actEnable->setChecked(enabled);
            m_actEnable->setText(enabled
                ? (m_isPolish ? QStringLiteral("✔ Obrys okna jest włączony (Meta+Ctrl+O)") : QStringLiteral("✔ Outline is Enabled (Meta+Ctrl+O)"))
                : (m_isPolish ? QStringLiteral("✖ Włącz obrys okna (Meta+Ctrl+O)") : QStringLiteral("✖ Enable Outline (Meta+Ctrl+O)")));
        }

        m_trayIcon->setIcon(createTrayIconPixmap(enabled, customColor.isValid() ? customColor : QColor(61, 174, 233)));
        const QString statusText = enabled
            ? (m_isPolish ? QStringLiteral("Active Window Outline: Włączony\n(Kliknij lewym przyciskiem myszy, aby wyłączyć)")
                          : QStringLiteral("Active Window Outline: Enabled\n(Click left mouse button to disable)"))
            : (m_isPolish ? QStringLiteral("Active Window Outline: Wyłączony\n(Kliknij lewym przyciskiem myszy, aby włączyć)")
                          : QStringLiteral("Active Window Outline: Disabled\n(Click left mouse button to enable)"));
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

        m_menu->addSeparator();

        // 3. Presets Submenu
        m_menuPresets = m_menu->addMenu(m_isPolish ? QStringLiteral("📁 Presety") : QStringLiteral("📁 Presets"));

        m_menu->addSeparator();

        // 4. Keyboard Shortcuts Submenu
        m_menuShortcuts = m_menu->addMenu(m_isPolish ? QStringLiteral("⌨️ Skróty klawiszowe") : QStringLiteral("⌨️ Keyboard Shortcuts"));

        m_menuShortcuts->addAction(
            m_isPolish ? QStringLiteral("🔲 Skocz do okna...\tMeta+Space") : QStringLiteral("🔲 Jump to Window...\tMeta+Space"),
            this, &OutlineTray::onTriggerJumpMode);

        m_menuShortcuts->addAction(
            m_isPolish ? QStringLiteral("🏷️ Oznacz / Odznacz okno\tMeta+Ctrl+M") : QStringLiteral("🏷️ Mark / Unmark Window\tMeta+Ctrl+M"),
            this, &OutlineTray::onToggleWindowMark);

        m_menuShortcuts->addAction(
            m_isPolish ? QStringLiteral("🎨 Oznacz z paletą barw...\tMeta+Ctrl+Shift+M") : QStringLiteral("🎨 Custom Mark Window...\tMeta+Ctrl+Shift+M"),
            this, &OutlineTray::onMarkWindowWithColor);

        m_menuShortcuts->addAction(
            m_isPolish ? QStringLiteral("🖼️ Włącz / Wyłącz obrys\tMeta+Ctrl+O") : QStringLiteral("🖼️ Toggle Outline\tMeta+Ctrl+O"),
            this, &OutlineTray::onToggleOutline);

        m_menuShortcuts->addAction(
            m_isPolish ? QStringLiteral("⏮️ Poprzednie okno w historii\tMeta+Alt+[") : QStringLiteral("⏮️ Previous Window\tMeta+Alt+["),
            this, &OutlineTray::onPrevRecentWindow);

        m_menuShortcuts->addAction(
            m_isPolish ? QStringLiteral("⏭️ Następne okno w historii\tMeta+Alt+]") : QStringLiteral("⏭️ Next Window\tMeta+Alt+]"),
            this, &OutlineTray::onNextRecentWindow);

        m_menuShortcuts->addSeparator();

        m_menuShortcuts->addAction(
            m_isPolish ? QStringLiteral("🧹 Wyczyść wszystkie oznaczenia") : QStringLiteral("🧹 Clear All Marks"),
            this, &OutlineTray::onClearAllMarks);

        m_menuShortcuts->addAction(
            m_isPolish ? QStringLiteral("📋 Ściąga skrótów klawiszowych...") : QStringLiteral("📋 Shortcuts Cheat Sheet..."),
            this, &OutlineTray::onShowShortcutsCheatSheet);

        m_menu->addSeparator();

        // 5. Updates & Settings
        m_menu->addAction(m_isPolish ? QStringLiteral("🔄 Sprawdź czy jest dostępna nowa wersja") : QStringLiteral("🔄 Check for Updates…"),
                          this, &OutlineTray::onCheckForUpdates);

        m_menu->addAction(m_isPolish ? QStringLiteral("⚙️ Ustawienia...") : QStringLiteral("⚙️ Settings..."),
                          this, &OutlineTray::onOpenSettings);

        m_menu->addSeparator();

        // 6. Exit
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
        const QString activePresetName = group.readEntry(QStringLiteral("ActivePreset"), QString());

        for (const auto &p : presets) {
            QAction *act = m_menuPresets->addAction(p.name);
            act->setCheckable(true);
            actionGroup->addAction(act);

            if (!activePresetName.isEmpty() && p.name == activePresetName) {
                act->setChecked(true);
            }

            const QString name = p.name;
            connect(act, &QAction::triggered, this, [this, name]() {
                onApplyPreset(name);
            });
        }

        m_menuPresets->addSeparator();

        m_menuPresets->addAction(
            m_isPolish ? QStringLiteral("💾 Zapisz obecny jako nowy preset (timestamp)")
                       : QStringLiteral("💾 Save Current as New Preset (Timestamp)"),
            this, &OutlineTray::onSaveCurrentPreset);

        m_menuPresets->addAction(
            m_isPolish ? QStringLiteral("📤 Eksportuj obecny preset...")
                       : QStringLiteral("📤 Export Current Preset..."),
            this, &OutlineTray::onExportPreset);

        m_menuPresets->addAction(
            m_isPolish ? QStringLiteral("📥 Importuj preset...")
                       : QStringLiteral("📥 Import Preset..."),
            this, &OutlineTray::onImportPreset);
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
    QMenu *m_menuShortcuts = nullptr;
    QAction *m_actEnable = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("active-window-outline-tray"));
    app.setApplicationDisplayName(QStringLiteral("Active Window Outline Tray"));
    app.setQuitOnLastWindowClosed(false);

    if (app.arguments().contains(QStringLiteral("--settings"))
        || app.arguments().contains(QStringLiteral("-s"))
        || app.arguments().contains(QStringLiteral("settings"))
        || app.applicationFilePath().endsWith(QStringLiteral("active-window-outline-settings"))) {
        SettingsDialog dialog;
        dialog.show();
        return app.exec();
    }

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
