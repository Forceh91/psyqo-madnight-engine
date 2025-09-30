#include "sound_manager.hh"
#include "psyqo/xprintf.h"
#include "../helpers/cdrom.hh"
#include "../render/renderer.hh"

ModSoundFile SoundManager::m_currentSoundFile = {"", 0, false};
unsigned SoundManager::m_musicTimer = 0;
uint16_t SoundManager::m_musicVolume = DEFAULT_MUSIC_VOLUME;

psyqo::Coroutine<> SoundManager::LoadMODSoundFromCDRom(const char *modSoundFileName, ModSoundFile **modSoundFileOut)
{
    *modSoundFileOut = nullptr;

    // ok checks passed, get it off the CD
    auto buffer = co_await CDRomHelper::LoadFile(modSoundFileName);
    void *data = buffer.data();
    size_t size = buffer.size();

    if (data == nullptr || size == 0)
    {
        printf("SOUND: Failed to load MOD file or it has no file size.\n");
        buffer.clear();
        co_return;
    }

    ModSoundFile soundFile = {modSoundFileName, 0, false};

    // load the data into the SPU
    soundFile.size = MOD_Load((MODFileFormat *)data);
    if (soundFile.size == 0)
        co_return;

    // loaded
    soundFile.isLoaded = true;

    // put into our array/library/whatever you wanna call it
    m_currentSoundFile = soundFile;
    *modSoundFileOut = &m_currentSoundFile;

    // done with the data, clear it
    buffer.clear();

    printf("SOUND: Successfully loaded MOD file of %d bytes into SPU.\n", size);
}

void SoundManager::PlaySoundEffect(uint32_t channel, uint32_t sampleID, int32_t pitch, uint32_t volume)
{
    if (!m_currentSoundFile.isLoaded)
        return;
    MOD_PlaySoundEffect(channel, sampleID, pitch, volume);
}

void SoundManager::PlayNote(uint32_t voiceID, uint32_t sampleID, uint32_t note, int16_t volume)
{
    if (!m_currentSoundFile.isLoaded)
        return;

    MOD_PlayNote(voiceID, sampleID, note, volume);
}

void SoundManager::PlayMusic(void)
{
    if (!m_currentSoundFile.isLoaded)
        return;

    PlayMusic(m_musicVolume);
}

void SoundManager::PlayMusic(uint16_t volume)
{
    if (!m_currentSoundFile.isLoaded)
        return;

    SetMusicVolume(volume);

    // this will start playing the music in the mod file that was put into the SPU
    auto &gpu = Renderer::Instance().GPU();
    m_musicTimer = gpu.armPeriodicTimer(MOD_hblanks * psyqo::GPU::US_PER_HBLANK, [&](uint32_t)
                                        {
        MOD_Poll();

        // There is no downside in changing the timer every time, in case the
        // mod player wants to change the timing.
        gpu.changeTimerPeriod(m_musicTimer, MOD_hblanks * psyqo::GPU::US_PER_HBLANK); });
}

void SoundManager::PauseMusic(void)
{
    if (!m_currentSoundFile.isLoaded)
        return;

    auto &gpu = Renderer::Instance().GPU();
    gpu.cancelTimer(m_musicTimer);
    MOD_SetMusicVolume(0);
}

void SoundManager::StopMusic(void)
{
    if (!m_currentSoundFile.isLoaded)
        return;

    auto &gpu = Renderer::Instance().GPU();
    gpu.cancelTimer(m_musicTimer);
    MOD_Silence();
}

void SoundManager::SetMusicVolume(uint16_t volume)
{
    if (!m_currentSoundFile.isLoaded)
        return;

    m_musicVolume = volume;
    MOD_SetMusicVolume(m_musicVolume);
}
