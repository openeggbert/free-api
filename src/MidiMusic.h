/**
 * @file MidiMusic.h
 * @brief Internal forward declarations for the MIDI/MCI music backend.
 *
 * These functions are implemented in MidiMusic.cpp and called from winapi.cpp.
 * Do not include this header in public API headers.
 *
 * @note Status: PARTIAL
 */
#ifndef FREE_API_MIDI_MUSIC_H
#define FREE_API_MIDI_MUSIC_H

#include "mmsystem.h"
#include "windows.h"

/* midiOut wrappers */
UINT     MidiMusicGetNumDevs();
MMRESULT MidiMusicOutOpen(LPHMIDIOUT phmo);
MMRESULT MidiMusicSetVolume(DWORD dwVolume);
MMRESULT MidiMusicOutClose();

/* MCI sequencer */
MCIERROR MidiMusicSendCommand(MCIDEVICEID mciId, UINT uMsg,
                               DWORD_PTR fdwCommand, DWORD_PTR dwParam);
BOOL     MidiMusicGetErrorString(MCIERROR mcierr, LPSTR pszText, UINT cchText);

#endif /* FREE_API_MIDI_MUSIC_H */
