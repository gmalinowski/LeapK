/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "persistentmarkhelper.h"
#include <QTemporaryDir>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testScoringHierarchy()
{
    using namespace LeapKMarks;

    PersistentMarkRule ruleAppOnly;
    ruleAppOnly.id = QStringLiteral("r1");
    ruleAppOnly.letter = QChar('F');
    ruleAppOnly.matchApp = QStringLiteral("firefox");

    PersistentMarkRule ruleAppDesktop;
    ruleAppDesktop.id = QStringLiteral("r2");
    ruleAppDesktop.letter = QChar('D');
    ruleAppDesktop.matchApp = QStringLiteral("firefox");
    ruleAppDesktop.matchDesktop = 2;

    PersistentMarkRule ruleAppCaption;
    ruleAppCaption.id = QStringLiteral("r3");
    ruleAppCaption.letter = QChar('C');
    ruleAppCaption.matchApp = QStringLiteral("firefox");
    ruleAppCaption.matchCaption = QStringLiteral("GitHub");

    PersistentMarkRule ruleAppCaptionDesktop;
    ruleAppCaptionDesktop.id = QStringLiteral("r4");
    ruleAppCaptionDesktop.letter = QChar('A');
    ruleAppCaptionDesktop.matchApp = QStringLiteral("firefox");
    ruleAppCaptionDesktop.matchCaption = QStringLiteral("GitHub");
    ruleAppCaptionDesktop.matchDesktop = 2;

    // 1. App-only score: 10
    assert(scoreWindowMatch(ruleAppOnly, QStringLiteral("firefox"), QStringLiteral("General Title"), 1) == 10);

    // 2. App + Desktop match score: 10 + 20 = 30
    assert(scoreWindowMatch(ruleAppDesktop, QStringLiteral("firefox"), QStringLiteral("General Title"), 2) == 30);

    // 3. App + Caption match score: 10 + 50 = 60
    assert(scoreWindowMatch(ruleAppCaption, QStringLiteral("firefox"), QStringLiteral("GitHub - Pull Requests"), 1) == 60);

    // 4. App + Caption + Desktop match score: 10 + 20 + 50 = 80
    assert(scoreWindowMatch(ruleAppCaptionDesktop, QStringLiteral("firefox"), QStringLiteral("GitHub - Pull Requests"), 2) == 80);

    std::cout << "testScoringHierarchy PASSED\n";
}

void testMismatchesReturnZero()
{
    using namespace LeapKMarks;

    PersistentMarkRule rule;
    rule.id = QStringLiteral("r");
    rule.letter = QChar('K');
    rule.matchApp = QStringLiteral("org.kde.konsole");
    rule.matchCaption = QStringLiteral("nvim");
    rule.matchDesktop = 1;

    // 1. App mismatch -> 0
    assert(scoreWindowMatch(rule, QStringLiteral("firefox"), QStringLiteral("nvim"), 1) == 0);

    // 2. Caption mismatch -> 0
    assert(scoreWindowMatch(rule, QStringLiteral("org.kde.konsole"), QStringLiteral("bash"), 1) == 0);

    // 3. Desktop mismatch -> 0
    assert(scoreWindowMatch(rule, QStringLiteral("org.kde.konsole"), QStringLiteral("nvim"), 2) == 0);

    // 4. Disabled rule -> 0 even on full match
    rule.enabled = false;
    assert(scoreWindowMatch(rule, QStringLiteral("org.kde.konsole"), QStringLiteral("nvim"), 1) == 0);

    std::cout << "testMismatchesReturnZero PASSED\n";
}

void testBestMatchingRuleSelection()
{
    using namespace LeapKMarks;

    PersistentMarkRule rGeneral;
    rGeneral.id = QStringLiteral("r-general");
    rGeneral.letter = QChar('G');
    rGeneral.matchApp = QStringLiteral("firefox");

    PersistentMarkRule rDesktop2;
    rDesktop2.id = QStringLiteral("r-d2");
    rDesktop2.letter = QChar('D');
    rDesktop2.matchApp = QStringLiteral("firefox");
    rDesktop2.matchDesktop = 2;

    PersistentMarkRule rGitHub;
    rGitHub.id = QStringLiteral("r-github");
    rGitHub.letter = QChar('H');
    rGitHub.matchApp = QStringLiteral("firefox");
    rGitHub.matchCaption = QStringLiteral("GitHub");

    PersistentMarkRule rGitHubD2;
    rGitHubD2.id = QStringLiteral("r-github-d2");
    rGitHubD2.letter = QChar('P');
    rGitHubD2.matchApp = QStringLiteral("firefox");
    rGitHubD2.matchCaption = QStringLiteral("GitHub");
    rGitHubD2.matchDesktop = 2;

    QList<PersistentMarkRule> rules = { rGeneral, rDesktop2, rGitHub, rGitHubD2 };

    // 1. General window on Desktop 1 -> rGeneral
    const auto *b1 = findBestMatchingRule(rules, QStringLiteral("firefox"), QStringLiteral("Wikipedia"), 1);
    assert(b1 != nullptr && b1->id == rGeneral.id);

    // 2. Window on Desktop 2 -> rDesktop2 (score 30 > 10)
    const auto *b2 = findBestMatchingRule(rules, QStringLiteral("firefox"), QStringLiteral("Wikipedia"), 2);
    assert(b2 != nullptr && b2->id == rDesktop2.id);

    // 3. GitHub window on Desktop 1 -> rGitHub (score 60 > 10)
    const auto *b3 = findBestMatchingRule(rules, QStringLiteral("firefox"), QStringLiteral("GitHub - LeapK"), 1);
    assert(b3 != nullptr && b3->id == rGitHub.id);

    // 4. GitHub window on Desktop 2 -> rGitHubD2 (score 80 > 60 > 30 > 10)
    const auto *b4 = findBestMatchingRule(rules, QStringLiteral("firefox"), QStringLiteral("GitHub - LeapK"), 2);
    assert(b4 != nullptr && b4->id == rGitHubD2.id);

    // 5. Unrelated app -> nullptr
    const auto *b5 = findBestMatchingRule(rules, QStringLiteral("alacritty"), QStringLiteral("Terminal"), 2);
    assert(b5 == nullptr);

    std::cout << "testBestMatchingRuleSelection PASSED\n";
}

void testUpsertCollisionAndReplacement()
{
    using namespace LeapKMarks;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/marks.json");

    PersistentMarkRule r1;
    r1.id = QStringLiteral("rule-1");
    r1.letter = QChar('A');
    r1.matchApp = QStringLiteral("konsole");

    QList<PersistentMarkRule> rules;
    upsertPersistentMark(rules, r1, filePath);
    assert(rules.size() == 1);
    assert(rules.first().letter == QChar('A'));

    // Updating existing rule replaces in place
    r1.letter = QChar('Z');
    upsertPersistentMark(rules, r1, filePath);
    assert(rules.size() == 1);
    assert(rules.first().letter == QChar('Z'));

    // Inserting new rule appends
    PersistentMarkRule r2;
    r2.id = QStringLiteral("rule-2");
    r2.letter = QChar('B');
    r2.matchApp = QStringLiteral("code");
    upsertPersistentMark(rules, r2, filePath);
    assert(rules.size() == 2);

    // Reload from file to ensure disk persistence
    QList<PersistentMarkRule> reloaded = loadPersistentMarks(filePath);
    assert(reloaded.size() == 2);
    assert(reloaded.at(0).letter == QChar('Z'));
    assert(reloaded.at(1).letter == QChar('B'));

    std::cout << "testUpsertCollisionAndReplacement PASSED\n";
}

int main()
{
    testScoringHierarchy();
    testMismatchesReturnZero();
    testBestMatchingRuleSelection();
    testUpsertCollisionAndReplacement();

    std::cout << "All mark scoring tests passed successfully!\n";
    return 0;
}
