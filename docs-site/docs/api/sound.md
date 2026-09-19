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
  uint64_t nameHash; // hash of the name supplied for the CD-ROM load, not the header's
  uint32_t spuAddr; // where it lives in SPU RAM
  uint32_t pitch;   // precomputed from sample rate
  uint32_t size;    // SPU RAM footprint
};

class SoundManager final {
public:
  void Init(void); // called automatically by the engine

  void Dump(void); // resets the SPU alloc pointer, doesn't clear SPU contents
  psyqo::Coroutine<> LoadVAGFile(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName, VagEntry** out);
  VagEntry* IsVAGLoaded(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName);
  VagEntry* IsVAGLoaded(uint64_t nameHash);
  VagEntry* IsVAGLoaded(const uint8_t& id);
  void SilenceChannels(const uint32_t channels);

  void PlayVAGFile(const VagEntry* vag, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut = false);
  void PlayVAGFile(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut = false);
  void PlayVAGFile(const uint8_t& vagID, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut = false);

  psyqo::SPU::ChannelPlaybackConfig CreatePlaybackConfig(const VagEntry* vag, uint16_t volume, uint32_t adsr = SPU_ADR_INSTANT_ATTACK_NO_DECAY);
  psyqo::SPU::ChannelPlaybackConfig CreatePlaybackConfig(const VagEntry* vag, uint16_t volumeL, uint16_t volumeR, uint32_t adsr = SPU_ADR_INSTANT_ATTACK_NO_DECAY);
};
```

Non-`static` member of `MadnightEngine` (`g_madnightEngine.m_soundManager`). `VagEntry` now stores a hashed `nameHash` instead of the full name string, and `IsVAGLoaded` gained a `uint64_t nameHash` overload alongside the existing by-name and by-id ones — precompute the hash with `HashName()` if you're checking the same name repeatedly.

Typical flow: `LoadVAGFile` once at load time, `CreatePlaybackConfig` to build a channel config for it (mono or stereo volume), then `PlayVAGFile` on whichever channel you want it to occupy. `hardCut = true` cuts the sample off immediately rather than releasing it naturally — useful when you need a channel back right away.

### Usage

```cpp
VagEntry *jumpSfx;
co_await g_madnightEngine.m_soundManager.LoadVAGFile("jump.vag", &jumpSfx);

auto config = g_madnightEngine.m_soundManager.CreatePlaybackConfig(jumpSfx, /*volume*/ 0x3FFF);
g_madnightEngine.m_soundManager.PlayVAGFile(jumpSfx, /*channelId*/ 0, config);
```

Checking whether a looping music/ambience sample is already loaded (e.g. preloaded during a level's asset manifest) before playing it, then silencing everything before switching scenes:

```cpp
auto vag = g_madnightEngine.m_soundManager.IsVAGLoaded("SFX/FCNTNA.VAG"); // nullptr if not loaded
if (vag) {
    auto volume = saveData->GetMusicVolumeSPU();
    auto &soundManager = g_madnightEngine.m_soundManager;
    soundManager.PlayVAGFile(vag, SPU_MAX_CHANNEL_ID, soundManager.CreatePlaybackConfig(vag, volume));
}

// later, e.g. right before switching scenes:
g_madnightEngine.m_soundManager.SilenceChannels(1 << SPU_MAX_CHANNEL_ID); // bitmask, one bit per channel
```

### Internals

- SPU memory allocation is a simple bump pointer (`m_spuAllocPtr`) that only ever grows. `Dump()` resets the pointer and clears the loaded-file list, but doesn't erase the sample bytes already written to SPU RAM. It also calls `psyqo::SPU::silenceChannels(0xffffffff)` first, stopping whatever is currently playing on every channel.
- `channelId` is silently clamped to `SPU_MAX_CHANNEL_ID` (23), so passing an out-of-range channel doesn't crash, it just reuses the last channel.
- `m_vagFiles` is `eastl::fixed_vector<VagEntry, MAX_VAG_FILE_COUNT>` with the overflow template argument omitted, which defaults to `true`. A 25th `LoadVAGFile` call silently heap-allocates past the documented 24-entry ceiling instead of failing there, unlike the UI's fixed vectors in [`GameplayHUD`](./ui#gameplayhud) and [`Menu`](./ui#menu), which explicitly disable overflow.

## ModSoundManager

`src/sound/mod_sound_manager.hh`

Plays `.MOD`-format tracker music. The underlying `modplayer` only holds **one** MOD file in the SPU at a time: loading a new one replaces it, but not literally everything. Music volume persists across the swap: `MOD_SetMusicVolume` is documented as surviving a subsequent `MOD_Load`, and `LoadMODSound` never touches `m_musicVolume`.

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
  psyqo::Coroutine<> LoadMODSound(const eastl::string_view &modSoundFileName, ModSoundFile **modSoundFileOut);
  const ModSoundFile *CurrentMODSoundFile(void);

  void PlaySoundEffect(uint32_t channel, uint32_t sampleID, int32_t pitch, uint32_t volume);
  void PlayNote(uint32_t voiceID, uint32_t sampleID, uint32_t note, int16_t volume);

  void PlayMusic(void);              // resumes at the last-set volume
  void PlayMusic(uint16_t volume);   // plays and sets volume in one call
  void PauseMusic(void);
  void StopMusic(void);              // stops completely
  void SetMusicVolume(uint16_t volume);
};
```

Non-`static` member of `MadnightEngine` (`g_madnightEngine.m_modSoundManager`). To switch tracks, just call `LoadMODSound` again with the new file — no explicit unload step is needed.

### Usage

```cpp
ModSoundFile *track;
co_await g_madnightEngine.m_modSoundManager.LoadMODSound("level01.mod", &track);
g_madnightEngine.m_modSoundManager.PlayMusic(20000);
```
