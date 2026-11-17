#ifndef OUTLINE_FUZZYMATCHER_H
#define OUTLINE_FUZZYMATCHER_H

#include <QString>
#include <QList>
#include <QChar>
#include <algorithm>

namespace OutlineFuzzy {

enum SearchScope {
    CurrentWorkspace = 0,
    AllWorkspaces = 1,
    AllAndStash = 2
};

inline int scoreSubsequence(const QString &pattern, const QString &text)
{
    if (pattern.isEmpty()) {
        return 100;
    }
    const int pLen = pattern.length();
    const int tLen = text.length();
    if (pLen > tLen) {
        return 0;
    }

    const QString pLower = pattern.toLower();
    const QString tLower = text.toLower();

    // Check if pattern is an exact substring first
    const int exactIdx = tLower.indexOf(pLower);
    int exactBonus = 0;
    if (exactIdx >= 0) {
        exactBonus = 70;
        if (exactIdx == 0) {
            exactBonus += 50; // Starts with exact pattern
        } else {
            const QChar prev = text.at(exactIdx - 1);
            if (!prev.isLetterOrNumber()) {
                exactBonus += 35; // Starts at word boundary
            }
        }
    }

    int pIdx = 0;
    int score = 0;
    int consecutive = 0;
    int prevMatchedIdx = -1;

    for (int tIdx = 0; tIdx < tLen && pIdx < pLen; ++tIdx) {
        if (tLower.at(tIdx) == pLower.at(pIdx)) {
            int charScore = 12;

            // Word boundary bonus
            bool isWordStart = false;
            if (tIdx == 0) {
                isWordStart = true;
            } else {
                const QChar prev = text.at(tIdx - 1);
                const QChar curr = text.at(tIdx);
                if (!prev.isLetterOrNumber()) {
                    isWordStart = true;
                } else if (prev.isLower() && curr.isUpper()) {
                    isWordStart = true; // CamelCase boundary
                }
            }

            if (isWordStart) {
                charScore += 30;
            }

            // Consecutive match bonus
            if (prevMatchedIdx == tIdx - 1) {
                consecutive++;
                charScore += consecutive * 18;
            } else {
                consecutive = 0;
            }

            // Distance penalty (prefer earlier matches)
            charScore -= std::min(15, tIdx / 3);

            score += charScore;
            prevMatchedIdx = tIdx;
            pIdx++;
        }
    }

    // If not all pattern characters were matched, no match
    if (pIdx < pLen) {
        return 0;
    }

    // Add length ratio bonus (more compact matches score higher)
    const int totalScore = score + exactBonus + std::max(0, 30 - (tLen - pLen));
    return std::max(1, totalScore);
}

inline int fuzzyMatchScore(const QString &pattern,
                           const QString &caption,
                           const QString &appName = QString(),
                           const QString &customLabel = QString())
{
    const QString cleanPattern = pattern.trimmed();
    if (cleanPattern.isEmpty()) {
        return 100;
    }

    int bestScore = 0;

    // 1. Match against appName
    if (!appName.isEmpty()) {
        const int appScore = scoreSubsequence(cleanPattern, appName);
        if (appScore > 0) {
            bestScore = std::max(bestScore, appScore + 25); // App name bonus
        }
    }

    // 2. Match against caption
    if (!caption.isEmpty()) {
        const int capScore = scoreSubsequence(cleanPattern, caption);
        if (capScore > 0) {
            bestScore = std::max(bestScore, capScore);
        }
    }

    // 3. Match against customLabel
    if (!customLabel.isEmpty()) {
        const int labelScore = scoreSubsequence(cleanPattern, customLabel);
        if (labelScore > 0) {
            bestScore = std::max(bestScore, labelScore + 35); // Custom marker label bonus
        }
    }

    return bestScore;
}

struct SearchCandidate {
    void *windowPtr = nullptr;
    QString caption;
    QString appName;
    QString customLabel;
    int score = 0;
    bool isRecent = false;
    bool isPinned = false;
    bool isPlayingAudio = false;
    int workspaceIndex = 0;
    bool isStashed = false;
};

inline void rankSearchCandidates(const QString &query, QList<SearchCandidate> &candidates)
{
    const QString cleanQuery = query.trimmed();
    const bool hasQuery = !cleanQuery.isEmpty();

    QList<SearchCandidate> filtered;
    filtered.reserve(candidates.size());

    for (auto &cand : candidates) {
        if (hasQuery) {
            cand.score = fuzzyMatchScore(cleanQuery, cand.caption, cand.appName, cand.customLabel);
            if (cand.score <= 0) {
                continue;
            }
        } else {
            cand.score = 100;
        }

        // Tie-breaker bonuses
        if (cand.isRecent) {
            cand.score += 20;
        }
        if (cand.isPinned) {
            cand.score += 15;
        }
        if (cand.isPlayingAudio) {
            cand.score += 10;
        }

        filtered.append(cand);
    }

    std::sort(filtered.begin(), filtered.end(), [](const SearchCandidate &a, const SearchCandidate &b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        if (a.isRecent != b.isRecent) {
            return a.isRecent;
        }
        if (a.isPinned != b.isPinned) {
            return a.isPinned;
        }
        return a.caption < b.caption;
    });

    candidates = filtered;
}

} // namespace OutlineFuzzy

#endif // OUTLINE_FUZZYMATCHER_H
