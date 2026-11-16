// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AUDIOVISUALIZER_H
#define AUDIOVISUALIZER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QLibrary>
#include <QElapsedTimer>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace OutlineAudioVisualizer {

struct AudioVisualizerData {
    float rms = 0.0f;           // Overall RMS volume: 0.0 to 1.0
    float peak = 0.0f;          // Peak amplitude: 0.0 to 1.0
    float bass = 0.0f;          // Low-frequency bass energy: 0.0 to 1.0
    float mids = 0.0f;          // Midrange energy: 0.0 to 1.0
    float highs = 0.0f;         // High-frequency treble energy: 0.0 to 1.0
    float beatPulse = 0.0f;     // Instant attack, exponential decay beat trigger: 0.0 to 1.0
    std::array<float, 64> waveform{};    // Normalized oscilloscope waveform samples: -1.0 to 1.0
    std::array<float, 16> spectrumBars{};// 16-band graphic equalizer bars: 0.0 to 1.0
    bool isSilent = true;       // True if audio is below noise floor
    qint64 lastUpdateMs = 0;    // Timestamp of last update
};

// Second-order Direct Form II Transposed Biquad Filter
struct BiquadFilter {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void setLowPass(float cutoffHz, float sampleRate, float q = 0.707f) {
        float safeHz = std::clamp(cutoffHz, 20.0f, sampleRate * 0.45f);
        float w0 = 2.0f * 3.141592653589793f * safeHz / sampleRate;
        float alpha = std::sin(w0) / (2.0f * q);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        b0 = ((1.0f - cosw0) / 2.0f) / a0;
        b1 = (1.0f - cosw0) / a0;
        b2 = ((1.0f - cosw0) / 2.0f) / a0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
        z1 = z2 = 0.0f;
    }

    void setBandPass(float centerHz, float sampleRate, float q = 1.4f) {
        float safeHz = std::clamp(centerHz, 20.0f, sampleRate * 0.45f);
        float w0 = 2.0f * 3.141592653589793f * safeHz / sampleRate;
        float alpha = std::sin(w0) / (2.0f * q);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        b0 = alpha / a0;
        b1 = 0.0f;
        b2 = -alpha / a0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
        z1 = z2 = 0.0f;
    }

    void setHighPass(float cutoffHz, float sampleRate, float q = 0.707f) {
        float safeHz = std::clamp(cutoffHz, 20.0f, sampleRate * 0.45f);
        float w0 = 2.0f * 3.141592653589793f * safeHz / sampleRate;
        float alpha = std::sin(w0) / (2.0f * q);
        float cosw0 = std::cos(w0);
        float a0 = 1.0f + alpha;
        b0 = ((1.0f + cosw0) / 2.0f) / a0;
        b1 = (-(1.0f + cosw0)) / a0;
        b2 = ((1.0f + cosw0) / 2.0f) / a0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
        z1 = z2 = 0.0f;
    }

    inline float process(float in) {
        float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return out;
    }

    void reset() {
        z1 = z2 = 0.0f;
    }
};

class AudioDspEngine {
public:
    explicit AudioDspEngine(float sampleRate = 22050.0f)
        : m_sampleRate(sampleRate)
    {
        initFilters();
    }

    void reset() {
        m_bassFilter.reset();
        m_midsFilter.reset();
        m_highsFilter.reset();
        for (auto &f : m_eqFilters) {
            f.reset();
        }
        m_movingAverageBass = 0.01f;
        m_timeSinceBeatMs = 999;
    }

    void processChunk(const int16_t *samples, int count, AudioVisualizerData &data, qreal sensitivity = 1.0)
    {
        if (!samples || count <= 0) {
            return;
        }

        const float sens = static_cast<float>(std::max(0.1, sensitivity));
        float sumSquares = 0.0f;
        float peakVal = 0.0f;

        float bassSum = 0.0f;
        float midsSum = 0.0f;
        float highsSum = 0.0f;
        std::array<float, 16> eqBandSums{};

        // Downsample input to populate 64-sample waveform buffer
        const int step = std::max(1, count / 64);
        for (int i = 0; i < 64; ++i) {
            int idx = std::min(count - 1, i * step);
            float normSample = samples[idx] / 32768.0f;
            data.waveform[i] = std::clamp(normSample * sens, -1.0f, 1.0f);
        }

        // Process audio stream through filter banks
        for (int i = 0; i < count; ++i) {
            const float s = samples[i] / 32768.0f;
            sumSquares += s * s;
            const float absS = std::abs(s);
            if (absS > peakVal) {
                peakVal = absS;
            }

            // 3-band energy
            float bOut = m_bassFilter.process(s);
            bassSum += bOut * bOut;

            float mOut = m_midsFilter.process(s);
            midsSum += mOut * mOut;

            float hOut = m_highsFilter.process(s);
            highsSum += hOut * hOut;

            // 16-band equalizer
            for (size_t k = 0; k < 16; ++k) {
                float eqOut = m_eqFilters[k].process(s);
                eqBandSums[k] += eqOut * eqOut;
            }
        }

        const float invCount = 1.0f / count;
        data.rms = std::clamp(std::sqrt(sumSquares * invCount) * sens * 2.5f, 0.0f, 1.0f);
        data.peak = std::clamp(peakVal * sens, 0.0f, 1.0f);

        data.bass = std::clamp(std::sqrt(bassSum * invCount) * sens * 3.0f, 0.0f, 1.0f);
        data.mids = std::clamp(std::sqrt(midsSum * invCount) * sens * 2.8f, 0.0f, 1.0f);
        data.highs = std::clamp(std::sqrt(highsSum * invCount) * sens * 3.5f, 0.0f, 1.0f);

        data.isSilent = (data.rms < 0.005f);

        // Beat Detection: Fast transient detection in bass register
        const float instantBassEnergy = bassSum * invCount;
        m_movingAverageBass = m_movingAverageBass * 0.93f + instantBassEnergy * 0.07f;
        const float beatThreshold = m_movingAverageBass * 1.35f + 0.002f;

        m_timeSinceBeatMs += static_cast<int>((count * 1000.0f) / m_sampleRate);

        if (instantBassEnergy > beatThreshold && m_timeSinceBeatMs >= 130 && data.bass > 0.08f) {
            data.beatPulse = 1.0f;
            m_timeSinceBeatMs = 0;
        } else {
            // Smooth exponential decay
            data.beatPulse *= 0.88f;
        }

        // 16-Band Equalizer: Attack and Peak Decay
        const float bandGains[16] = {
            3.2f, 3.0f, 2.8f, 2.6f, 2.5f, 2.4f, 2.4f, 2.5f,
            2.6f, 2.8f, 3.0f, 3.2f, 3.5f, 3.8f, 4.0f, 4.2f
        };
        for (size_t k = 0; k < 16; ++k) {
            float target = std::clamp(std::sqrt(eqBandSums[k] * invCount) * sens * bandGains[k], 0.0f, 1.0f);
            if (target >= data.spectrumBars[k]) {
                data.spectrumBars[k] = target; // Instant attack
            } else {
                data.spectrumBars[k] = std::max(0.0f, data.spectrumBars[k] * 0.85f); // Smooth decay
            }
        }
    }

    void decay(AudioVisualizerData &data, float deltaSec) {
        float factor = std::clamp(1.0f - deltaSec * 8.0f, 0.0f, 1.0f);
        data.beatPulse *= factor;
        data.rms *= factor;
        data.peak *= factor;
        data.bass *= factor;
        data.mids *= factor;
        data.highs *= factor;
        for (auto &b : data.spectrumBars) {
            b *= factor;
        }
        for (auto &w : data.waveform) {
            w *= factor;
        }
    }

private:
    void initFilters() {
        // 3-Band Filters: Bass < 180Hz, Mids 180Hz - 3500Hz, Highs > 3500Hz
        m_bassFilter.setLowPass(180.0f, m_sampleRate, 0.707f);
        m_midsFilter.setBandPass(1200.0f, m_sampleRate, 0.8f);
        m_highsFilter.setHighPass(3500.0f, m_sampleRate, 0.707f);

        // 16 Logarithmically spaced frequency bands from 60 Hz to 9800 Hz (< Nyquist 11025 Hz)
        const float centerFreqs[16] = {
            60.0f, 90.0f, 130.0f, 200.0f, 300.0f, 450.0f, 700.0f, 1000.0f,
            1500.0f, 2200.0f, 3200.0f, 4500.0f, 6000.0f, 7500.0f, 8800.0f, 9800.0f
        };
        for (size_t i = 0; i < 16; ++i) {
            m_eqFilters[i].setBandPass(centerFreqs[i], m_sampleRate, 1.5f);
        }
    }

    float m_sampleRate;
    BiquadFilter m_bassFilter;
    BiquadFilter m_midsFilter;
    BiquadFilter m_highsFilter;
    std::array<BiquadFilter, 16> m_eqFilters;
    float m_movingAverageBass = 0.01f;
    int m_timeSinceBeatMs = 999;
};

// PulseAudio Simple API Type Definitions for Dynamic Loading
typedef void pa_simple;
struct pa_sample_spec {
    int format;
    uint32_t rate;
    uint8_t channels;
};

class AudioVisualizerThread : public QThread {
public:
    explicit AudioVisualizerThread(QObject *parent = nullptr)
        : QThread(parent)
        , m_dsp(22050.0f)
    {
    }

    ~AudioVisualizerThread() override {
        stopThread();
    }

    void stopThread() {
        m_running = false;
        m_wakeCondition.wakeAll();
        wait(500);
    }

    void setActive(bool active) {
        QMutexLocker locker(&m_mutex);
        m_active = active;
        if (m_active) {
            m_wakeCondition.wakeAll();
        }
    }

    void setSensitivity(qreal sensitivity) {
        QMutexLocker locker(&m_mutex);
        m_sensitivity = sensitivity;
    }

    AudioVisualizerData currentData() {
        QMutexLocker locker(&m_mutex);
        return m_data;
    }

protected:
    void run() override {
        // Dynamically resolve PulseAudio Simple API
        QLibrary pulseSimple(QStringLiteral("libpulse-simple.so.0"));
        if (!pulseSimple.load()) {
            pulseSimple.setFileName(QStringLiteral("libpulse-simple.so"));
            if (!pulseSimple.load()) {
                pulseSimple.setFileName(QStringLiteral("libpulse-simple"));
                pulseSimple.load();
            }
        }

        typedef pa_simple* (*fn_pa_simple_new)(const char *, const char *, int, const char *, const char *, const pa_sample_spec *, const void *, const void *, int *);
        typedef int (*fn_pa_simple_read)(pa_simple *, void *, size_t, int *);
        typedef void (*fn_pa_simple_free)(pa_simple *);

        auto pa_new = reinterpret_cast<fn_pa_simple_new>(pulseSimple.resolve("pa_simple_new"));
        auto pa_read = reinterpret_cast<fn_pa_simple_read>(pulseSimple.resolve("pa_simple_read"));
        auto pa_free = reinterpret_cast<fn_pa_simple_free>(pulseSimple.resolve("pa_simple_free"));

        if (!pa_new || !pa_read || !pa_free) {
            // Fallback idle loop if library unavailable
            while (m_running) {
                msleep(100);
            }
            return;
        }

        pa_sample_spec ss;
        ss.format = 3; // PA_SAMPLE_S16LE
        ss.rate = 22050;
        ss.channels = 1;

        pa_simple *stream = nullptr;
        int err = 0;

        constexpr int CHUNK_SIZE = 256;
        int16_t buffer[CHUNK_SIZE];

        while (m_running) {
            {
                QMutexLocker locker(&m_mutex);
                while (m_running && !m_active) {
                    if (stream) {
                        pa_free(stream);
                        stream = nullptr;
                    }
                    m_wakeCondition.wait(&m_mutex, 200);
                }
            }

            if (!m_running) break;

            if (!stream) {
                // Connect to @DEFAULT_MONITOR@ (monitor of currently playing sink)
                err = 0;
                stream = pa_new(nullptr, "LeapK_Visualizer", 2 /* PA_STREAM_RECORD */, "@DEFAULT_MONITOR@", "audio_outline", &ss, nullptr, nullptr, &err);
                if (!stream) {
                    // Try fallback default sink monitor or default source
                    stream = pa_new(nullptr, "LeapK_Visualizer", 2, nullptr, "audio_outline", &ss, nullptr, nullptr, &err);
                }
                if (!stream) {
                    msleep(500); // Retry later
                    continue;
                }
            }

            err = 0;
            if (pa_read(stream, buffer, sizeof(buffer), &err) == 0) {
                QMutexLocker locker(&m_mutex);
                m_dsp.processChunk(buffer, CHUNK_SIZE, m_data, m_sensitivity);
                m_data.lastUpdateMs = m_clock.elapsed();
            } else {
                // Read error or server reconnect
                pa_free(stream);
                stream = nullptr;
                msleep(100);
            }
        }

        if (stream) {
            pa_free(stream);
            stream = nullptr;
        }
    }

private:
    QMutex m_mutex;
    QWaitCondition m_wakeCondition;
    bool m_running = true;
    bool m_active = false;
    qreal m_sensitivity = 1.0;
    AudioVisualizerData m_data;
    AudioDspEngine m_dsp;
    QElapsedTimer m_clock;
};

class AudioVisualizerManager {
public:
    static AudioVisualizerManager &instance() {
        static AudioVisualizerManager s_instance;
        return s_instance;
    }

    void start() {
        if (!m_thread) {
            m_thread = new AudioVisualizerThread();
            m_thread->start(QThread::LowPriority);
        }
    }

    void stop() {
        if (m_thread) {
            m_thread->stopThread();
            delete m_thread;
            m_thread = nullptr;
        }
    }

    void setActive(bool active) {
        if (m_thread) {
            m_thread->setActive(active);
        }
    }

    void setSensitivity(qreal sens) {
        if (m_thread) {
            m_thread->setSensitivity(sens);
        }
    }

    AudioVisualizerData data() const {
        return m_thread ? m_thread->currentData() : AudioVisualizerData();
    }

private:
    AudioVisualizerManager() = default;
    ~AudioVisualizerManager() {
        stop();
    }

    AudioVisualizerThread *m_thread = nullptr;
};

} // namespace OutlineAudioVisualizer

#endif // AUDIOVISUALIZER_H
