/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "persistentmarkhelper.h"
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

void testRuleSerialization()
{
    using namespace LeapKMarks;

    PersistentMarkRule rule;
    rule.id = QStringLiteral("test-id-123");
    rule.letter = QChar('F');
    rule.color = QColor(QStringLiteral("#3498db"));
    rule.label = QStringLiteral("Browser");
    rule.matchApp = QStringLiteral("firefox");
    rule.matchCaption = QStringLiteral("Project");
    rule.matchDesktop = 2;
    rule.enabled = true;
    rule.createdAt = 1234567890;

    QJsonObject json = ruleToJson(rule);
    PersistentMarkRule restored = ruleFromJson(json);

    assert(restored.id == rule.id);
    assert(restored.letter == rule.letter);
    assert(restored.color == rule.color);
    assert(restored.label == rule.label);
    assert(restored.matchApp == rule.matchApp);
    assert(restored.matchCaption == rule.matchCaption);
    assert(restored.matchDesktop == rule.matchDesktop);
    assert(restored.enabled == rule.enabled);
    assert(restored.createdAt == rule.createdAt);

    std::cout << "testRuleSerialization PASSED\n";
}

void testSaveAndLoadMarks()
{
    using namespace LeapKMarks;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/marks.json");

    PersistentMarkRule r1;
    r1.id = QStringLiteral("rule-1");
    r1.letter = QChar('T');
    r1.color = QColor(QStringLiteral("#e74c3c"));
    r1.matchApp = QStringLiteral("org.kde.konsole");

    PersistentMarkRule r2;
    r2.id = QStringLiteral("rule-2");
    r2.letter = QChar('F');
    r2.color = QColor(QStringLiteral("#2ecc71"));
    r2.matchApp = QStringLiteral("firefox");

    QList<PersistentMarkRule> rules = { r1, r2 };
    bool saved = savePersistentMarks(rules, filePath);
    assert(saved);

    QList<PersistentMarkRule> loaded = loadPersistentMarks(filePath);
    assert(loaded.size() == 2);
    assert(loaded.at(0).letter == QChar('T'));
    assert(loaded.at(1).letter == QChar('F'));

    std::cout << "testSaveAndLoadMarks PASSED\n";
}

void testScoreWindowMatch()
{
    using namespace LeapKMarks;

    PersistentMarkRule ruleApp;
    ruleApp.id = QStringLiteral("app-rule");
    ruleApp.letter = QChar('F');
    ruleApp.color = QColor(QStringLiteral("#e74c3c"));
    ruleApp.matchApp = QStringLiteral("firefox");

    PersistentMarkRule ruleSpecific;
    ruleSpecific.id = QStringLiteral("specific-rule");
    ruleSpecific.letter = QChar('W');
    ruleSpecific.color = QColor(QStringLiteral("#3498db"));
    ruleSpecific.matchApp = QStringLiteral("firefox");
    ruleSpecific.matchCaption = QStringLiteral("Work");
    ruleSpecific.matchDesktop = 2;

    QList<PersistentMarkRule> rules = { ruleApp, ruleSpecific };

    // 1. Regular Firefox on desktop 1
    const auto *best1 = findBestMatchingRule(rules, QStringLiteral("firefox"), QStringLiteral("General Title"), 1);
    assert(best1 != nullptr);
    assert(best1->id == ruleApp.id);
    assert(best1->letter == QChar('F'));

    // 2. Specific Firefox window with "Work" on desktop 2
    const auto *best2 = findBestMatchingRule(rules, QStringLiteral("firefox"), QStringLiteral("Work - Task List"), 2);
    assert(best2 != nullptr);
    assert(best2->id == ruleSpecific.id);
    assert(best2->letter == QChar('W'));

    // 3. Unrelated application
    const auto *best3 = findBestMatchingRule(rules, QStringLiteral("org.kde.konsole"), QStringLiteral("Shell"), 1);
    assert(best3 == nullptr);

    std::cout << "testScoreWindowMatch PASSED\n";
}

int main()
{
    testRuleSerialization();
    testSaveAndLoadMarks();
    testScoreWindowMatch();
    std::cout << "All persistent mark tests passed successfully!\n";
    return 0;
}
