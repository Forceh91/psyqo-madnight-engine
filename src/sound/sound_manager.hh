#ifndef _SOUND_MANAGER_H
#define _SOUND_MANAGER_H

#include <EASTL/array.h>
#include "sound.hh"
#include "psyqo/coroutine.hh"

class SoundManager final
{
    static ModSoundFile m_currentSoundFile;
    static unsigned m_musicTimer;

public:
    // this will find a .MOD format file on the CD ROM, given the dir/name.ext format, and load it directly into the SPU
    // the modplayer is designed in a such a way where you can have one .mod file in the SPU at once
    // thereforce calling this again when a .mod file is already in the SPU will reset everything, and
    // the newly loaded file will become the only file in the SPU
    // as the SPU only contains 512K memory it is up to you to manage memory properly
    // this will give back some basic info about the loaded file incase you feel it is relevant
    // if it comes back as nullptr then something probably went wrong
    static psyqo::Coroutine<> LoadMODSoundFromCDRom(const char *modSoundFileName, ModSoundFile **modSoundFileOut);
    // not really important but added for convenience
    static const ModSoundFile *CurrentMODSoundFile(void) { return &m_currentSoundFile; }

    // see `MOD_PlaySoundEffect` on the best way to use this
    static void PlaySoundEffect(uint32_t channel, uint32_t sampleID, int32_t pitch, uint32_t volume);
    // see `MOD_PlayNote` on the best way to use this
    static void PlayNote(uint32_t voiceID, uint32_t sampleID, uint32_t note, int16_t volume);
    // plays the music in the MOD file using GPU timers/`MOD_Poll`
    static void PlayMusic();
    static void PlayMusic(uint32_t volume);
    // pauses the music in the MOD file. if you want to switch to a new track then you simply just `LoadMODSoundFromCDRom` again
    static void PauseMusic();
    // see `MOD_SetMusicVolme` on the best way to use this
    static void SetMusicVolume(uint32_t volume);
};

#endif
