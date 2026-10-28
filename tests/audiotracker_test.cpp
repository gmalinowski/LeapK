// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audiotracker.h"
#include <cassert>
#include <iostream>

static void testPactlParsing()
{
    const QString samplePactl = QStringLiteral(
        "Sink Input #1080\n"
        "\tDriver: PipeWire\n"
        "\tCorked: no\n"
        "\tMute: no\n"
        "\tProperties:\n"
        "\t\tapplication.process.id = \"38327\"\n"
        "\t\tapplication.name = \"speech-dispatcher-generic\"\n"
        "\n"
        "Sink Input #3604\n"
        "\tDriver: PipeWire\n"
        "\tCorked: yes\n"
        "\tMute: no\n"
        "\tProperties:\n"
        "\t\tapplication.process.id = \"12562\"\n"
        "\t\tapplication.name = \"Firefox\"\n"
        "\n"
        "Sink Input #5000\n"
        "\tDriver: PipeWire\n"
        "\tCorked: no\n"
        "\tMute: yes\n"
        "\tProperties:\n"
        "\t\tapplication.process.id = \"9999\"\n"
        "\t\tapplication.name = \"MutedPlayer\"\n"
        "\n"
        "Sink Input #6000\n"
        "\tDriver: PipeWire\n"
        "\tCorked: no\n"
        "\tMute: no\n"
        "\tProperties:\n"
        "\t\tapplication.process.id = \"4455\"\n"
        "\t\tapplication.name = \"Spotify\"\n"
    );

    const QSet<pid_t> pids = OutlineAudio::parsePactlSinkInputs(samplePactl);
    assert(pids.size() == 2);
    assert(pids.contains(38327)); // Uncorked & unmuted
    assert(pids.contains(4455));  // Uncorked & unmuted
    assert(!pids.contains(12562)); // Corked
    assert(!pids.contains(9999));  // Muted
}

static void testPidMatching()
{
    QSet<pid_t> playingPids;
    playingPids.insert(12345);
    playingPids.insert(67890);

    // Direct match
    assert(OutlineAudio::isWindowPidPlaying(12345, playingPids));
    assert(OutlineAudio::isWindowPidPlaying(67890, playingPids));
    assert(!OutlineAudio::isWindowPidPlaying(99999, playingPids));
    assert(!OutlineAudio::isWindowPidPlaying(0, playingPids));
    assert(!OutlineAudio::isWindowPidPlaying(-1, playingPids));
}

static void testAudioTrackerMock()
{
    OutlineAudio::AudioTracker tracker;
    assert(!tracker.hasPlayingAudio());

    QSet<pid_t> mock;
    mock.insert(5555);
    tracker.setMockPids(mock);

    assert(tracker.hasPlayingAudio());
    assert(tracker.isWindowPlaying(5555));
    assert(!tracker.isWindowPlaying(6666));
}

int main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    testPactlParsing();
    testPidMatching();
    testAudioTrackerMock();

    std::cout << "All audiotracker tests passed successfully." << std::endl;
    return 0;
}
