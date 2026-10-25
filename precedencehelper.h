/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QString>
#include <QStringList>

namespace OutlinePrecedence
{

enum class ResolvedStyle {
    Active,
    Marked,
    Recent,
    None
};

inline QStringList parsePrecedence(const QString &serialized)
{
    const QStringList parsed = serialized.split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList result;
    for (const QString &item : parsed) {
        const QString trimmed = item.trimmed();
        if (trimmed.compare(QStringLiteral("Active"), Qt::CaseInsensitive) == 0 && !result.contains(QStringLiteral("Active"))) {
            result.append(QStringLiteral("Active"));
        } else if (trimmed.compare(QStringLiteral("Marked"), Qt::CaseInsensitive) == 0 && !result.contains(QStringLiteral("Marked"))) {
            result.append(QStringLiteral("Marked"));
        } else if (trimmed.compare(QStringLiteral("Recent"), Qt::CaseInsensitive) == 0 && !result.contains(QStringLiteral("Recent"))) {
            result.append(QStringLiteral("Recent"));
        }
    }
    const QStringList allCategories = {QStringLiteral("Marked"), QStringLiteral("Active"), QStringLiteral("Recent")};
    for (const QString &cat : allCategories) {
        if (!result.contains(cat)) {
            result.append(cat);
        }
    }
    return result;
}

inline ResolvedStyle resolveStyle(bool isFocus, bool isMarked, bool isRecentInHistory, const QStringList &precedence)
{
    int rankMarked = precedence.indexOf(QStringLiteral("Marked"));
    int rankActive = precedence.indexOf(QStringLiteral("Active"));
    int rankRecent = precedence.indexOf(QStringLiteral("Recent"));
    if (rankMarked < 0) rankMarked = 0;
    if (rankActive < 0) rankActive = 1;
    if (rankRecent < 0) rankRecent = 2;

    const bool activeOverridesMark = isFocus && (rankActive < rankMarked);
    const bool recentOverridesMark = isRecentInHistory && (rankRecent < rankMarked);

    const bool treatAsMarked = isMarked && !activeOverridesMark && !recentOverridesMark;
    const bool treatAsRecent = isRecentInHistory && (!isMarked || recentOverridesMark);

    if (isFocus) {
        return treatAsMarked ? ResolvedStyle::Marked : ResolvedStyle::Active;
    }
    if (treatAsMarked) {
        return ResolvedStyle::Marked;
    }
    if (treatAsRecent) {
        return ResolvedStyle::Recent;
    }
    return ResolvedStyle::None;
}

} // namespace OutlinePrecedence
