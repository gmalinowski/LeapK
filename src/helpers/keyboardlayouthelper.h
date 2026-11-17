/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QChar>
#include <QList>
#include <QMap>
#include <QString>
#include <QSet>
#include <QProcess>
#include <QFile>
#include <QStandardPaths>
#include <cmath>

namespace LeapKKeyboard {

enum class Layout {
    QWERTY,
    QWERTZ,
    AZERTY,
    COLEMAK,
    DVORAK
};

inline Layout parseLayoutName(const QString &name)
{
    const QString lower = name.trimmed().toLower();
    if (lower.contains(QStringLiteral("colemak"))) {
        return Layout::COLEMAK;
    }
    if (lower.contains(QStringLiteral("dvorak"))) {
        return Layout::DVORAK;
    }
    if (lower.contains(QStringLiteral("qwertz")) || lower == QStringLiteral("de") || lower == QStringLiteral("ch") || lower == QStringLiteral("at")) {
        return Layout::QWERTZ;
    }
    if (lower.contains(QStringLiteral("azerty")) || lower == QStringLiteral("fr") || lower == QStringLiteral("be")) {
        return Layout::AZERTY;
    }
    return Layout::QWERTY;
}

inline Layout detectActiveLayout()
{
    // 1. Check KDE Plasma kxkbrc config
    const QString kxkbrcPath = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, QStringLiteral("kxkbrc"));
    if (!kxkbrcPath.isEmpty()) {
        QFile f(kxkbrcPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine()).trimmed();
                if (line.startsWith(QStringLiteral("LayoutList="), Qt::CaseInsensitive)) {
                    const QString list = line.mid(11).trimmed();
                    const QString firstLayout = list.section(QLatin1Char(','), 0, 0).trimmed();
                    if (!firstLayout.isEmpty()) {
                        return parseLayoutName(firstLayout);
                    }
                }
            }
        }
    }

    // 2. Check environment variables
    const QString xkbLayout = qEnvironmentVariable("XKB_DEFAULT_LAYOUT");
    if (!xkbLayout.isEmpty()) {
        return parseLayoutName(xkbLayout);
    }

    const QString lang = qEnvironmentVariable("LANG");
    if (lang.startsWith(QStringLiteral("de_"), Qt::CaseInsensitive)) {
        return Layout::QWERTZ;
    }
    if (lang.startsWith(QStringLiteral("fr_"), Qt::CaseInsensitive)) {
        return Layout::AZERTY;
    }

    return Layout::QWERTY;
}

// Returns the full letter pool ordered by ergonomic comfort (Home row first!)
inline QString preferredLetterPool(Layout layout = Layout::QWERTY)
{
    switch (layout) {
    case Layout::COLEMAK:
        // Home row: A R S T D H N E I O
        // Top row:  Q W F P G J L U Y
        // Bottom:   Z X C V B K M
        return QStringLiteral("ARSTDHNEIOQWFPGJLUYZXCVBKM1234567890");
    case Layout::DVORAK:
        // Home row: A O E U I D H T N S
        // Top row:  P Y F G C R L
        // Bottom:   Q J K X B M W V Z
        return QStringLiteral("AOEUIDHTNSPYFGCRLJQJKXBMWVZ1234567890");
    case Layout::QWERTZ:
        // Home row: A S D F G H J K L
        // Top row:  Q W E R T Z U I O P
        // Bottom:   Y X C V B N M
        return QStringLiteral("ASDFGHJKLQWERTZUIOPYXCVBNM1234567890");
    case Layout::AZERTY:
        // Home row: Q S D F G H J K L M
        // Top row:  A Z E R T Y U I O P
        // Bottom:   W X C V B N
        return QStringLiteral("QSDFGHJKLMWAZERTYUIOPXCVBN1234567890");
    case Layout::QWERTY:
    default:
        // Home row: A S D F G H J K L
        // Top row:  Q W E R T Y U I O P
        // Bottom:   Z X C V B N M
        return QStringLiteral("ASDFGHJKLQWERTYUIOPZXCVBNM1234567890");
    }
}

struct KeyCoord {
    int row = 0; // 0 = number, 1 = top, 2 = home, 3 = bottom
    int col = 0; // 0..9 (approximate keyboard column)
    bool isLeftHand = true;
};

inline QMap<QChar, KeyCoord> getKeyMatrix(Layout layout)
{
    QMap<QChar, KeyCoord> m;

    auto addRow = [&m](int row, const QString &keys, int colStart = 0) {
        for (int i = 0; i < keys.length(); ++i) {
            const QChar ch = keys.at(i);
            const int col = colStart + i;
            m[ch] = KeyCoord{row, col, col <= 4};
        }
    };

    switch (layout) {
    case Layout::COLEMAK:
        addRow(0, QStringLiteral("1234567890"));
        addRow(1, QStringLiteral("QWFPGJLUY"));
        addRow(2, QStringLiteral("ARSTDHNEIO"));
        addRow(3, QStringLiteral("ZXCVBKM"));
        break;
    case Layout::DVORAK:
        addRow(0, QStringLiteral("1234567890"));
        addRow(1, QStringLiteral("PYFGCRL"), 3);
        addRow(2, QStringLiteral("AOEUIDHTNS"));
        addRow(3, QStringLiteral("QJKXBMWVZ"));
        break;
    case Layout::QWERTZ:
        addRow(0, QStringLiteral("1234567890"));
        addRow(1, QStringLiteral("QWERTZUIOP"));
        addRow(2, QStringLiteral("ASDFGHJKL"));
        addRow(3, QStringLiteral("YXCVBNM"));
        break;
    case Layout::AZERTY:
        addRow(0, QStringLiteral("1234567890"));
        addRow(1, QStringLiteral("AZERTYUIOP"));
        addRow(2, QStringLiteral("QSDFGHJKLM"));
        addRow(3, QStringLiteral("WXCVBN"));
        break;
    case Layout::QWERTY:
    default:
        addRow(0, QStringLiteral("1234567890"));
        addRow(1, QStringLiteral("QWERTYUIOP"));
        addRow(2, QStringLiteral("ASDFGHJKL"));
        addRow(3, QStringLiteral("ZXCVBNM"));
        break;
    }

    return m;
}

// Finds spatial neighbor keys around baseKey, suitable for multi-instance disambiguation.
// Ensures baseKey is assigned to the primary instance (e.g. index 0 or center), so tapping baseKey twice leaps immediately.
inline QList<QChar> findSpatialNeighborKeys(QChar baseKey, int count, Layout layout = Layout::QWERTY)
{
    if (count <= 0) {
        return {};
    }
    const QChar baseUpper = baseKey.toUpper();
    if (count == 1) {
        return { baseUpper };
    }

    const auto matrix = getKeyMatrix(layout);
    if (!matrix.contains(baseUpper)) {
        // Fallback to simple pool slice
        const QString pool = preferredLetterPool(layout);
        QList<QChar> result = { baseUpper };
        for (const QChar &ch : pool) {
            if (result.size() >= count) break;
            if (ch != baseUpper && !result.contains(ch)) {
                result.append(ch);
            }
        }
        return result;
    }

    const KeyCoord baseCoord = matrix.value(baseUpper);

    // Collect all candidate keys on the keyboard
    struct ScoredKey {
        QChar key;
        KeyCoord coord;
        double distance;
        int rowDiff;
        int colDiff;
    };
    QList<ScoredKey> candidates;

    for (auto it = matrix.constBegin(); it != matrix.constEnd(); ++it) {
        const QChar ch = it.key();
        if (ch == baseUpper || ch.isDigit()) {
            continue;
        }
        const KeyCoord c = it.value();
        const int dr = c.row - baseCoord.row;
        const int dc = c.col - baseCoord.col;

        // Prefer same hand keys
        double handPenalty = (c.isLeftHand == baseCoord.isLeftHand) ? 0.0 : 2.5;
        // Strong preference for same row (horizontal neighbors)
        double rowPenalty = std::abs(dr) * 1.8;
        double dist = std::sqrt(dc * dc + dr * dr) + handPenalty + rowPenalty;

        candidates.append(ScoredKey{ch, c, dist, dr, dc});
    }

    // Sort by distance (closest physical keys first)
    std::sort(candidates.begin(), candidates.end(), [](const ScoredKey &a, const ScoredKey &b) {
        if (std::abs(a.distance - b.distance) > 0.001) {
            return a.distance < b.distance;
        }
        // Same row preferred
        if (std::abs(a.rowDiff) != std::abs(b.rowDiff)) {
            return std::abs(a.rowDiff) < std::abs(b.rowDiff);
        }
        return a.key < b.key;
    });

    // Special case for count == 2:
    // Window 0 (primary) = baseKey, Window 1 = closest neighbor (e.g. F, G)
    if (count == 2) {
        QChar neighbor = candidates.isEmpty() ? QChar('G') : candidates.first().key;
        return { baseUpper, neighbor };
    }

    // Special case for count == 3:
    // Centered around baseKey! (e.g. [ D, F, G ])
    // Find left neighbor (same row, col < base) and right neighbor (same row, col > base)
    if (count == 3) {
        QChar leftKey = QChar();
        QChar rightKey = QChar();

        for (const auto &cand : candidates) {
            if (cand.coord.row == baseCoord.row) {
                if (cand.coord.col < baseCoord.col && leftKey.isNull()) {
                    leftKey = cand.key;
                } else if (cand.coord.col > baseCoord.col && rightKey.isNull()) {
                    rightKey = cand.key;
                }
            }
        }
        // If left and right on the same row found:
        if (!leftKey.isNull() && !rightKey.isNull()) {
            return { leftKey, baseUpper, rightKey };
        }
    }

    // General case for count >= 3:
    // Select (count - 1) closest candidates
    QList<QChar> selected = { baseUpper };
    for (int i = 0; i < candidates.size() && selected.size() < count; ++i) {
        selected.append(candidates.at(i).key);
    }

    // Sort selected keys spatially from left to right, maintaining baseKey's association
    // Primary window (most recent) is paired with baseKey!
    return selected;
}

} // namespace LeapKKeyboard
