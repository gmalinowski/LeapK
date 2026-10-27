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
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QRegularExpressionValidator>
#include <QSettings>
#include "badgehelper.h"
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("leapk-colorpicker"));
    app.setApplicationDisplayName(QObject::tr("LeapK - Mark Window"));

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

    QSettings settings(QStringLiteral("leapk"), QStringLiteral("colorpicker"));
    if (!settings.contains(QStringLiteral("MarkWithLetter"))) {
        QSettings oldSettings(QStringLiteral("kwin-active-window-outline"), QStringLiteral("colorpicker"));
        if (oldSettings.contains(QStringLiteral("MarkWithLetter"))) {
            settings.setValue(QStringLiteral("MarkWithLetter"), oldSettings.value(QStringLiteral("MarkWithLetter")));
            settings.setValue(QStringLiteral("MarkWithColor"), oldSettings.value(QStringLiteral("MarkWithColor")));
            settings.setValue(QStringLiteral("MarkPersistent"), oldSettings.value(QStringLiteral("MarkPersistent"), true));
        }
    }
    bool useLetter = settings.value(QStringLiteral("MarkWithLetter"), true).toBool();
    bool useColor = settings.value(QStringLiteral("MarkWithColor"), true).toBool();
    bool usePersistent = settings.value(QStringLiteral("MarkPersistent"), true).toBool();

    if (argc > 3 && argv[3]) {
        const QString arg3 = QString::fromUtf8(argv[3]).trimmed();
        if (arg3 == QStringLiteral("1") || arg3 == QStringLiteral("true")) {
            useLetter = true;
        } else if (arg3 == QStringLiteral("0") || arg3 == QStringLiteral("false")) {
            useLetter = false;
        }
    }
    if (argc > 4 && argv[4]) {
        const QString arg4 = QString::fromUtf8(argv[4]).trimmed();
        if (arg4 == QStringLiteral("1") || arg4 == QStringLiteral("true")) {
            useColor = true;
        } else if (arg4 == QStringLiteral("0") || arg4 == QStringLiteral("false")) {
            useColor = false;
        }
    }

    QString currentLabel;
    if (argc > 5 && argv[5]) {
        currentLabel = QString::fromUtf8(argv[5]).trimmed();
    }
    if (argc > 6 && argv[6]) {
        const QString arg6 = QString::fromUtf8(argv[6]).trimmed();
        if (arg6 == QStringLiteral("1") || arg6 == QStringLiteral("true")) {
            usePersistent = true;
        } else if (arg6 == QStringLiteral("0") || arg6 == QStringLiteral("false")) {
            usePersistent = false;
        }
    }

    const bool isPolish = (QLocale().language() == QLocale::Polish);

    QDialog dlg;
    dlg.setWindowTitle(isPolish ? QStringLiteral("Ustawienia oznaczenia okna") : QObject::tr("Mark Window Settings"));
    dlg.setMinimumWidth(380);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(14);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(12);

    auto *chkLetter = new QCheckBox(isPolish ? QStringLiteral("Oznacz stałą literą:") : QObject::tr("Mark with fixed letter:"), &dlg);
    chkLetter->setChecked(useLetter);

    auto *letterEdit = new QLineEdit(&dlg);
    letterEdit->setMaxLength(1);
    letterEdit->setText(currentLetter);
    letterEdit->setAlignment(Qt::AlignCenter);
    QFont letterFont = letterEdit->font();
    letterFont.setPointSize(letterFont.pointSize() + 4);
    letterFont.setBold(true);
    letterEdit->setFont(letterFont);
    letterEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[A-Za-z0-9]")), letterEdit));
    letterEdit->setEnabled(useLetter);
    if (useLetter) {
        letterEdit->selectAll();
    }
    formLayout->addRow(chkLetter, letterEdit);

    auto *chkColor = new QCheckBox(isPolish ? QStringLiteral("Własny kolor oznaczenia:") : QObject::tr("Custom mark color:"), &dlg);
    chkColor->setChecked(useColor);

    auto *colorBtn = new QPushButton(&dlg);
    const auto updateColorBtn = [colorBtn, chkColor, &currentColor, isPolish]() {
        const QString hex = (currentColor.alpha() < 255) ? currentColor.name(QColor::HexArgb) : currentColor.name(QColor::HexRgb);
        if (chkColor && !chkColor->isChecked()) {
            colorBtn->setText(isPolish ? QStringLiteral("Domyślny / Losowy kolor") : QObject::tr("Default / Random Color"));
        } else {
            colorBtn->setText(hex);
        }
        const QColor textCol = OutlineBadge::contrastingTextColor(currentColor);
        const QString textColStr = (textCol == Qt::black) ? QStringLiteral("#000000") : QStringLiteral("#ffffff");
        colorBtn->setStyleSheet(QStringLiteral(
            "QPushButton:enabled { background-color: %1; color: %2; font-weight: bold; padding: 6px 12px; border-radius: 4px; border: 1px solid rgba(0,0,0,0.3); }"
            "QPushButton:disabled { background-color: rgba(128,128,128,0.15); color: palette(disabled-text); font-style: italic; padding: 6px 12px; border-radius: 4px; border: 1px dashed rgba(128,128,128,0.4); }"
        ).arg(hex, textColStr));
    };
    updateColorBtn();
    colorBtn->setEnabled(useColor);

    QObject::connect(colorBtn, &QPushButton::clicked, [&dlg, &currentColor, updateColorBtn, isPolish]() {
        QColorDialog colDlg(currentColor, &dlg);
        colDlg.setOption(QColorDialog::ShowAlphaChannel, true);
        colDlg.setWindowTitle(isPolish ? QStringLiteral("Wybierz kolor oznaczenia") : QObject::tr("Select Mark Color"));
        if (colDlg.exec() == QDialog::Accepted) {
            currentColor = colDlg.selectedColor();
            updateColorBtn();
        }
    });
    formLayout->addRow(chkColor, colorBtn);

    auto *lblLabel = new QLabel(isPolish ? QStringLiteral("Własna etykieta (opcjonalnie):") : QObject::tr("Window label (optional):"), &dlg);
    auto *labelEdit = new QLineEdit(&dlg);
    labelEdit->setText(currentLabel);
    labelEdit->setPlaceholderText(isPolish ? QStringLiteral("np. Backend, Terminal, Projekt A...") : QObject::tr("e.g. Backend, Terminal, Project A..."));
    labelEdit->setClearButtonEnabled(true);
    labelEdit->setMaxLength(50);
    formLayout->addRow(lblLabel, labelEdit);

    auto *chkPersistent = new QCheckBox(isPolish ? QStringLiteral("Zapamiętaj na stałe (przypnij regułę dla aplikacji):") : QObject::tr("Remember mark permanently (pin rule for app):"), &dlg);
    chkPersistent->setChecked(usePersistent);
    formLayout->addRow(chkPersistent);

    layout->addLayout(formLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttonBox);

    const auto updateUI = [chkLetter, letterEdit, chkColor, colorBtn, labelEdit, buttonBox, updateColorBtn, isPolish]() {
        const bool letOn = chkLetter->isChecked();
        const bool colOn = chkColor->isChecked();
        letterEdit->setEnabled(letOn);
        if (letOn) {
            letterEdit->setPlaceholderText(QString());
        } else {
            letterEdit->setPlaceholderText(isPolish ? QStringLiteral("Auto") : QObject::tr("Auto"));
        }
        colorBtn->setEnabled(colOn);
        updateColorBtn();
        auto *okBtn = buttonBox->button(QDialogButtonBox::Ok);
        if (okBtn) {
            okBtn->setEnabled(letOn || colOn || !labelEdit->text().trimmed().isEmpty());
        }
    };

    QObject::connect(chkLetter, &QCheckBox::toggled, &dlg, updateUI);
    QObject::connect(chkColor, &QCheckBox::toggled, &dlg, updateUI);
    QObject::connect(labelEdit, &QLineEdit::textChanged, &dlg, updateUI);
    updateUI();

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        const bool finalUseLetter = chkLetter->isChecked();
        const bool finalUseColor = chkColor->isChecked();
        const bool finalPersistent = chkPersistent->isChecked();

        settings.setValue(QStringLiteral("MarkWithLetter"), finalUseLetter);
        settings.setValue(QStringLiteral("MarkWithColor"), finalUseColor);
        settings.setValue(QStringLiteral("MarkPersistent"), finalPersistent);

        QString finalLetter = QStringLiteral("none");
        if (finalUseLetter) {
            finalLetter = letterEdit->text().trimmed().toUpper();
            if (finalLetter.isEmpty()) {
                finalLetter = currentLetter;
            }
        }

        QString finalColor = QStringLiteral("none");
        if (finalUseColor) {
            finalColor = (currentColor.alpha() < 255) ? currentColor.name(QColor::HexArgb) : currentColor.name(QColor::HexRgb);
        }

        QString finalLabel = labelEdit->text().trimmed();
        if (finalLabel.isEmpty()) {
            finalLabel = QStringLiteral("-");
        }

        std::cout << finalColor.toStdString() << " " << finalLetter.toStdString() << " " << finalLabel.toStdString() << " " << (finalPersistent ? "1" : "0") << std::endl;
        return 0;
    }
    return 1;
}
