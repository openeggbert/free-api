#pragma once

/**
 * @file MidiFixtures.hpp
 * @brief Shared MIDI test-fixture helpers, header-only, test-infrastructure
 * only (never linked into free-api itself).
 *
 * TASK-0004 (maintainability sweep, 2026-07-09): WriteMinimalMidi() used to
 * be independently hand-copied, byte-for-byte identical, into
 * tests/basic_test.cpp, tests/test_mci_sequences.cpp, and
 * tests/test_midi_soundfont_rendering.cpp -- confirmed via diff, not
 * assumed. A future change to the fixture shape could easily be applied to
 * one or two copies and silently miss the third. Consolidated here so
 * there is exactly one copy to keep in sync.
 */

#include <cstdio>
#include <string>

// A tiny, valid, single-note Type-0 MIDI file -- long enough to render at
// least one mixer block, short enough to finish (and fire MM_MCINOTIFY)
// quickly in a test. Program-change 0, note-on (key 0x3C/60, velocity
// 0x40/64), note-off, end-of-track.
inline bool WriteMinimalMidi(const std::string& path)
{
    static const unsigned char kMidi[] = {
        'M', 'T', 'h', 'd',
        0x00, 0x00, 0x00, 0x06,
        0x00, 0x00,
        0x00, 0x01,
        0x00, 0x60,
        'M', 'T', 'r', 'k',
        0x00, 0x00, 0x00, 0x0F,
        0x00, 0xC0, 0x00,
        0x00, 0x90, 0x3C, 0x40,
        0x60, 0x80, 0x3C, 0x00,
        0x00, 0xFF, 0x2F, 0x00,
    };

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }

    const size_t written = fwrite(kMidi, 1, sizeof(kMidi), f);
    fclose(f);
    return written == sizeof(kMidi);
}
