/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "keyboardlayouthelper.h"
#include <QSet>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testAllLayoutHomeRowMatrices()
{
    using namespace LeapKKeyboard;

    const Layout layouts[] = {
        Layout::QWERTY,
        Layout::QWERTZ,
        Layout::AZERTY,
        Layout::COLEMAK,
        Layout::DVORAK
    };

    for (Layout l : layouts) {
        const auto matrix = getKeyMatrix(l);
        // Verify all 26 alphabetic characters exist in the keyboard layout matrix
        for (char c = 'A'; c <= 'Z'; ++c) {
            assert(matrix.contains(QChar(c)));
            const KeyCoord coord = matrix.value(QChar(c));
            assert(coord.row >= 1 && coord.row <= 3);
            assert(coord.col >= 0 && coord.col <= 9);
        }
    }

    // Verify home row (row 2) for QWERTY
    const auto qwertyMatrix = getKeyMatrix(Layout::QWERTY);
    const QString qwertyHome = QStringLiteral("ASDFGHJKL");
    for (const QChar &ch : qwertyHome) {
        assert(qwertyMatrix.value(ch).row == 2);
    }

    // Verify home row for AZERTY
    const auto azertyMatrix = getKeyMatrix(Layout::AZERTY);
    const QString azertyHome = QStringLiteral("QSDFGHJKLM");
    for (const QChar &ch : azertyHome) {
        assert(azertyMatrix.value(ch).row == 2);
    }

    // Verify home row for COLEMAK
    const auto colemakMatrix = getKeyMatrix(Layout::COLEMAK);
    const QString colemakHome = QStringLiteral("ARSTDHNEIO");
    for (const QChar &ch : colemakHome) {
        assert(colemakMatrix.value(ch).row == 2);
    }

    // Verify home row for DVORAK
    const auto dvorakMatrix = getKeyMatrix(Layout::DVORAK);
    const QString dvorakHome = QStringLiteral("AOEUIDHTNS");
    for (const QChar &ch : dvorakHome) {
        assert(dvorakMatrix.value(ch).row == 2);
    }

    std::cout << "testAllLayoutHomeRowMatrices PASSED\n";
}

void testHandAssignments()
{
    using namespace LeapKKeyboard;

    const auto matrix = getKeyMatrix(Layout::QWERTY);

    // Left hand keys (col <= 4)
    const QString leftKeys = QStringLiteral("QWERTASDFGZXCVB");
    for (const QChar &ch : leftKeys) {
        assert(matrix.value(ch).isLeftHand == true);
    }

    // Right hand keys (col > 4)
    const QString rightKeys = QStringLiteral("YUIOPHJKLNM");
    for (const QChar &ch : rightKeys) {
        assert(matrix.value(ch).isLeftHand == false);
    }

    std::cout << "testHandAssignments PASSED\n";
}

void testCenteringInvariants()
{
    using namespace LeapKKeyboard;

    // Count = 3 around 'F' on QWERTY: [ D, F, G ]
    {
        QList<QChar> trioF = findSpatialNeighborKeys(QChar('F'), 3, Layout::QWERTY);
        assert(trioF.size() == 3);
        assert(trioF.at(1) == QChar('F')); // 'F' is centered
        assert(trioF.at(0) == QChar('D')); // Left neighbor
        assert(trioF.at(2) == QChar('G')); // Right neighbor
    }

    // Count = 3 around 'J' on QWERTY: [ H, J, K ]
    {
        QList<QChar> trioJ = findSpatialNeighborKeys(QChar('J'), 3, Layout::QWERTY);
        assert(trioJ.size() == 3);
        assert(trioJ.at(1) == QChar('J')); // 'J' is centered
        assert(trioJ.at(0) == QChar('H')); // Left neighbor
        assert(trioJ.at(2) == QChar('K')); // Right neighbor
    }

    // Count = 3 around 'K' on QWERTY: [ J, K, L ]
    {
        QList<QChar> trioK = findSpatialNeighborKeys(QChar('K'), 3, Layout::QWERTY);
        assert(trioK.size() == 3);
        assert(trioK.at(1) == QChar('K'));
        assert(trioK.at(0) == QChar('J'));
        assert(trioK.at(2) == QChar('L'));
    }

    // Count = 3 around 'S' on QWERTY: [ A, S, D ]
    {
        QList<QChar> trioS = findSpatialNeighborKeys(QChar('S'), 3, Layout::QWERTY);
        assert(trioS.size() == 3);
        assert(trioS.at(1) == QChar('S'));
        assert(trioS.at(0) == QChar('A'));
        assert(trioS.at(2) == QChar('D'));
    }

    std::cout << "testCenteringInvariants PASSED\n";
}

void testDisambiguationCountsAndUniqueness()
{
    using namespace LeapKKeyboard;

    const int counts[] = { 1, 2, 3, 4, 5, 8, 10 };
    const QChar testKeys[] = { QChar('A'), QChar('F'), QChar('J'), QChar('Z'), QChar('P') };

    for (QChar base : testKeys) {
        for (int cnt : counts) {
            QList<QChar> result = findSpatialNeighborKeys(base, cnt, Layout::QWERTY);
            assert(result.size() == cnt);

            // Ensure no duplicate keys
            QSet<QChar> uniqueKeys(result.begin(), result.end());
            assert(uniqueKeys.size() == cnt);

            // Ensure baseKey is included
            assert(result.contains(base));

            // Count 1: exact single key
            if (cnt == 1) {
                assert(result.first() == base);
            }
            // Count 2: base key is primary (index 0)
            if (cnt == 2) {
                assert(result.at(0) == base);
            }
        }
    }

    std::cout << "testDisambiguationCountsAndUniqueness PASSED\n";
}

void testLayoutNameParsingVariants()
{
    using namespace LeapKKeyboard;

    assert(parseLayoutName(QStringLiteral("us")) == Layout::QWERTY);
    assert(parseLayoutName(QStringLiteral("pl")) == Layout::QWERTY);
    assert(parseLayoutName(QStringLiteral("de")) == Layout::QWERTZ);
    assert(parseLayoutName(QStringLiteral("at")) == Layout::QWERTZ);
    assert(parseLayoutName(QStringLiteral("ch")) == Layout::QWERTZ);
    assert(parseLayoutName(QStringLiteral("fr")) == Layout::AZERTY);
    assert(parseLayoutName(QStringLiteral("be")) == Layout::AZERTY);
    assert(parseLayoutName(QStringLiteral("colemak")) == Layout::COLEMAK);
    assert(parseLayoutName(QStringLiteral("dvorak")) == Layout::DVORAK);

    // Fallbacks
    assert(parseLayoutName(QStringLiteral("unknown")) == Layout::QWERTY);
    assert(parseLayoutName(QString()) == Layout::QWERTY);

    std::cout << "testLayoutNameParsingVariants PASSED\n";
}

int main()
{
    testAllLayoutHomeRowMatrices();
    testHandAssignments();
    testCenteringInvariants();
    testDisambiguationCountsAndUniqueness();
    testLayoutNameParsingVariants();

    std::cout << "All spatial keyboard tests passed successfully!\n";
    return 0;
}
