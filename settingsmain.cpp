/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QMessageBox>
#include "settingsdialog.h"
#include "packhelper.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("leapk-settings"));
    app.setApplicationDisplayName(QStringLiteral("LeapK Settings"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("leapk"),
                                       QIcon(QStringLiteral(":/icons/leapk.svg"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("LeapK Configuration and Pack Manager"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption installPackOpt(QStringList() << QStringLiteral("install-pack") << QStringLiteral("i"),
        QStringLiteral("Install a LeapK Pack (.leapkpack file)"), QStringLiteral("file"));
    parser.addOption(installPackOpt);
    parser.addPositionalArgument(QStringLiteral("[pack-file]"), QStringLiteral("Optional .leapkpack file to install"));

    parser.process(app);

    QString packToInstall;
    if (parser.isSet(installPackOpt)) {
        packToInstall = parser.value(installPackOpt);
    } else if (!parser.positionalArguments().isEmpty()) {
        const QString arg = parser.positionalArguments().first();
        if (arg.endsWith(QStringLiteral(".leapkpack"), Qt::CaseInsensitive)) {
            packToInstall = arg;
        }
    }

    if (!packToInstall.isEmpty()) {
        LeapKPack::PackBundle bundle;
        QString err;
        if (!LeapKPack::importPackFromFile(packToInstall, bundle, &err)) {
            QMessageBox::critical(nullptr, QStringLiteral("LeapK Pack Error"),
                QStringLiteral("Failed to open LeapK Pack file '%1':\n%2").arg(packToInstall, err));
            return 1;
        }

        QString details;
        if (bundle.hasTheme) details += QStringLiteral("• Theme: %1\n").arg(bundle.theme.name);
        if (bundle.hasPreset) details += QStringLiteral("• Preset: %1\n").arg(bundle.preset.name);
        if (bundle.hasShaders && !bundle.shaders.isEmpty()) {
            QStringList names;
            for (const auto &s : bundle.shaders) names << s.name;
            details += QStringLiteral("• Shaders: %1\n").arg(names.join(QStringLiteral(", ")));
        }
        if (bundle.hasShortcuts && !bundle.shortcuts.isEmpty()) {
            details += QStringLiteral("• Shortcuts: %1 action keybinding(s)\n").arg(bundle.shortcuts.size());
        }

        const auto reply = QMessageBox::question(
            nullptr,
            QStringLiteral("Install Pack: %1").arg(bundle.manifest.name.isEmpty() ? bundle.manifest.id : bundle.manifest.name),
            QStringLiteral("Install complete LeapK Pack '%1' (version %2 by %3)?\n\nIncluded components:\n%4\nDo you want to proceed and activate this pack?")
                .arg(bundle.manifest.name, bundle.manifest.version,
                     bundle.manifest.author.isEmpty() ? QStringLiteral("Unknown") : bundle.manifest.author,
                     details),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );

        if (reply == QMessageBox::Yes) {
            LeapKPack::InstallOptions opts;
            opts.installTheme = bundle.hasTheme;
            opts.installPreset = bundle.hasPreset;
            opts.installShaders = bundle.hasShaders;
            opts.installShortcuts = bundle.hasShortcuts;

            if (!LeapKPack::installPack(bundle, opts, &err)) {
                QMessageBox::critical(nullptr, QStringLiteral("Installation Error"),
                    QStringLiteral("Failed to install pack:\n%1").arg(err));
                return 1;
            }

            QMessageBox::information(nullptr, QStringLiteral("LeapK Pack Installed"),
                QStringLiteral("LeapK Pack '%1' installed successfully!").arg(bundle.manifest.name));
            return 0;
        }
        return 0;
    }

    SettingsDialog dialog;
    dialog.show();
    return app.exec();
}
