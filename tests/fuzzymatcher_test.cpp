#include "fuzzymatcher.h"
#include <QCoreApplication>
#include <cassert>
#include <iostream>

void testFuzzyMatchingBasic()
{
    std::cout << "[TEST] Running testFuzzyMatchingBasic..." << std::endl;

    // 1. Empty pattern matches with baseline score
    int scoreEmpty = OutlineFuzzy::fuzzyMatchScore("", "Konsole", "konsole");
    assert(scoreEmpty == 100);

    // 2. Exact match on app name
    int scoreExact = OutlineFuzzy::fuzzyMatchScore("konsole", "Terminal - bash", "konsole");
    assert(scoreExact > 100);

    // 3. Prefix match on caption
    int scorePrefix = OutlineFuzzy::fuzzyMatchScore("fire", "Firefox Web Browser", "firefox");
    assert(scorePrefix > 80);

    // 4. Subsequence / Acronym match on caption
    int scoreAcronym = OutlineFuzzy::fuzzyMatchScore("vsc", "Visual Studio Code", "code");
    assert(scoreAcronym > 50);

    // 5. Non-match returns 0
    int scoreNoMatch = OutlineFuzzy::fuzzyMatchScore("xyz", "Google Chrome", "google-chrome");
    assert(scoreNoMatch == 0);

    // 6. Pattern longer than target returns 0
    int scoreTooLong = OutlineFuzzy::fuzzyMatchScore("reallylongpatternthatwillnotmatch", "cat", "cat");
    assert(scoreTooLong == 0);

    std::cout << "[PASS] testFuzzyMatchingBasic passed." << std::endl;
}

void testFuzzyRanking()
{
    std::cout << "[TEST] Running testFuzzyRanking..." << std::endl;

    QList<OutlineFuzzy::SearchCandidate> list = {
        {reinterpret_cast<void*>(1), "CMakeLists.txt - Kate", "kate", "", 0, false, false, false, 0, false},
        {reinterpret_cast<void*>(2), "Pull Request #42 · GitHub - Firefox", "firefox", "", 0, true, false, false, 0, false},
        {reinterpret_cast<void*>(3), "bash - Konsole", "konsole", "", 0, false, false, false, 0, false},
        {reinterpret_cast<void*>(4), "Bug Report - Firefox", "firefox", "", 0, false, false, false, 0, false}
    };

    // Query "fire" should rank Firefox windows at the top
    QList<OutlineFuzzy::SearchCandidate> fireMatches = list;
    OutlineFuzzy::rankSearchCandidates("fire", fireMatches);

    assert(fireMatches.size() == 2);
    // Recent Firefox window should rank #1 due to recency bonus
    assert(fireMatches.at(0).caption.contains("Pull Request"));
    assert(fireMatches.at(1).caption.contains("Bug Report"));

    // Query "kate" should match only Kate
    QList<OutlineFuzzy::SearchCandidate> kateMatches = list;
    OutlineFuzzy::rankSearchCandidates("kate", kateMatches);
    assert(kateMatches.size() == 1);
    assert(kateMatches.at(0).appName == "kate");

    // Empty query returns all candidates sorted with recent first
    QList<OutlineFuzzy::SearchCandidate> allMatches = list;
    OutlineFuzzy::rankSearchCandidates("", allMatches);
    assert(allMatches.size() == 4);
    assert(allMatches.at(0).isRecent == true);

    std::cout << "[PASS] testFuzzyRanking passed." << std::endl;
}

void testCustomLabelMatching()
{
    std::cout << "[TEST] Running testCustomLabelMatching..." << std::endl;

    QList<OutlineFuzzy::SearchCandidate> list = {
        {reinterpret_cast<void*>(1), "Window 1", "unknown", "Server Monitor", 0, false, false, false, 0, false},
        {reinterpret_cast<void*>(2), "Window 2", "unknown", "Database GUI", 0, false, false, false, 0, false}
    };

    QList<OutlineFuzzy::SearchCandidate> dbMatches = list;
    OutlineFuzzy::rankSearchCandidates("data", dbMatches);

    assert(dbMatches.size() == 1);
    assert(dbMatches.at(0).customLabel == "Database GUI");

    std::cout << "[PASS] testCustomLabelMatching passed." << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testFuzzyMatchingBasic();
    testFuzzyRanking();
    testCustomLabelMatching();

    std::cout << "\nALL FUZZY MATCHER TESTS PASSED (100%)\n" << std::endl;
    return 0;
}
