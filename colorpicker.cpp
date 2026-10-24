/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QRegularExpressionValidator>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("active-window-outline-colorpicker"));
    app.setApplicationDisplayName(QObject::tr("Active Window Outline - Mark Window"));

    QColor currentColor(QStringLiteral("#e74c3c"));
    if (argc > 1 && argv[1]) {
        const QColor parsed(QString::fromUtf8(argv[1]));
        if (parsed.isValid()) {
            currentColor = parsed;
        }
    }

    QString currentLetter = QStringLiteral("A");
    if (argc > 2 && argv[2]) {
        currentLetter = QString::fromUtf8(argv[2]).trimmed().toUpper();
        if (currentLetter.isEmpty()) {
            currentLetter = QStringLiteral("A");
        }
    }

    QDialog dlg;
    dlg.setWindowTitle(QObject::tr("Mark Window Settings"));
    dlg.setMinimumWidth(320);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(14);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    auto *letterEdit = new QLineEdit(&dlg);
    letterEdit->setMaxLength(1);
    letterEdit->setText(currentLetter);
    letterEdit->setAlignment(Qt::AlignCenter);
    QFont letterFont = letterEdit->font();
    letterFont.setPointSize(letterFont.pointSize() + 4);
    letterFont.setBold(true);
    letterEdit->setFont(letterFont);
    letterEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9]")), letterEdit));
    letterEdit->selectAll();
    formLayout->addRow(QObject::tr("Mark Letter (A-Z / 0-9):"), letterEdit);

    auto *colorBtn = new QPushButton(&dlg);
    const auto updateColorBtn = [colorBtn, &currentColor]() {
        const QString hex = (currentColor.alpha() < 255) ? currentColor.name(QColor::HexArgb) : currentColor.name(QColor::HexRgb);
        colorBtn->setText(hex);
        const QString textCol = (currentColor.lightness() > 128) ? QStringLiteral("#000000") : QStringLiteral("#ffffff");
        colorBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: %2; font-weight: bold; padding: 6px 12px; border-radius: 4px; border: 1px solid rgba(0,0,0,0.3); }").arg(hex, textCol));
    };
    updateColorBtn();

    QObject::connect(colorBtn, &QPushButton::clicked, [&dlg, &currentColor, updateColorBtn]() {
        QColorDialog colDlg(currentColor, &dlg);
        colDlg.setOption(QColorDialog::ShowAlphaChannel, true);
        colDlg.setWindowTitle(QObject::tr("Select Mark Color"));
        if (colDlg.exec() == QDialog::Accepted) {
            currentColor = colDlg.selectedColor();
            updateColorBtn();
        }
    });
    formLayout->addRow(QObject::tr("Mark Color:"), colorBtn);

    layout->addLayout(formLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (dlg.exec() == QDialog::Accepted) {
        QString finalLetter = letterEdit->text().trimmed().toUpper();
        if (finalLetter.isEmpty()) {
            finalLetter = currentLetter;
        }
        const QString outColor = (currentColor.alpha() < 255) ? currentColor.name(QColor::HexArgb) : currentColor.name(QColor::HexRgb);
        std::cout << outColor.toStdString() << " " << finalLetter.toStdString() << std::endl;
        return 0;
    }
    return 1;
}
