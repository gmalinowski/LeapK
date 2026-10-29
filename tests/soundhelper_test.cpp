#include "soundhelper.h"
#include <QCoreApplication>
#include <cassert>
#include <iostream>

void testWavGeneration()
{
    std::cout << "[TEST] Running testWavGeneration..." << std::endl;

    QByteArray enterWav = OutlineSound::generateWavData(OutlineSound::SoundEffect::EnterJump);
    assert(enterWav.size() > 44);
    assert(enterWav.startsWith("RIFF"));
    assert(enterWav.mid(8, 8) == "WAVEfmt ");

    // Verify subchunk1size == 16 (PCM)
    const quint32 *subchunk1Size = reinterpret_cast<const quint32 *>(enterWav.constData() + 16);
    assert(*subchunk1Size == 16);

    // Verify format == 1 (PCM), channels == 1
    const quint16 *format = reinterpret_cast<const quint16 *>(enterWav.constData() + 20);
    assert(*format == 1);
    const quint16 *channels = reinterpret_cast<const quint16 *>(enterWav.constData() + 22);
    assert(*channels == 1);

    // Verify sample rate == 44100
    const quint32 *sampleRate = reinterpret_cast<const quint32 *>(enterWav.constData() + 24);
    assert(*sampleRate == 44100);

    QByteArray landWav = OutlineSound::generateWavData(OutlineSound::SoundEffect::LandWindow);
    assert(landWav.size() > 44);
    assert(landWav.startsWith("RIFF"));
    assert(landWav.mid(8, 8) == "WAVEfmt ");

    std::cout << "[PASS] testWavGeneration passed." << std::endl;
}

void testSoundFilesExistAndPlayer()
{
    std::cout << "[TEST] Running testSoundFilesExistAndPlayer..." << std::endl;

    bool ok = OutlineSound::ensureSoundFilesExist();
    assert(ok);

    const QString enterPath = OutlineSound::soundFilePath(OutlineSound::SoundEffect::EnterJump);
    const QString landPath = OutlineSound::soundFilePath(OutlineSound::SoundEffect::LandWindow);
    assert(QFile::exists(enterPath));
    assert(QFile::exists(landPath));

    QString player = OutlineSound::detectAudioPlayer();
    std::cout << "Detected audio player: " << player.toStdString() << std::endl;
    assert(!player.isEmpty());

    // Test playSound call with 0% (noop) and 20%
    OutlineSound::playSound(OutlineSound::SoundEffect::EnterJump, 0);
    OutlineSound::playSound(OutlineSound::SoundEffect::EnterJump, 20);

    std::cout << "[PASS] testSoundFilesExistAndPlayer passed." << std::endl;
}

void testProfilesAndNewEffects()
{
    std::cout << "[TEST] Running testProfilesAndNewEffects..." << std::endl;

    const OutlineSound::SoundEffect effects[] = {
        OutlineSound::SoundEffect::EnterJump,
        OutlineSound::SoundEffect::LandWindow,
        OutlineSound::SoundEffect::SwitchWorkspace,
        OutlineSound::SoundEffect::NavTab,
        OutlineSound::SoundEffect::Dismiss,
        OutlineSound::SoundEffect::Stash
    };

    const OutlineSound::SoundProfile profiles[] = {
        OutlineSound::SoundProfile::CrispClick,
        OutlineSound::SoundProfile::SoftPop,
        OutlineSound::SoundProfile::SciFiBlip,
        OutlineSound::SoundProfile::Mechanical
    };

    for (auto p : profiles) {
        for (auto e : effects) {
            QByteArray pcm = OutlineSound::generatePcmData(e, p, 50);
            assert(!pcm.isEmpty());
            assert(pcm.size() % 2 == 0);

            QByteArray wav = OutlineSound::generateWavData(e, p);
            assert(wav.size() > 44);
            assert(wav.startsWith("RIFF"));

            const QString path = OutlineSound::soundFilePath(e, p);
            assert(!path.isEmpty());

            OutlineSound::playSound(e, 30, p);
        }
    }

    assert(OutlineSound::profileFromString(QStringLiteral("soft_pop")) == OutlineSound::SoundProfile::SoftPop);
    assert(OutlineSound::profileFromString(QStringLiteral("scifi_blip")) == OutlineSound::SoundProfile::SciFiBlip);
    assert(OutlineSound::profileFromString(QStringLiteral("mechanical")) == OutlineSound::SoundProfile::Mechanical);
    assert(OutlineSound::profileFromString(QStringLiteral("crisp_click")) == OutlineSound::SoundProfile::CrispClick);

    std::cout << "[PASS] testProfilesAndNewEffects passed." << std::endl;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testWavGeneration();
    testSoundFilesExistAndPlayer();
    testProfilesAndNewEffects();

    std::cout << "\nALL SOUND HELPER TESTS PASSED (100%)\n" << std::endl;
    return 0;
}
