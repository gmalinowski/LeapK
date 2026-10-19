/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "activewindowoutline_config.h"
#include "ui_activewindowoutline_config.h"

#include <KColorScheme>
#include <KConfigSkeleton>
#include <KPluginFactory>
#include <QDBusConnection>
#include <QDBusMessage>

#ifndef ACTIVE_WINDOW_OUTLINE_VERSION
#define ACTIVE_WINDOW_OUTLINE_VERSION "development"
#endif

K_PLUGIN_CLASS(ActiveWindowOutlineConfig)

ActiveWindowOutlineConfig::ActiveWindowOutlineConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
    , m_ui(new Ui::ActiveWindowOutlineConfigForm)
{
    m_ui->setupUi(widget());
    m_ui->label_versionValue->setText(QStringLiteral(ACTIVE_WINDOW_OUTLINE_VERSION));

    QPalette infoPalette = m_ui->label_info->palette();
    const QColor neutralText = KColorScheme(QPalette::Active, KColorScheme::Window)
                                   .foreground(KColorScheme::NeutralText)
                                   .color();
    infoPalette.setColor(QPalette::WindowText, neutralText);
    m_ui->label_info->setPalette(infoPalette);

    auto *config = new KConfigSkeleton(QStringLiteral("kwinrc"), this);
    config->setCurrentGroup(QStringLiteral("Effect-active-window-outline"));

    config->addItemBool(QStringLiteral("UseAccentColor"), *new bool, true);
    config->addItemInt(QStringLiteral("AccentOpacity"), *new int, 66);
    config->addItemColor(QStringLiteral("CustomColor"), *new QColor, QColor(61, 174, 233));
    config->addItemInt(QStringLiteral("BorderWidth"), *new int, 4);
    config->addItemInt(QStringLiteral("OutlineGap"), *new int, 0);
    config->addItemInt(QStringLiteral("BorderRadius"), *new int, 5);
    config->addItemBool(QStringLiteral("DrawInside"), *new bool, false);
    config->addItemBool(QStringLiteral("SmoothEdges"), *new bool, true);
    config->addItemBool(QStringLiteral("GlowEnabled"), *new bool, true);
    config->addItemInt(QStringLiteral("GlowRadius"), *new int, 6);

    addConfig(config, widget());

    // Keep the opacity control with the accent color and the alpha-enabled
    // custom picker with the custom color, so each mode has one clear control.
    auto updateColorEnabled = [this] {
        const bool useAccent = m_ui->kcfg_UseAccentColor->isChecked();
        m_ui->kcfg_AccentOpacity->setEnabled(useAccent);
        m_ui->label_accentOpacity->setEnabled(useAccent);
        m_ui->kcfg_CustomColor->setEnabled(!useAccent);
        m_ui->label_customColor->setEnabled(!useAccent);
    };
    connect(m_ui->kcfg_UseAccentColor, &QCheckBox::toggled, this, updateColorEnabled);
    updateColorEnabled();

    const auto updateGlowEnabled = [this] {
        const bool enabled = m_ui->kcfg_GlowEnabled->isChecked();
        m_ui->kcfg_GlowRadius->setEnabled(enabled);
        m_ui->label_glowRadius->setEnabled(enabled);
    };
    connect(m_ui->kcfg_GlowEnabled, &QCheckBox::toggled, this, updateGlowEnabled);
    updateGlowEnabled();
}

void ActiveWindowOutlineConfig::save()
{
    KCModule::save();

    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("reconfigureEffect"));
    message << QStringLiteral("active-window-outline");
    // Wait until KWin has actually processed the reconfigure request. This
    // avoids closing the KCM while an unchecked fire-and-forget call is still
    // queued. The effect also watches kwinrc as a compatibility fallback.
    QDBusConnection::sessionBus().call(message, QDBus::Block, 2000);
}

#include "activewindowoutline_config.moc"
