#ifndef OUTLINE_SOUNDHELPER_H
#define OUTLINE_SOUNDHELPER_H

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QLibrary>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <dlfcn.h>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace OutlineSound {

enum class SoundEffect {
    EnterJump = 0,
    LandWindow = 1,
    SwitchWorkspace = 2,
    NavTab = 3,
    Dismiss = 4,
    Stash = 5
};

enum class SoundProfile {
    CrispClick = 0,
    SoftPop = 1,
    SciFiBlip = 2,
    Mechanical = 3
};

inline QString effectToString(SoundEffect effect)
{
    switch (effect) {
    case SoundEffect::EnterJump: return QStringLiteral("enter");
    case SoundEffect::LandWindow: return QStringLiteral("land");
    case SoundEffect::SwitchWorkspace: return QStringLiteral("workspace");
    case SoundEffect::NavTab: return QStringLiteral("nav");
    case SoundEffect::Dismiss: return QStringLiteral("dismiss");
    case SoundEffect::Stash: return QStringLiteral("stash");
    }
    return QStringLiteral("sound");
}

inline QString profileToString(SoundProfile profile)
{
    switch (profile) {
    case SoundProfile::CrispClick: return QStringLiteral("crisp_click");
    case SoundProfile::SoftPop: return QStringLiteral("soft_pop");
    case SoundProfile::SciFiBlip: return QStringLiteral("scifi_blip");
    case SoundProfile::Mechanical: return QStringLiteral("mechanical");
    }
    return QStringLiteral("crisp_click");
}

inline SoundProfile profileFromString(const QString &str)
{
    if (str == QStringLiteral("soft_pop")) return SoundProfile::SoftPop;
    if (str == QStringLiteral("scifi_blip")) return SoundProfile::SciFiBlip;
    if (str == QStringLiteral("mechanical")) return SoundProfile::Mechanical;
    return SoundProfile::CrispClick;
}

inline QByteArray generatePcmData(SoundEffect effect, SoundProfile profile = SoundProfile::CrispClick, int volumePercent = 100)
{
    const int sampleRate = 44100;
    double duration = 0.015;
    if (effect == SoundEffect::SwitchWorkspace) duration = 0.022;
    else if (effect == SoundEffect::NavTab) duration = 0.009;
    else if (effect == SoundEffect::EnterJump) duration = 0.014;
    else if (effect == SoundEffect::Dismiss) duration = 0.011;
    else if (effect == SoundEffect::Stash) duration = 0.016;

    const int numSamples = static_cast<int>(sampleRate * duration);
    QByteArray pcmData;
    pcmData.resize(numSamples * 2);
    qint16 *samples = reinterpret_cast<qint16 *>(pcmData.data());

    const double vol = qBound(0.0, volumePercent / 100.0, 1.0);

    for (int i = 0; i < numSamples; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        double val = 0.0;

        switch (profile) {
        case SoundProfile::CrispClick: {
            double baseF = 1800.0, transF = 3800.0, decay = 320.0;
            if (effect == SoundEffect::EnterJump) { baseF = 2400.0; transF = 4800.0; decay = 350.0; }
            else if (effect == SoundEffect::LandWindow) { baseF = 950.0; transF = 2800.0; decay = 280.0; }
            else if (effect == SoundEffect::SwitchWorkspace) { baseF = 1350.0; transF = 2200.0; decay = 200.0; }
            else if (effect == SoundEffect::NavTab) { baseF = 2100.0; transF = 4200.0; decay = 450.0; }
            else if (effect == SoundEffect::Dismiss) { baseF = 1100.0; transF = 1800.0; decay = 380.0; }
            else if (effect == SoundEffect::Stash) { baseF = 1600.0; transF = 3400.0; decay = 300.0; }

            const double env = std::exp(-t * decay);
            val = std::sin(2.0 * M_PI * baseF * t) * env;
            if (t < 0.003) {
                val += std::sin(2.0 * M_PI * transF * t) * (1.0 - t / 0.003) * 0.5;
            }
            break;
        }
        case SoundProfile::SoftPop: {
            double freq = 650.0, decay = 260.0;
            if (effect == SoundEffect::EnterJump) { freq = 850.0; decay = 280.0; }
            else if (effect == SoundEffect::LandWindow) { freq = 520.0; decay = 240.0; }
            else if (effect == SoundEffect::SwitchWorkspace) { freq = 440.0; decay = 180.0; }
            else if (effect == SoundEffect::NavTab) { freq = 980.0; decay = 360.0; }
            else if (effect == SoundEffect::Dismiss) { freq = 480.0; decay = 300.0; }
            else if (effect == SoundEffect::Stash) { freq = 720.0; decay = 250.0; }

            const double fDrop = freq * (1.0 + 0.5 * std::exp(-t * 400.0));
            const double env = std::exp(-t * decay);
            val = std::sin(2.0 * M_PI * fDrop * t) * env;
            break;
        }
        case SoundProfile::SciFiBlip: {
            double startF = 3200.0, endF = 1200.0, decay = 280.0;
            if (effect == SoundEffect::EnterJump) { startF = 1600.0; endF = 3600.0; decay = 260.0; }
            else if (effect == SoundEffect::LandWindow) { startF = 2800.0; endF = 900.0; decay = 290.0; }
            else if (effect == SoundEffect::SwitchWorkspace) { startF = 1200.0; endF = 2400.0; decay = 170.0; }
            else if (effect == SoundEffect::NavTab) { startF = 3400.0; endF = 2000.0; decay = 400.0; }
            else if (effect == SoundEffect::Dismiss) { startF = 2200.0; endF = 800.0; decay = 340.0; }
            else if (effect == SoundEffect::Stash) { startF = 1800.0; endF = 2800.0; decay = 250.0; }

            const double curF = startF + (endF - startF) * (t / duration);
            const double env = std::exp(-t * decay);
            const double mod = std::sin(2.0 * M_PI * (curF * 0.5) * t) * 0.3;
            val = std::sin(2.0 * M_PI * (curF * (1.0 + mod)) * t) * env;
            break;
        }
        case SoundProfile::Mechanical: {
            double thudF = 420.0, snapF = 3100.0, decay = 350.0;
            if (effect == SoundEffect::EnterJump) { thudF = 550.0; snapF = 3600.0; }
            else if (effect == SoundEffect::LandWindow) { thudF = 380.0; snapF = 2800.0; }
            else if (effect == SoundEffect::SwitchWorkspace) { thudF = 480.0; snapF = 2400.0; decay = 220.0; }
            else if (effect == SoundEffect::NavTab) { thudF = 620.0; snapF = 3800.0; decay = 450.0; }
            else if (effect == SoundEffect::Dismiss) { thudF = 350.0; snapF = 2200.0; }
            else if (effect == SoundEffect::Stash) { thudF = 500.0; snapF = 3200.0; }

            const double env = std::exp(-t * decay);
            val = std::sin(2.0 * M_PI * thudF * t) * env * 0.7;
            if (t < 0.0025) {
                val += std::sin(2.0 * M_PI * snapF * t) * (1.0 - t / 0.0025) * 0.8;
            }
            if (t >= 0.0025 && t < 0.0055) {
                const double t2 = t - 0.0025;
                val += std::sin(2.0 * M_PI * (snapF * 0.85) * t2) * (1.0 - t2 / 0.003) * 0.5;
            }
            break;
        }
        }

        const double scaled = val * 32767.0 * 0.40 * vol;
        const int clamped = std::max(-32768, std::min(32767, static_cast<int>(scaled)));
        samples[i] = static_cast<qint16>(clamped);
    }
    return pcmData;
}

inline QByteArray buildWavHeader(int pcmSize, int sampleRate = 44100, int channels = 1, int bitsPerSample = 16)
{
    QByteArray wav;
    const quint32 subChunk2Size = static_cast<quint32>(pcmSize);
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

    const quint32 subChunk1Size = 16;
    const quint16 audioFormat = 1;
    const quint16 numChannels = channels;
    const quint32 sRate = sampleRate;
    const quint32 byteRate = sampleRate * channels * (bitsPerSample / 8);
    const quint16 blockAlign = channels * (bitsPerSample / 8);
    const quint16 bps = bitsPerSample;

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
        b2[2] = bps & 0xFF; b2[3] = (bps >> 8) & 0xFF;
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
    return wav;
}

inline QByteArray generateWavData(SoundEffect effect, SoundProfile profile = SoundProfile::CrispClick)
{
    const QByteArray pcm = generatePcmData(effect, profile, 100);
    QByteArray wav = buildWavHeader(pcm.size());
    wav.append(pcm);
    return wav;
}

inline QString soundFilePath(SoundEffect effect, SoundProfile profile = SoundProfile::CrispClick)
{
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/leapk_sounds");
    QDir().mkpath(cacheDir);
    if (profile == SoundProfile::CrispClick) {
        switch (effect) {
        case SoundEffect::EnterJump: return cacheDir + QStringLiteral("/click_enter.wav");
        case SoundEffect::LandWindow: return cacheDir + QStringLiteral("/click_land.wav");
        case SoundEffect::SwitchWorkspace: return cacheDir + QStringLiteral("/click_workspace.wav");
        case SoundEffect::NavTab: return cacheDir + QStringLiteral("/click_nav.wav");
        case SoundEffect::Dismiss: return cacheDir + QStringLiteral("/click_dismiss.wav");
        case SoundEffect::Stash: return cacheDir + QStringLiteral("/click_stash.wav");
        }
    }
    const QString profStr = profileToString(profile);
    const QString effStr = effectToString(effect);
    return QStringLiteral("%1/click_%2_%3.wav").arg(cacheDir, effStr, profStr);
}

inline bool ensureSoundFilesExist()
{
    const SoundEffect effects[] = {
        SoundEffect::EnterJump, SoundEffect::LandWindow, SoundEffect::SwitchWorkspace,
        SoundEffect::NavTab, SoundEffect::Dismiss, SoundEffect::Stash
    };
    const SoundProfile profiles[] = {
        SoundProfile::CrispClick, SoundProfile::SoftPop, SoundProfile::SciFiBlip, SoundProfile::Mechanical
    };
    for (SoundProfile prof : profiles) {
        for (SoundEffect eff : effects) {
            const QString path = soundFilePath(eff, prof);
            if (!QFile::exists(path)) {
                QFile f(path);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(generateWavData(eff, prof));
                    f.close();
                }
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

class AudioEngine {
public:
    struct PlayRequest {
        SoundEffect effect;
        int volumePercent;
        SoundProfile profile;
        QString customFilePath;
    };

    static AudioEngine &instance()
    {
        static AudioEngine eng;
        return eng;
    }

    void play(SoundEffect effect, int volumePercent, SoundProfile profile, const QString &customFilePath)
    {
        if (volumePercent <= 0) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(PlayRequest{effect, volumePercent, profile, customFilePath});
        }
        m_cv.notify_one();
    }

    ~AudioEngine()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        cleanup();
    }

private:
    AudioEngine()
    {
        m_thread = std::thread(&AudioEngine::workerLoop, this);
    }

    typedef void pa_simple;
    typedef struct pa_sample_spec {
        int format;
        uint32_t rate;
        uint8_t channels;
    } pa_sample_spec;
    typedef struct pa_buffer_attr {
        uint32_t maxlength, tlength, prebuf, minreq, fragsize;
    } pa_buffer_attr;

    typedef pa_simple* (*pa_simple_new_t)(const char *, const char *, int, const char *, const char *, const pa_sample_spec *, const void *, const pa_buffer_attr *, int *);
    typedef int (*pa_simple_write_t)(pa_simple *, const void *, size_t, int *);
    typedef void (*pa_simple_free_t)(pa_simple *);

    typedef void ca_context;
    typedef int (*ca_create_t)(ca_context **);
    typedef int (*ca_open_t)(ca_context *);
    typedef int (*ca_play_t)(ca_context *, unsigned int, ...);
    typedef void (*ca_destroy_t)(ca_context *);

    void *m_pulseLib = nullptr;
    pa_simple *m_pulse = nullptr;
    pa_simple_write_t m_p_write = nullptr;
    pa_simple_free_t m_p_free = nullptr;

    void *m_canberraLib = nullptr;
    ca_context *m_canberra = nullptr;
    ca_play_t m_ca_play = nullptr;
    ca_destroy_t m_ca_destroy = nullptr;

    std::queue<PlayRequest> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_thread;
    bool m_stop = false;

    void initPulse()
    {
        if (m_pulse) return;
        if (!m_pulseLib) {
            m_pulseLib = dlopen("libpulse-simple.so.0", RTLD_LAZY);
            if (!m_pulseLib) {
                m_pulseLib = dlopen("libpulse-simple.so", RTLD_LAZY);
            }
        }
        if (m_pulseLib) {
            pa_simple_new_t p_new = (pa_simple_new_t)dlsym(m_pulseLib, "pa_simple_new");
            m_p_write = (pa_simple_write_t)dlsym(m_pulseLib, "pa_simple_write");
            m_p_free = (pa_simple_free_t)dlsym(m_pulseLib, "pa_simple_free");
            if (p_new && m_p_write && m_p_free) {
                pa_sample_spec ss{3 /* PA_SAMPLE_S16LE */, 44100, 1};
                pa_buffer_attr attr{
                    static_cast<uint32_t>(-1),
                    static_cast<uint32_t>(44100 * 2 * 0.02), // 20ms buffer for ultra-low latency
                    static_cast<uint32_t>(-1),
                    static_cast<uint32_t>(-1),
                    static_cast<uint32_t>(-1)
                };
                int err = 0;
                m_pulse = p_new(nullptr, "LeapK", 1 /* PLAYBACK */, nullptr, "Feedback", &ss, nullptr, &attr, &err);
            }
        }
    }

    void initCanberra()
    {
        if (m_canberra) return;
        if (!m_canberraLib) {
            m_canberraLib = dlopen("libcanberra.so.0", RTLD_LAZY);
            if (!m_canberraLib) {
                m_canberraLib = dlopen("libcanberra.so", RTLD_LAZY);
            }
        }
        if (m_canberraLib) {
            ca_create_t ca_create = (ca_create_t)dlsym(m_canberraLib, "ca_context_create");
            ca_open_t ca_open = (ca_open_t)dlsym(m_canberraLib, "ca_context_open");
            m_ca_play = (ca_play_t)dlsym(m_canberraLib, "ca_context_play");
            m_ca_destroy = (ca_destroy_t)dlsym(m_canberraLib, "ca_context_destroy");
            if (ca_create && ca_open && m_ca_play && m_ca_destroy) {
                ca_create(&m_canberra);
                if (m_canberra) {
                    ca_open(m_canberra);
                }
            }
        }
    }

    void cleanup()
    {
        if (m_pulse && m_p_free) {
            m_p_free(m_pulse);
            m_pulse = nullptr;
        }
        if (m_pulseLib) {
            dlclose(m_pulseLib);
            m_pulseLib = nullptr;
        }
        if (m_canberra && m_ca_destroy) {
            m_ca_destroy(m_canberra);
            m_canberra = nullptr;
        }
        if (m_canberraLib) {
            dlclose(m_canberraLib);
            m_canberraLib = nullptr;
        }
    }

    void workerLoop()
    {
        initPulse();
        if (!m_pulse) {
            initCanberra();
        }

        while (true) {
            PlayRequest req;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() { return m_stop || !m_queue.empty(); });
                if (m_stop && m_queue.empty()) {
                    break;
                }
                req = m_queue.front();
                m_queue.pop();
            }

            dispatchPlay(req);
        }
    }

    void dispatchPlay(const PlayRequest &req)
    {
        // 1. If custom file is specified and exists, play it
        if (!req.customFilePath.isEmpty() && QFile::exists(req.customFilePath)) {
            if (playFileViaPulse(req.customFilePath, req.volumePercent)) {
                return;
            }
            if (playFileViaCanberra(req.customFilePath, req.volumePercent)) {
                return;
            }
            playFileViaProcess(req.customFilePath, req.volumePercent);
            return;
        }

        // 2. Synthesized zero-latency sound
        const QByteArray pcm = generatePcmData(req.effect, req.profile, req.volumePercent);
        if (pcm.isEmpty()) return;

        // Try low-latency Pulse stream first
        if (!m_pulse) {
            initPulse();
        }
        if (m_pulse && m_p_write) {
            int err = 0;
            if (m_p_write(m_pulse, pcm.constData(), pcm.size(), &err) >= 0) {
                return;
            }
            // Stream had error (e.g. server restart)
            if (m_p_free) m_p_free(m_pulse);
            m_pulse = nullptr;
        }

        // Fallback to Canberra
        const QString filePath = soundFilePath(req.effect, req.profile);
        if (!QFile::exists(filePath)) {
            QFile f(filePath);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(generateWavData(req.effect, req.profile));
                f.close();
            }
        }
        if (playFileViaCanberra(filePath, req.volumePercent)) {
            return;
        }

        // Fallback to Process
        playFileViaProcess(filePath, req.volumePercent);
    }

    bool playFileViaPulse(const QString &filePath, int volumePercent)
    {
        if (!m_pulse) initPulse();
        if (!m_pulse || !m_p_write) return false;

        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) return false;
        QByteArray data = f.readAll();
        if (data.size() < 44 || !data.startsWith("RIFF")) return false;

        // Skip 44-byte WAV header and apply volume
        const int pcmBytes = data.size() - 44;
        const qint16 *src = reinterpret_cast<const qint16 *>(data.constData() + 44);
        const int numSamples = pcmBytes / 2;
        const double vol = qBound(0.0, volumePercent / 100.0, 1.0);

        std::vector<qint16> scaled(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            scaled[i] = static_cast<qint16>(qBound(-32768, static_cast<int>(src[i] * vol), 32767));
        }

        int err = 0;
        return (m_p_write(m_pulse, scaled.data(), scaled.size() * sizeof(qint16), &err) >= 0);
    }

    bool playFileViaCanberra(const QString &filePath, int volumePercent)
    {
        if (!m_canberra) initCanberra();
        if (!m_canberra || !m_ca_play) return false;

        const QByteArray pathUtf8 = filePath.toUtf8();
        const QByteArray volStr = QString::number(qBound(-30, qRound((volumePercent - 100) * 0.3), 0)).toUtf8();
        const int res = m_ca_play(m_canberra, 1, "media.filename", pathUtf8.constData(), "canberra.volume", volStr.constData(), nullptr);
        return (res == 0);
    }

    void playFileViaProcess(const QString &filePath, int volumePercent)
    {
        const QString player = detectAudioPlayer();
        if (player.isEmpty()) return;

        QStringList args;
        if (player.endsWith(QLatin1String("pw-play"))) {
            const double vol = qBound(0.01, volumePercent / 100.0, 1.0);
            args << QStringLiteral("--volume") << QString::number(vol, 'f', 2) << filePath;
        } else if (player.endsWith(QLatin1String("paplay"))) {
            const int vol = qBound(500, qRound(volumePercent * 65536.0 / 100.0), 65536);
            args << QStringLiteral("--volume") << QString::number(vol) << filePath;
        } else {
            args << filePath;
        }
        QProcess::startDetached(player, args);
    }
};

inline void playSound(SoundEffect effect, int volumePercent = 30, SoundProfile profile = SoundProfile::CrispClick, const QString &customFilePath = QString())
{
    if (volumePercent <= 0) {
        return;
    }
    AudioEngine::instance().play(effect, volumePercent, profile, customFilePath);
}

} // namespace OutlineSound

#endif // OUTLINE_SOUNDHELPER_H
