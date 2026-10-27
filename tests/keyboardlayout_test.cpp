/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "keyboardlayouthelper.h"
#include <cassert>
#include <iostream>

void testLayoutParsing()
{
    using namespace LeapKKeyboard;
    assert(parseLayoutName(QStringLiteral("us")) == Layout::QWERTY);
    assert(parseLayoutName(QStringLiteral("pl")) == Layout::QWERTY);
    assert(parseLayoutName(QStringLiteral("de")) == Layout::QWERTZ);
    assert(parseLayoutName(QStringLiteral("qwertz")) == Layout::QWERTZ);
    assert(parseLayoutName(QStringLiteral("fr")) == Layout::AZERTY);
    assert(parseLayoutName(QStringLiteral("azerty")) == Layout::AZERTY);
    assert(parseLayoutName(QStringLiteral("colemak")) == Layout::COLEMAK);
    assert(parseLayoutName(QStringLiteral("dvorak")) == Layout::DVORAK);
    std::cout << "testLayoutParsing PASSED\n";
}

void testPreferredLetterPool()
{
    using namespace LeapKKeyboard;
    QString qwertyPool = preferredLetterPool(Layout::QWERTY);
    assert(qwertyPool.startsWith(QStringLiteral("ASDFGHJKL")));

    QString qwertzPool = preferredLetterPool(Layout::QWERTZ);
    assert(qwertzPool.startsWith(QStringLiteral("ASDFGHJKL")));

    QString azertyPool = preferredLetterPool(Layout::AZERTY);
    assert(azertyPool.startsWith(QStringLiteral("QSDFGHJKLM")));

    QString colemakPool = preferredLetterPool(Layout::COLEMAK);
    assert(colemakPool.startsWith(QStringLiteral("ARSTDHNEIO")));

    QString dvorakPool = preferredLetterPool(Layout::DVORAK);
    assert(dvorakPool.startsWith(QStringLiteral("AOEUIDHTNS")));

    std::cout << "testPreferredLetterPool PASSED\n";
}

void testSpatialNeighborKeys()
{
    using namespace LeapKKeyboard;

    // Count = 1
    QList<QChar> single = findSpatialNeighborKeys(QChar('F'), 1, Layout::QWERTY);
    assert(single.size() == 1);
    assert(single.first() == QChar('F'));

    // Count = 2
    QList<QChar> pair = findSpatialNeighborKeys(QChar('F'), 2, Layout::QWERTY);
    assert(pair.size() == 2);
    assert(pair.first() == QChar('F'));
    assert(pair.at(1) != QChar('F'));

    // Count = 3 around 'F' on QWERTY: Centered! [ D, F, G ]
    QList<QChar> trioF = findSpatialNeighborKeys(QChar('F'), 3, Layout::QWERTY);
    assert(trioF.size() == 3);
    assert(trioF.at(1) == QChar('F')); // 'F' is centered
    assert(trioF.at(0) == QChar('D')); // Left neighbor
    assert(trioF.at(2) == QChar('G')); // Right neighbor

    // Count = 3 around 'J' on QWERTY: Centered! [ H, J, K ]
    QList<QChar> trioJ = findSpatialNeighborKeys(QChar('J'), 3, Layout::QWERTY);
    assert(trioJ.size() == 3);
    assert(trioJ.at(1) == QChar('J')); // 'J' is centered
    assert(trioJ.at(0) == QChar('H')); // Left neighbor
    assert(trioJ.at(2) == QChar('K')); // Right neighbor

    // Count = 4 around 'F'
    QList<QChar> quad = findSpatialNeighborKeys(QChar('F'), 4, Layout::QWERTY);
    assert(quad.size() == 4);
    assert(quad.contains(QChar('F')));

    std::cout << "testSpatialNeighborKeys PASSED\n";
}

int main()
{
    testLayoutParsing();
    testPreferredLetterPool();
    testSpatialNeighborKeys();
    std::cout << "All keyboard layout tests passed successfully!\n";
    return 0;
}
