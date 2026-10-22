/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <KCModule>
#include <QColor>

namespace Ui
{
class ActiveWindowOutlineConfigForm;
}

class KConfigSkeleton;
class QNetworkAccessManager;

class ActiveWindowOutlineConfig : public KCModule
{
    Q_OBJECT

public:
    explicit ActiveWindowOutlineConfig(QObject *parent, const KPluginMetaData &data);

    void save() override;
    void load() override;
    void defaults() override;

private:
    void populateColorTable(const QString &serialized);
    QString serializeColorTable() const;
    void addColorRow(const QColor &color, double holdTime);

    void updatePresetSelector();
    void showAmbilightHelpDialog();
    void checkForUpdates();
    void updateRecentWindowUI();

    Ui::ActiveWindowOutlineConfigForm *m_ui;
    KConfigSkeleton *m_configSkeleton = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    bool m_isApplyingPreset = false;
};
