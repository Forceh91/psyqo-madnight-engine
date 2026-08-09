---
title: Sound
sidebar_position: 8
---

# Sound

`src/sound/` — two independent systems: `SoundManager` for one-shot/looping VAG samples over the SPU's 24 channels, and `ModSoundManager` for MOD-tracker background music.

## SoundManager

`src/sound/sound_manager.hh`

Loads `.VAG` samples into SPU RAM and plays them back on any of the SPU's channels (`MAX_VAG_FILE_COUNT` = 24, matching hardware channel count).

```cpp
static constexpr uint8_t VAG_FILE_NAME_LEN = 16;
static constexpr uint8_t MAX_VAG_FILE_COUNT = 24;
static constexpr int8_t INVALID_VAG_FILE_ID = -1;
static constexpr uint32_t SPU_NOMINAL_PITCH = 4096;
static constexpr uint32_t SPU_MEMORY_SIZE = 0x80000;
static constexpr uint32_t SPU_ADR_INSTANT_ATTACK_NO_DECAY = 0x80000000;
static constexpr uint8_t SPU_MAX_CHANNEL_ID = 23;

struct VagEntry {
  int8_t id = INVALID_VAG_FILE_ID;
  eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> name; // the name supplied for the CD-ROM load, not the header's
  uint32_t spuAddr; // where it lives in SPU RAM
  uint32_t pitch;   // precomputed from sample rate
  uint32_t size;    // SPU RAM footprint
};

class SoundManager final {
public:
  static void Init(void); // called automatically by the engine

  static void Dump(void); // resets the SPU alloc pointer, doesn't clear SPU contents
  static psyqo::Coroutine<> LoadVAGFile(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName, VagEntry** out);
  static VagEntry* IsVAGLoaded(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName);
  static VagEntry* IsVAGLoaded(const uint8_t& fileName);
  static void SilenceChannels(const uint32_t channels);

  static void PlayVAGFile(const VagEntry* vag, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut = false);
  static void PlayVAGFile(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut = false);
  static void PlayVAGFile(const uint8_t& vagID, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut = false);

  static psyqo::SPU::ChannelPlaybackConfig CreatePlaybackConfig(const VagEntry* vag, uint16_t volume, uint32_t adsr = SPU_ADR_INSTANT_ATTACK_NO_DECAY);
  static psyqo::SPU::ChannelPlaybackConfig CreatePlaybackConfig(const VagEntry* vag, uint16_t volumeL, uint16_t volumeR, uint32_t adsr = SPU_ADR_INSTANT_ATTACK_NO_DECAY);
};
```

Typical flow: `LoadVAGFile` once at load time, `CreatePlaybackConfig` to build a channel config for it (mono or stereo volume), then `PlayVAGFile` on whichever channel you want it to occupy. `hardCut = true` cuts the sample off immediately rather than releasing it naturally — useful when you need a channel back right away.

## ModSoundManager

`src/sound/mod_sound_manager.hh`

Plays `.MOD`-format tracker music. The underlying `modplayer` only holds **one** MOD file in the SPU at a time — loading a new one resets everything and replaces it.

```cpp
static constexpr uint16_t MAX_MUSIC_VOLUME = 65535;
static constexpr uint16_t DEFAULT_MUSIC_VOLUME = 16384;

struct ModSoundFile {
  eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN> name;
  uint32_t size;
  bool isLoaded;
};

class ModSoundManager final {
public:
  // Finds a .MOD file on the CD-ROM (dir/name.ext) and loads it directly into the SPU.
  // The SPU only has 512K, so it's on you to manage memory sensibly.
  static psyqo::Coroutine<> LoadMODSound(const char *modSoundFileName, ModSoundFile **modSoundFileOut);
  static const ModSoundFile *CurrentMODSoundFile(void);

  static void PlaySoundEffect(uint32_t channel, uint32_t sampleID, int32_t pitch, uint32_t volume);
  static void PlayNote(uint32_t voiceID, uint32_t sampleID, uint32_t note, int16_t volume);

  static void PlayMusic(void);              // resumes at the last-set volume
  static void PlayMusic(uint16_t volume);   // plays and sets volume in one call
  static void PauseMusic(void);
  static void StopMusic(void);              // stops completely
  static void SetMusicVolume(uint16_t volume);
};
```

To switch tracks, just call `LoadMODSound` again with the new file — no explicit unload step is needed.
