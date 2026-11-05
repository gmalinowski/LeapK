/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QString>
#include <cmath>

namespace LeapKNerdStats {

class RollingAverage {
public:
    explicit RollingAverage(double alpha = 0.08)
        : m_alpha(alpha)
        , m_initialized(false)
        , m_value(0.0)
    {}

    void addSample(double sample) {
        if (!m_initialized) {
            m_value = sample;
            m_initialized = true;
        } else {
            m_value = (m_alpha * sample) + ((1.0 - m_alpha) * m_value);
        }
    }

    double average() const {
        return m_value;
    }

    bool isInitialized() const {
        return m_initialized;
    }

    void reset() {
        m_initialized = false;
        m_value = 0.0;
    }

private:
    double m_alpha;
    bool m_initialized;
    double m_value;
};

struct NerdStatsSnapshot {
    double avgLeapkWatts = 0.0;         // Estimated average power draw of LeapK
    double avgPackagePowerWatts = 0.0;  // Total hardware (GPU/CPU/Battery) average power in Watts
    double avgCpuPercent = 0.0;         // Rolling average CPU utilization percentage
    double avgFrameLatencyMs = 0.0;     // Rolling average frame render latency in ms
    int refreshRateHz = 60;             // Compositor screen refresh rate
    int activeShaderPasses = 0;         // Active shader / effect passes
    double textureMemoryMb = 0.0;       // Cached texture memory footprint
    double hardwareTempC = 0.0;         // Hardware temperature in °C
    bool hasHardwarePower = false;
};

inline double queryHardwarePowerWatts(bool *outSuccess = nullptr)
{
    // 1. Try hwmon power1_average or power1_input
    for (int i = 0; i <= 15; ++i) {
        const QString pathAvg = QStringLiteral("/sys/class/hwmon/hwmon%1/power1_average").arg(i);
        QFile fileAvg(pathAvg);
        if (fileAvg.open(QIODevice::ReadOnly)) {
            bool ok = false;
            const qint64 valUw = fileAvg.readAll().trimmed().toLongLong(&ok);
            if (ok && valUw > 0) {
                if (outSuccess) *outSuccess = true;
                return valUw / 1000000.0; // microwatts to Watts
            }
        }
        const QString pathInput = QStringLiteral("/sys/class/hwmon/hwmon%1/power1_input").arg(i);
        QFile fileInput(pathInput);
        if (fileInput.open(QIODevice::ReadOnly)) {
            bool ok = false;
            const qint64 valUw = fileInput.readAll().trimmed().toLongLong(&ok);
            if (ok && valUw > 0) {
                if (outSuccess) *outSuccess = true;
                return valUw / 1000000.0;
            }
        }
    }

    // 2. Try battery power_now
    for (int i = 0; i <= 2; ++i) {
        const QString pathBat = QStringLiteral("/sys/class/power_supply/BAT%1/power_now").arg(i);
        QFile fileBat(pathBat);
        if (fileBat.open(QIODevice::ReadOnly)) {
            bool ok = false;
            const qint64 valUw = fileBat.readAll().trimmed().toLongLong(&ok);
            if (ok && valUw > 0) {
                if (outSuccess) *outSuccess = true;
                return valUw / 1000000.0;
            }
        }
    }

    if (outSuccess) *outSuccess = false;
    return 0.0;
}

inline double queryHardwareTemperatureC()
{
    for (int i = 0; i <= 15; ++i) {
        const QString namePath = QStringLiteral("/sys/class/hwmon/hwmon%1/name").arg(i);
        QFile nameFile(namePath);
        if (nameFile.open(QIODevice::ReadOnly)) {
            const QString name = QString::fromUtf8(nameFile.readAll()).trimmed().toLower();
            if (name.contains(QLatin1String("amdgpu")) || name.contains(QLatin1String("nouveau")) ||
                name.contains(QLatin1String("coretemp")) || name.contains(QLatin1String("nvme"))) {
                const QString tempPath = QStringLiteral("/sys/class/hwmon/hwmon%1/temp1_input").arg(i);
                QFile tempFile(tempPath);
                if (tempFile.open(QIODevice::ReadOnly)) {
                    bool ok = false;
                    const qint64 tempMilli = tempFile.readAll().trimmed().toLongLong(&ok);
                    if (ok && tempMilli > 0) {
                        return tempMilli / 1000.0;
                    }
                }
            }
        }
    }
    return 0.0;
}

inline QImage renderNerdStatsCard(qreal scale, const NerdStatsSnapshot &stats)
{
    const int totalW = qRound(330 * scale);
    const int totalH = qRound(165 * scale);
    const qreal radius = 8.0 * scale;

    QImage img(totalW, totalH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF cardRect(1.0 * scale, 1.0 * scale, totalW - 2.0 * scale, totalH - 2.0 * scale);

    // Glass background
    QPainterPath bgPath;
    bgPath.addRoundedRect(cardRect, radius, radius);
    p.fillPath(bgPath, QColor(14, 18, 26, 238));

    // Outer cyber glowing border
    QPen borderPen(QColor(0, 195, 255, 140), 1.2 * scale);
    p.strokePath(bgPath, borderPen);

    // Header bar
    const qreal headerH = 26.0 * scale;
    const QRectF headerRect(cardRect.left(), cardRect.top(), cardRect.width(), headerH);
    QPainterPath headerPath;
    headerPath.addRoundedRect(headerRect, radius, radius);
    p.fillPath(headerPath, QColor(0, 195, 255, 30));

    // Accent line under header
    p.setPen(QPen(QColor(0, 195, 255, 80), 1.0 * scale));
    p.drawLine(QPointF(cardRect.left(), headerRect.bottom()), QPointF(cardRect.right(), headerRect.bottom()));

    // Header Text
    QFont headerFont(QStringLiteral("monospace"));
    headerFont.setPixelSize(qMax(9, qRound(10.5 * scale)));
    headerFont.setBold(true);
    p.setFont(headerFont);
    p.setPen(QColor(0, 235, 255));
    p.drawText(QRectF(headerRect.left() + 10.0 * scale, headerRect.top(), headerRect.width() - 20.0 * scale, headerRect.height()),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("⚡ STATS FOR NERDS (AVERAGE)"));

    // Close hint on top right of header
    QFont hintFont(QStringLiteral("sans-serif"));
    hintFont.setPixelSize(qMax(8, qRound(8.5 * scale)));
    p.setFont(hintFont);
    p.setPen(QColor(130, 150, 175));
    p.drawText(QRectF(headerRect.left(), headerRect.top(), headerRect.width() - 10.0 * scale, headerRect.height()),
               Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("Meta+D to close"));

    // Data rows
    QFont labelFont(QStringLiteral("sans-serif"));
    labelFont.setPixelSize(qMax(8, qRound(9.5 * scale)));
    QFont valueFont(QStringLiteral("monospace"));
    valueFont.setPixelSize(qMax(9, qRound(10.0 * scale)));
    valueFont.setBold(true);

    struct StatRow {
        QString label;
        QString value;
        QColor valColor;
    };

    const QString powerStr = (stats.avgLeapkWatts > 0.0)
        ? (stats.avgPackagePowerWatts > 0.0
            ? QStringLiteral("%1 W avg (%2 W pkg)").arg(QString::number(stats.avgLeapkWatts, 'f', 2), QString::number(stats.avgPackagePowerWatts, 'f', 1))
            : QStringLiteral("%1 W avg").arg(QString::number(stats.avgLeapkWatts, 'f', 2)))
        : QStringLiteral("~0.05 W (idle)");

    const QString cpuStr = QStringLiteral("%1% avg").arg(QString::number(stats.avgCpuPercent, 'f', 1));
    const QString latencyStr = QStringLiteral("%1 ms avg (%2 Hz)").arg(QString::number(stats.avgFrameLatencyMs, 'f', 2)).arg(stats.refreshRateHz);
    const QString vramStr = QStringLiteral("%1 MB cached").arg(QString::number(stats.textureMemoryMb, 'f', 1));
    const QString pipelineStr = QStringLiteral("%1 passes%2")
        .arg(stats.activeShaderPasses)
        .arg(stats.hardwareTempC > 0.0 ? QStringLiteral(" • %1°C").arg(QString::number(stats.hardwareTempC, 'f', 0)) : QString());

    const QList<StatRow> rows = {
        { QStringLiteral("⚡ LeapK Power:"), powerStr, QColor(46, 204, 113) },
        { QStringLiteral("🧠 CPU Utilization:"), cpuStr, QColor(241, 196, 15) },
        { QStringLiteral("⏱️ Render Latency:"), latencyStr, QColor(0, 195, 255) },
        { QStringLiteral("💾 Texture VRAM:"), vramStr, QColor(168, 85, 247) },
        { QStringLiteral("🎨 GLSL Pipeline:"), pipelineStr, QColor(230, 240, 250) },
    };

    qreal curY = headerRect.bottom() + 6.0 * scale;
    const qreal rowH = (cardRect.bottom() - curY - 4.0 * scale) / rows.size();
    const qreal padX = 12.0 * scale;

    for (const auto &row : rows) {
        const QRectF rowRect(cardRect.left() + padX, curY, cardRect.width() - 2 * padX, rowH);

        p.setFont(labelFont);
        p.setPen(QColor(160, 175, 195));
        p.drawText(rowRect, Qt::AlignLeft | Qt::AlignVCenter, row.label);

        p.setFont(valueFont);
        p.setPen(row.valColor);
        p.drawText(rowRect, Qt::AlignRight | Qt::AlignVCenter, row.value);

        curY += rowH;
    }

    p.end();
    return img;
}

} // namespace LeapKNerdStats
