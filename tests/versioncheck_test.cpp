/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVersionNumber>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static int failures = 0;

static void check(bool condition, const char *desc)
{
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", desc);
        ++failures;
    }
}

int main()
{
    // 1. QVersionNumber comparison checks
    {
        const QVersionNumber vCurrent = QVersionNumber::fromString(QStringLiteral("1.4.7"));
        const QVersionNumber vSame = QVersionNumber::fromString(QStringLiteral("1.4.7"));
        const QVersionNumber vOlder = QVersionNumber::fromString(QStringLiteral("1.4.6"));
        const QVersionNumber vNewerMinor = QVersionNumber::fromString(QStringLiteral("1.4.8"));
        const QVersionNumber vNewerMajor = QVersionNumber::fromString(QStringLiteral("2.0.0"));

        check(vCurrent == vSame, "v1.4.7 == v1.4.7");
        check(vCurrent > vOlder, "v1.4.7 > v1.4.6");
        check(vNewerMinor > vCurrent, "v1.4.8 > v1.4.7");
        check(vNewerMajor > vCurrent, "v2.0.0 > v1.4.7");
    }

    // 2. Tag cleaning
    {
        QString tag = QStringLiteral("v1.4.8");
        if (tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
            tag.remove(0, 1);
        }
        check(tag == QStringLiteral("1.4.8"), "strip 'v' prefix from tag");
        const QVersionNumber parsed = QVersionNumber::fromString(tag);
        check(parsed == QVersionNumber(1, 4, 8), "parsed cleaned tag matches 1.4.8");
    }

    // 3. GitHub JSON payload parsing
    {
        const char *sampleJson = R"json([
            {
                "tag_name": "v1.4.8",
                "prerelease": true,
                "html_url": "https://github.com/gmalinowski/leapk/releases/tag/v1.4.8",
                "body": "Bugfixes and performance improvements"
            },
            {
                "tag_name": "v1.3.6",
                "prerelease": false,
                "html_url": "https://github.com/gmalinowski/leapk/releases/tag/v1.3.6",
                "body": "Stable release"
            }
        ])json";

        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(sampleJson));
        check(!doc.isNull() && doc.isArray(), "parsed valid JSON releases array");

        const QJsonArray releases = doc.array();
        check(releases.size() == 2, "releases array size is 2");

        const QJsonObject latest = releases.first().toObject();
        check(latest.value(QStringLiteral("tag_name")).toString() == QStringLiteral("v1.4.8"), "latest tag is v1.4.8");
        check(latest.value(QStringLiteral("prerelease")).toBool() == true, "latest is prerelease");
        check(!latest.value(QStringLiteral("html_url")).toString().isEmpty(), "has html_url");
        check(!latest.value(QStringLiteral("body")).toString().isEmpty(), "has release notes body");

        // Install command construction
        const bool isPre = latest.value(QStringLiteral("prerelease")).toBool();
        const QString cmd = isPre
            ? QStringLiteral("curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash -s -- --pre")
            : QStringLiteral("curl -sSL https://raw.githubusercontent.com/gmalinowski/leapk/main/install.sh | bash");
        check(cmd.contains(QStringLiteral("--pre")), "prerelease command contains --pre flag");
    }

    // 4. Beta and Prerelease Version Parsing
    {
        QString betaTag = QStringLiteral("v1.6.0-beta1");
        if (betaTag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
            betaTag.remove(0, 1);
        }
        check(betaTag == QStringLiteral("1.6.0-beta1"), "cleaned beta tag");
        const QVersionNumber vBeta = QVersionNumber::fromString(betaTag);
        const QVersionNumber vPrev = QVersionNumber(1, 5, 2);
        check(vBeta > vPrev, "v1.6.0-beta1 > v1.5.2");

        const QVersionNumber vRelease = QVersionNumber(1, 6, 0);
        check(vRelease >= vBeta, "v1.6.0 >= v1.6.0-beta1");
    }

    // 5. Malformed GitHub JSON payloads
    {
        // Malformed syntax
        const QJsonDocument docBroken = QJsonDocument::fromJson(QByteArray("{ broken }"));
        check(docBroken.isNull(), "detect broken json syntax");

        // Object instead of array
        const QJsonDocument docObj = QJsonDocument::fromJson(QByteArray("{\"message\": \"Not Found\"}"));
        check(docObj.isObject() && !docObj.isArray(), "detect API error object");

        // Empty array
        const QJsonDocument docEmpty = QJsonDocument::fromJson(QByteArray("[]"));
        check(docEmpty.isArray() && docEmpty.array().isEmpty(), "detect empty releases list");
    }

    std::printf("versioncheck_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
