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

static void testDetailedPactlParsing()
{
    const QString sample = QStringLiteral(
        "Sink Input #123\n"
        "\tCorked: no\n"
        "\tMute: no\n"
        "\tProperties:\n"
        "\t\tapplication.process.id = \"12562\"\n"
        "\t\tapplication.name = \"Firefox\"\n"
        "\t\tmedia.name = \"C Programming Full Course for free ⚙️ - YouTube\"\n"
    );

    const auto streams = OutlineAudio::parsePactlSinkInputsDetailed(sample);
    assert(streams.size() == 1);
    assert(streams[0].pid == 12562);
    assert(streams[0].appName == QStringLiteral("Firefox"));
    assert(streams[0].title == QStringLiteral("C Programming Full Course for free ⚙️ - YouTube"));
}

static void testMediaTitleMatching()
{
    const QString mediaTitle = QStringLiteral("C Programming Full Course for free ⚙️ - YouTube");
    const QString winFirefox1 = QStringLiteral("C Programming Full Course for free ⚙️ - YouTube — Mozilla Firefox");
    const QString winFirefox2 = QStringLiteral("GitHub - kde/kwin: An easy to use, but flexible window manager — Mozilla Firefox");
    const QString winChromium = QStringLiteral("C Programming Full Course for free ⚙️ - YouTube - Chromium");

    assert(OutlineAudio::mediaTitleMatchesCaption(mediaTitle, winFirefox1));
    assert(OutlineAudio::mediaTitleMatchesCaption(mediaTitle, winChromium));
    assert(!OutlineAudio::mediaTitleMatchesCaption(mediaTitle, winFirefox2));

    // Test MPRIS title without YouTube suffix matching YouTube window title
    const QString mprisTitle = QStringLiteral("C Programming Full Course for free ⚙️");
    assert(OutlineAudio::mediaTitleMatchesCaption(mprisTitle, winFirefox1));
    assert(!OutlineAudio::mediaTitleMatchesCaption(mprisTitle, winFirefox2));
}

static void testMultiWindowDisambiguation()
{
    OutlineAudio::AudioTracker tracker;

    // Both windows share the same PID 12562 (standard Firefox multi-window behavior)
    const pid_t firefoxPid = 12562;
    const QString winPlaying = QStringLiteral("C Programming Full Course for free ⚙️ - YouTube — Mozilla Firefox");
    const QString winSilent = QStringLiteral("KWin Scripting Tutorial — Mozilla Firefox");

    QList<QPair<pid_t, QString>> knownWindows = {
        {firefoxPid, winPlaying},
        {firefoxPid, winSilent}
    };

    QList<OutlineAudio::AudioStream> streams = {
        {firefoxPid, QStringLiteral("C Programming Full Course for free ⚙️ - YouTube"), QStringLiteral("Firefox")}
    };

    tracker.setMockStreams(streams, knownWindows);

    // The window playing audio MUST return true
    assert(tracker.isWindowPlaying(firefoxPid, winPlaying));

    // The other window of the same PID MUST return false!
    assert(!tracker.isWindowPlaying(firefoxPid, winSilent));

    // Fallback: If caption is empty or unknown window, behavior remains safe
    assert(tracker.isWindowPlaying(firefoxPid, QString()));

    // Fallback: If audio stream title is generic ("playback"), both windows return true
    QList<OutlineAudio::AudioStream> genericStreams = {
        {firefoxPid, QStringLiteral("playback"), QStringLiteral("Firefox")}
    };
    tracker.setMockStreams(genericStreams, knownWindows);
    assert(tracker.isWindowPlaying(firefoxPid, winPlaying));
    assert(tracker.isWindowPlaying(firefoxPid, winSilent));
}

int main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    testPactlParsing();
    testPidMatching();
    testAudioTrackerMock();
    testDetailedPactlParsing();
    testMediaTitleMatching();
    testMultiWindowDisambiguation();

    std::cout << "All audiotracker tests passed successfully." << std::endl;
    return 0;
}
