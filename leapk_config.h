/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <KCModule>
#include <QColor>
#include <QJsonObject>
#include "customshaderhelper.h"
#include "persistentmarkhelper.h"
#include "themehelper.h"

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
    ~ActiveWindowOutlineConfig() override;

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
    void updateMarkWindowUI();
    void updateBadgePreview();

    void populatePrecedenceList(const QString &serialized);
    QString serializePrecedenceList() const;
    void updatePrecedenceExplanation();
    void updatePrecedenceButtons();
    void movePrecedenceUp();
    void movePrecedenceDown();

    void initPresetsTab();
    void refreshPresetsTable();
    void updatePresetsTableHeight();
    void activateSelectedPreset();
    void createNewPreset();
    void duplicateSelectedPreset();
    void editSelectedPreset();
    void deleteSelectedPreset();
    void exportSelectedPreset();
    void exportPresetByName(const QString &name);
    void importPreset();
    QJsonObject collectCurrentSettings() const;
    void applySettings(const QJsonObject &settings);

    void initShadersSection();
    void refreshShadersTable();
    void toggleSelectedShader();
    void toggleShaderAt(int row);
    void setShaderActive(int row, bool active);
    void createNewShader();
    void editSelectedShader();
    void deleteSelectedShader();
    void exportSelectedShader();
    void importShader();
    void updateShaderParametersUI();
    void updateShaderParametersUIFor(const QString &id);
    void saveShaderParameterValues();

    void initPersistentMarksSection();
    void refreshPersistentMarksTable();
    void deleteSelectedPersistentMark();
    void clearAllPersistentMarks();

    void initThemesTab();
    void refreshThemesTable();
    void applySelectedTheme();
    void importTheme();
    void exportSelectedTheme();
    void resetThemeParameters();
    void updateThemeParametersUI();
    void updateThemeParametersUIFor(const QString &id);
    void saveThemeParameterValues();

    Ui::ActiveWindowOutlineConfigForm *m_ui;
    KConfigSkeleton *m_configSkeleton = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    bool m_isApplyingPreset = false;
    bool m_updatingTable = false;
    QString m_currentDisplayedShaderId;
    QList<LeapKShader::CustomShaderEffect> m_shaders;
    QString m_activeShaderId;
    QList<LeapKMarks::PersistentMarkRule> m_persistentMarks;
    QString m_activeThemeId;
    QString m_currentDisplayedThemeId;
    QList<LeapKTheme::Theme> m_themes;
    QJsonObject m_themeUserOverrides;
};
