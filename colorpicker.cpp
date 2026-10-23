/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QColorDialog>
#include <QColor>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("active-window-outline-colorpicker"));
    app.setApplicationDisplayName(QObject::tr("Active Window Outline - Mark Color"));

    QColor initialColor(QStringLiteral("#e74c3c"));
    if (argc > 1 && argv[1]) {
        const QColor parsed(QString::fromUtf8(argv[1]));
        if (parsed.isValid()) {
            initialColor = parsed;
        }
    }

    QColorDialog dlg(initialColor);
    dlg.setOption(QColorDialog::ShowAlphaChannel, true);
    dlg.setWindowTitle(QObject::tr("Select Window Mark Color"));

    if (dlg.exec() == QDialog::Accepted) {
        const QColor selected = dlg.selectedColor();
        if (selected.isValid()) {
            std::cout << selected.name(QColor::HexArgb).toStdString() << std::endl;
            return 0;
        }
    }
    return 1;
}
