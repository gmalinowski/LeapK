/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QProcess>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace LeapK
{

inline bool openUrlInBrowser(const QString &urlStr)
{
    if (urlStr.trimmed().isEmpty()) {
        return false;
    }
    const QString trimmed = urlStr.trimmed();
    const QUrl url(trimmed);

    if (QDesktopServices::openUrl(url)) {
        return true;
    }
    if (QProcess::startDetached(QStringLiteral("xdg-open"), QStringList{trimmed})) {
        return true;
    }
    if (QProcess::startDetached(QStringLiteral("gio"), QStringList{QStringLiteral("open"), trimmed})) {
        return true;
    }
    return false;
}

inline void showNewVersionDialog(QWidget *parent,
                                const QString &tagName,
                                const QString &currentVerStr,
                                bool isPrerelease,
                                const QString &htmlUrl,
                                const QString &body,
                                bool isPolish)
{
    const QString installCmd = isPrerelease
        ? QStringLiteral("curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash -s -- --pre")
        : QStringLiteral("curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash");

    QDialog dlg(parent);
    dlg.setWindowTitle(isPolish ? QStringLiteral("Dostępna jest nowa wersja!")
                                : QStringLiteral("New Version Available!"));
    dlg.setWindowIcon(QIcon::fromTheme(QStringLiteral("leapk"),
                                       QIcon(QStringLiteral(":/icons/leapk.svg"))));
    dlg.resize(560, 460);

    auto *vbox = new QVBoxLayout(&dlg);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(12);

    auto *info = new QLabel(&dlg);
    info->setTextFormat(Qt::RichText);
    info->setWordWrap(true);
    info->setText(isPolish
        ? QStringLiteral("<h3>🎉 Dostępna jest nowa wersja: <b>%1</b></h3>"
                         "<p>Obecnie zainstalowana wersja: <b>v%2</b><br>"
                         "Najnowsze wydanie na GitHubie: <b>%1</b> %3</p>")
            .arg(tagName, currentVerStr, isPrerelease ? QStringLiteral("(pre-release)") : QStringLiteral("(stabilne)"))
        : QStringLiteral("<h3>🎉 A new version is available: <b>%1</b></h3>"
                         "<p>Currently installed: <b>v%2</b><br>"
                         "Latest release on GitHub: <b>%1</b> %3</p>")
            .arg(tagName, currentVerStr, isPrerelease ? QStringLiteral("(pre-release)") : QStringLiteral("(stable)")));
    vbox->addWidget(info);

    if (!body.trimmed().isEmpty()) {
        auto *notesLabel = new QLabel(isPolish ? QStringLiteral("<b>Informacje o wydaniu (Release Notes):</b>")
                                               : QStringLiteral("<b>Release Notes:</b>"), &dlg);
        vbox->addWidget(notesLabel);

        auto *browser = new QTextBrowser(&dlg);
        browser->setOpenExternalLinks(true);
        browser->setPlainText(body);
        browser->setMaximumHeight(180);
        vbox->addWidget(browser);
    }

    auto *cmdLabel = new QLabel(isPolish ? QStringLiteral("Polecenie instalacji / aktualizacji w terminalu:")
                                         : QStringLiteral("Terminal installation / update command:"), &dlg);
    vbox->addWidget(cmdLabel);

    auto *cmdBox = new QLineEdit(installCmd, &dlg);
    cmdBox->setReadOnly(true);
    vbox->addWidget(cmdBox);

    auto *btnLayout = new QHBoxLayout();

    auto *btnOpenUrl = new QPushButton(isPolish ? QStringLiteral("🌐 Otwórz stronę wydania")
                                                : QStringLiteral("🌐 Open Release Page"), &dlg);
    btnOpenUrl->setIcon(QIcon::fromTheme(QStringLiteral("internet-web-browser")));
    QObject::connect(btnOpenUrl, &QPushButton::clicked, [htmlUrl]() {
        if (!htmlUrl.isEmpty()) {
            openUrlInBrowser(htmlUrl);
        }
    });
    btnLayout->addWidget(btnOpenUrl);

    auto *btnCopy = new QPushButton(isPolish ? QStringLiteral("📋 Kopiuj polecenie")
                                             : QStringLiteral("📋 Copy Command"), &dlg);
    btnCopy->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    QObject::connect(btnCopy, &QPushButton::clicked, [installCmd, btnCopy, isPolish]() {
        QGuiApplication::clipboard()->setText(installCmd);
        btnCopy->setText(isPolish ? QStringLiteral("✓ Skopiowano!") : QStringLiteral("✓ Copied!"));
    });
    btnLayout->addWidget(btnCopy);

    btnLayout->addStretch();

    auto *btnClose = new QPushButton(isPolish ? QStringLiteral("Zamknij") : QStringLiteral("Close"), &dlg);
    QObject::connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(btnClose);

    vbox->addLayout(btnLayout);
    dlg.exec();
}

inline void showUpToDateDialog(QWidget *parent,
                               const QString &currentVerStr,
                               const QString &htmlUrl,
                               bool isPolish)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(isPolish ? QStringLiteral("LeapK — Aktualizacje")
                                : QStringLiteral("LeapK — Updates"));
    dlg.setWindowIcon(QIcon::fromTheme(QStringLiteral("leapk"),
                                       QIcon(QStringLiteral(":/icons/leapk.svg"))));
    dlg.resize(480, 190);

    auto *vbox = new QVBoxLayout(&dlg);
    vbox->setContentsMargins(16, 16, 16, 16);
    vbox->setSpacing(12);

    auto *lbl = new QLabel(&dlg);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);
    lbl->setText(isPolish
        ? QStringLiteral("<h3>✓ LeapK jest aktualny!</h3>"
                         "<p>Masz zainstalowaną najnowszą wersję: <b>v%1</b>.</p>")
            .arg(currentVerStr)
        : QStringLiteral("<h3>✓ LeapK is up to date!</h3>"
                         "<p>You have the latest version of LeapK installed: <b>v%1</b>.</p>")
            .arg(currentVerStr));
    vbox->addWidget(lbl);

    auto *btnLayout = new QHBoxLayout();
    auto *btnOpenUrl = new QPushButton(isPolish ? QStringLiteral("🌐 Otwórz stronę wydania")
                                                : QStringLiteral("🌐 Open Release Page"), &dlg);
    btnOpenUrl->setIcon(QIcon::fromTheme(QStringLiteral("internet-web-browser")));
    QObject::connect(btnOpenUrl, &QPushButton::clicked, [htmlUrl]() {
        const QString urlToOpen = htmlUrl.isEmpty()
            ? QStringLiteral("https://github.com/gmalinowski/leapk/releases")
            : htmlUrl;
        openUrlInBrowser(urlToOpen);
    });
    btnLayout->addWidget(btnOpenUrl);

    btnLayout->addStretch();

    auto *btnClose = new QPushButton(isPolish ? QStringLiteral("Zamknij") : QStringLiteral("Close"), &dlg);
    QObject::connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnLayout->addWidget(btnClose);

    vbox->addLayout(btnLayout);
    dlg.exec();
}

} // namespace LeapK
