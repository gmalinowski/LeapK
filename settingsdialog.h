/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QIcon>
#include <QLocale>
#include <QScreen>
#include <QGuiApplication>
#include <KPluginMetaData>
#include "activewindowoutline_config.h"

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_isPolish(QLocale().language() == QLocale::Polish)
    {
        setWindowTitle(m_isPolish ? QStringLiteral("Active Window Outline — Ustawienia")
                                  : QStringLiteral("Active Window Outline — Settings"));
        setWindowIcon(QIcon::fromTheme(QStringLiteral("active-window-outline"),
                                       QIcon(QStringLiteral(":/icons/active-window-outline.svg"))));
        QScreen *screen = QGuiApplication::primaryScreen();
        const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
        const int targetWidth = qBound(640, 880, avail.width() - 40);
        const int targetHeight = qBound(450, 620, avail.height() - 80);
        resize(targetWidth, targetHeight);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);
        mainLayout->setSpacing(8);

        m_config = new ActiveWindowOutlineConfig(nullptr, KPluginMetaData());
        m_config->load();
        mainLayout->addWidget(m_config->widget());

        auto *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel,
            this
        );
        mainLayout->addWidget(buttonBox);

        connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
            m_config->save();
        });
        connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
            m_config->save();
            accept();
        });
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    ~SettingsDialog() override
    {
        delete m_config;
    }

private:
    bool m_isPolish = false;
    ActiveWindowOutlineConfig *m_config = nullptr;
};
