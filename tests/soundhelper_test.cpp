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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    testWavGeneration();
    testSoundFilesExistAndPlayer();

    std::cout << "\nALL SOUND HELPER TESTS PASSED (100%)\n" << std::endl;
    return 0;
}
