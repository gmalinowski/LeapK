#ifndef OUTLINE_SOUNDHELPER_H
#define OUTLINE_SOUNDHELPER_H

#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <cmath>

namespace OutlineSound {

enum class SoundEffect {
    EnterJump = 0,
    LandWindow = 1
};

inline QByteArray generateWavData(SoundEffect effect)
{
    const int sampleRate = 44100;
    const double duration = (effect == SoundEffect::EnterJump) ? 0.012 : 0.015;
    const int numSamples = static_cast<int>(sampleRate * duration);

    // Audio characteristics
    const double baseFreq = (effect == SoundEffect::EnterJump) ? 2200.0 : 900.0;
    const double transientFreq = (effect == SoundEffect::EnterJump) ? 4600.0 : 2200.0;
    const double decayRate = (effect == SoundEffect::EnterJump) ? 350.0 : 280.0;
    const double transientDuration = (effect == SoundEffect::EnterJump) ? 0.0025 : 0.0030;

    QByteArray pcmData;
    pcmData.resize(numSamples * 2);
    qint16 *samples = reinterpret_cast<qint16 *>(pcmData.data());

    for (int i = 0; i < numSamples; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        const double env = std::exp(-t * decayRate);
        double val = std::sin(2.0 * M_PI * baseFreq * t) * env;

        if (t < transientDuration) {
            const double transEnv = 1.0 - (t / transientDuration);
            val += std::sin(2.0 * M_PI * transientFreq * t) * transEnv * 0.6;
        }

        // Clamp and scale to 16-bit
        const double scaled = val * 32767.0 * 0.35;
        const int clamped = std::max(-32768, std::min(32767, static_cast<int>(scaled)));
        samples[i] = static_cast<qint16>(clamped);
    }

    // Build 44-byte standard RIFF WAV header
    QByteArray wav;
    const quint32 subChunk2Size = static_cast<quint32>(pcmData.size());
    const quint32 chunkSize = 36 + subChunk2Size;

    wav.append("RIFF", 4);
    {
        char b[4];
        b[0] = chunkSize & 0xFF;
        b[1] = (chunkSize >> 8) & 0xFF;
        b[2] = (chunkSize >> 16) & 0xFF;
        b[3] = (chunkSize >> 24) & 0xFF;
        wav.append(b, 4);
    }
    wav.append("WAVEfmt ", 8);

    const quint32 subChunk1Size = 16; // PCM
    const quint16 audioFormat = 1;    // PCM
    const quint16 numChannels = 1;    // Mono
    const quint32 sRate = sampleRate;
    const quint32 byteRate = sampleRate * 2;
    const quint16 blockAlign = 2;
    const quint16 bitsPerSample = 16;

    {
        char b[16];
        b[0] = subChunk1Size & 0xFF; b[1] = (subChunk1Size >> 8) & 0xFF; b[2] = 0; b[3] = 0;
        b[4] = audioFormat & 0xFF; b[5] = (audioFormat >> 8) & 0xFF;
        b[6] = numChannels & 0xFF; b[7] = (numChannels >> 8) & 0xFF;
        b[8] = sRate & 0xFF; b[9] = (sRate >> 8) & 0xFF; b[10] = (sRate >> 16) & 0xFF; b[11] = (sRate >> 24) & 0xFF;
        b[12] = byteRate & 0xFF; b[13] = (byteRate >> 8) & 0xFF; b[14] = (byteRate >> 16) & 0xFF; b[15] = (byteRate >> 24) & 0xFF;
        wav.append(b, 16);

        char b2[4];
        b2[0] = blockAlign & 0xFF; b2[1] = (blockAlign >> 8) & 0xFF;
        b2[2] = bitsPerSample & 0xFF; b2[3] = (bitsPerSample >> 8) & 0xFF;
        wav.append(b2, 4);
    }

    wav.append("data", 4);
    {
        char b[4];
        b[0] = subChunk2Size & 0xFF;
        b[1] = (subChunk2Size >> 8) & 0xFF;
        b[2] = (subChunk2Size >> 16) & 0xFF;
        b[3] = (subChunk2Size >> 24) & 0xFF;
        wav.append(b, 4);
    }
    wav.append(pcmData);

    return wav;
}

inline QString soundFilePath(SoundEffect effect)
{
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/leapk_sounds");
    QDir().mkpath(cacheDir);
    const QString fileName = (effect == SoundEffect::EnterJump)
        ? QStringLiteral("click_enter.wav")
        : QStringLiteral("click_land.wav");
    return cacheDir + QLatin1Char('/') + fileName;
}

inline bool ensureSoundFilesExist()
{
    const SoundEffect effects[] = {SoundEffect::EnterJump, SoundEffect::LandWindow};
    for (SoundEffect eff : effects) {
        const QString path = soundFilePath(eff);
        if (!QFile::exists(path)) {
            QFile f(path);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(generateWavData(eff));
                f.close();
            } else {
                return false;
            }
        }
    }
    return true;
}

inline QString detectAudioPlayer()
{
    static QString cachedPlayer;
    if (!cachedPlayer.isEmpty()) {
        return cachedPlayer;
    }

    const QStringList candidates = {
        QStringLiteral("/usr/bin/pw-play"),
        QStringLiteral("/usr/bin/paplay"),
        QStringLiteral("/usr/bin/aplay"),
        QStringLiteral("pw-play"),
        QStringLiteral("paplay"),
        QStringLiteral("aplay")
    };

    for (const auto &prog : candidates) {
        const QString fullPath = QStandardPaths::findExecutable(prog);
        if (!fullPath.isEmpty()) {
            cachedPlayer = fullPath;
            return cachedPlayer;
        }
    }
    return QString();
}

inline void playSound(SoundEffect effect, int volumePercent = 30)
{
    if (volumePercent <= 0) {
        return;
    }

    ensureSoundFilesExist();
    const QString filePath = soundFilePath(effect);
    if (!QFile::exists(filePath)) {
        return;
    }

    const QString player = detectAudioPlayer();
    if (player.isEmpty()) {
        return;
    }

    QStringList args;
    if (player.endsWith(QLatin1String("pw-play"))) {
        const double vol = qBound(0.01, volumePercent / 100.0, 1.0);
        args << QStringLiteral("--volume") << QString::number(vol, 'f', 2) << filePath;
    } else if (player.endsWith(QLatin1String("paplay"))) {
        const int vol = qBound(500, qRound(volumePercent * 65536.0 / 100.0), 65536);
        args << QStringLiteral("--volume") << QString::number(vol) << filePath;
    } else {
        // aplay does not have commandline volume flag, plays file directly
        args << filePath;
    }

    QProcess::startDetached(player, args);
}

} // namespace OutlineSound

#endif // OUTLINE_SOUNDHELPER_H
