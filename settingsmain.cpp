/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include "settingsdialog.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("active-window-outline-settings"));
    app.setApplicationDisplayName(QStringLiteral("Active Window Outline Settings"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-system-windows")));

    SettingsDialog dialog;
    dialog.show();
    return app.exec();
}
