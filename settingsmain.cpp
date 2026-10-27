/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include "settingsdialog.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("leapk-settings"));
    app.setApplicationDisplayName(QStringLiteral("LeapK Settings"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("leapk"),
                                       QIcon(QStringLiteral(":/icons/leapk.svg"))));

    SettingsDialog dialog;
    dialog.show();
    return app.exec();
}
