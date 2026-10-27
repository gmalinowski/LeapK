/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QColor>
#include <QChar>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <QDateTime>

namespace LeapKMarks {

struct PersistentMarkRule {
    QString id;
    QChar letter;
    QColor color;
    QString label;
    QString matchApp;        // App class or desktop filename, e.g. "firefox", "org.kde.konsole"
    QString matchCaption;    // Substring or pattern in window title, optional
    int matchDesktop = 0;    // 0 = any desktop, >0 = specific virtual desktop
    bool enabled = true;
    qint64 createdAt = 0;
};

inline QJsonObject ruleToJson(const PersistentMarkRule &rule)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = rule.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : rule.id;
    obj[QStringLiteral("letter")] = QString(rule.letter);
    obj[QStringLiteral("color")] = rule.color.isValid() ? ((rule.color.alpha() < 255) ? rule.color.name(QColor::HexArgb) : rule.color.name(QColor::HexRgb)) : QStringLiteral("#e74c3c");
    obj[QStringLiteral("label")] = rule.label;
    obj[QStringLiteral("matchApp")] = rule.matchApp;
    obj[QStringLiteral("matchCaption")] = rule.matchCaption;
    obj[QStringLiteral("matchDesktop")] = rule.matchDesktop;
    obj[QStringLiteral("enabled")] = rule.enabled;
    obj[QStringLiteral("createdAt")] = rule.createdAt > 0 ? rule.createdAt : QDateTime::currentMSecsSinceEpoch();
    return obj;
}

inline PersistentMarkRule ruleFromJson(const QJsonObject &obj)
{
    PersistentMarkRule rule;
    rule.id = obj.value(QStringLiteral("id")).toString();
    const QString letStr = obj.value(QStringLiteral("letter")).toString().trimmed();
    rule.letter = letStr.isEmpty() ? QChar() : letStr.at(0).toUpper();
    rule.color = QColor(obj.value(QStringLiteral("color")).toString(QStringLiteral("#e74c3c")));
    rule.label = obj.value(QStringLiteral("label")).toString();
    rule.matchApp = obj.value(QStringLiteral("matchApp")).toString();
    rule.matchCaption = obj.value(QStringLiteral("matchCaption")).toString();
    rule.matchDesktop = obj.value(QStringLiteral("matchDesktop")).toInt(0);
    rule.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    rule.createdAt = obj.value(QStringLiteral("createdAt")).toVariant().toLongLong();
    return rule;
}

inline QString defaultMarksDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString dir = base + QStringLiteral("/leapk");
    QDir().mkpath(dir);
    return dir;
}

inline QString defaultMarksFilePath()
{
    const QString primary = defaultMarksDirectory() + QStringLiteral("/marks.json");
    if (QFile::exists(primary)) {
        return primary;
    }
    // Migration fallback from old kwin-active-window-outline directory
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString legacy = base + QStringLiteral("/kwin-active-window-outline/marks.json");
    if (QFile::exists(legacy)) {
        // Copy over to new location
        QFile::copy(legacy, primary);
        return primary;
    }
    return primary;
}

inline QList<PersistentMarkRule> loadPersistentMarks(const QString &filePath = QString())
{
    const QString path = filePath.isEmpty() ? defaultMarksFilePath() : filePath;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return {};
    }

    QList<PersistentMarkRule> list;
    const QJsonArray arr = doc.array();
    for (const auto &val : arr) {
        if (val.isObject()) {
            list.append(ruleFromJson(val.toObject()));
        }
    }
    return list;
}

inline bool savePersistentMarks(const QList<PersistentMarkRule> &rules, const QString &filePath = QString())
{
    const QString path = filePath.isEmpty() ? defaultMarksFilePath() : filePath;
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QJsonArray arr;
    for (const auto &rule : rules) {
        arr.append(ruleToJson(rule));
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return true;
}

// Computes a match score for an open window against a persistent rule.
// Returns 0 if window doesn't match the rule, or a positive score (higher = better specific match).
inline int scoreWindowMatch(const PersistentMarkRule &rule, const QString &appId, const QString &caption, int desktop)
{
    if (!rule.enabled) {
        return 0;
    }

    // 1. App matching: rule.matchApp must match window's appId / windowClass
    if (!rule.matchApp.isEmpty()) {
        const QString cleanRuleApp = rule.matchApp.trimmed().toLower();
        const QString cleanApp = appId.trimmed().toLower();
        if (!cleanApp.contains(cleanRuleApp) && !cleanRuleApp.contains(cleanApp)) {
            return 0;
        }
    }

    int score = 10; // Base score for matching application

    // 2. Desktop matching: if rule specifies a desktop, must match
    if (rule.matchDesktop > 0) {
        if (desktop > 0 && desktop != rule.matchDesktop) {
            return 0; // Mismatched desktop
        }
        if (desktop == rule.matchDesktop) {
            score += 20; // Specific desktop match bonus
        }
    }

    // 3. Caption / title pattern matching
    if (!rule.matchCaption.trimmed().isEmpty()) {
        const QString pattern = rule.matchCaption.trimmed();
        if (!caption.contains(pattern, Qt::CaseInsensitive)) {
            return 0; // Caption requirement not met
        }
        score += 50; // High score for matching specific window title/project!
    }

    return score;
}

// Finds the best matching persistent rule for a given window among a list of rules
inline const PersistentMarkRule *findBestMatchingRule(const QList<PersistentMarkRule> &rules, const QString &appId, const QString &caption, int desktop)
{
    const PersistentMarkRule *best = nullptr;
    int highestScore = 0;

    for (const auto &rule : rules) {
        const int s = scoreWindowMatch(rule, appId, caption, desktop);
        if (s > highestScore) {
            highestScore = s;
            best = &rule;
        }
    }

    return best;
}

} // namespace LeapKMarks
