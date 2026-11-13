---
title: Sound
sidebar_position: 8
---

# Sound

`src/sound/` has two separate players. `SoundManager` plays `.VAG` samples, which is what you want for sound effects and short loops. `ModSoundManager` plays one `.MOD` tracker file as background music.

Both are members of `g_madnightEngine`: `m_soundManager` and `m_modSoundManager`.

:::caution[VAG samples and MOD music overwrite each other]
Both players upload their samples to the same place in SPU RAM. Loading a MOD file overwrites any VAG samples already uploaded, and VAG samples loaded after a hard load overwrite the MOD's instruments. Use one or the other in a scene, not both.
:::

## SoundManager

`src/sound/sound_manager.hh`

### Loading a sound effect

Put the `.VAG` file in a load queue and the loader uploads it to the SPU for you.

```cpp
eastl::vector<LoadQueue> files = {
    {.name = "SFX/JUMP.VAG", .type = LoadFileType::VAG},
};
co_await g_madnightEngine.HardLoadingScreen(eastl::move(files), &gameplayScene);
```

In a manifest the line is `vag SFX/JUMP.VAG` (see [SCENEBIN](../guides/scenebin#types)).

To load one yourself from a coroutine:

```cpp
VagEntry *jump = nullptr;
co_await g_madnightEngine.m_soundManager.LoadVAGFile("SFX/JUMP.VAG", &jump);
if (jump == nullptr) {
  // missing, not a valid VAG, out of SPU RAM, or all 24 slots are taken
}
```

Loading a name that is already loaded hands back the existing entry without reading the file again.

Later, look a sample up by the name you loaded it under. `IsVAGLoaded` returns `nullptr` if it is not loaded.

```cpp
VagEntry *jump = g_madnightEngine.m_soundManager.IsVAGLoaded("SFX/JUMP.VAG");
```

It also takes a `HashName` result, which saves hashing the name each time:

```cpp
constexpr uint64_t jumpHash = HashName("SFX/JUMP.VAG");
VagEntry *jump = g_madnightEngine.m_soundManager.IsVAGLoaded(jumpHash);
```

Limits:

- Up to 24 samples at once (`MAX_VAG_FILE_COUNT`). Past that, `LoadVAGFile` loads nothing and leaves your pointer as `nullptr`.
- All samples share the SPU's 512 KB of sound RAM. A sample that does not fit is not loaded.
- Samples are never freed one by one. A hard load (`HardLoadingScreen`) silences every channel and drops all of them at once.

### Playing a sound effect

Build a playback config for the sample, then play it on a channel.

```cpp
auto &sound = g_madnightEngine.m_soundManager;
VagEntry *jump = sound.IsVAGLoaded("SFX/JUMP.VAG");
sound.PlayVAGFile(jump, /*channelId*/ 4, sound.CreatePlaybackConfig(jump, 0x3FFF));
```

`CreatePlaybackConfig` fills in the sample's pitch for you from its sample rate. Pass one volume for both speakers, or a left and a right volume:

```cpp
auto pannedLeft = sound.CreatePlaybackConfig(jump, 0x3FFF, 0x1000);
```

0x3FFF is full volume. An optional last argument sets the ADSR envelope; it defaults to `SPU_ADR_INSTANT_ATTACK_NO_DECAY`.

`PlayVAGFile` also accepts the file name instead of the entry. It looks the sample up by name and does nothing if it is not loaded.

```cpp
sound.PlayVAGFile("SFX/JUMP.VAG", 4, config);
```

You choose the channel yourself, 0 to 23. There is no automatic channel picking, and playing on a busy channel replaces what was there. Channel numbers above 23 are treated as 23. Pass `true` as the last argument (`hardCut`) to key the channel off before the new sound starts, which is what you want when retriggering the same sound quickly.

Playing a `nullptr` entry, or one that failed to load, does nothing.

### Silencing channels

`SilenceChannels` takes a bit mask, one bit per channel.

```cpp
g_madnightEngine.m_soundManager.SilenceChannels(1 << 4);         // channel 4
g_madnightEngine.m_soundManager.SilenceChannels(0xffffffff);     // every channel
```

Use it before switching scenes if you have a looping sample playing, since a soft load does not stop anything.

## ModSoundManager

`src/sound/mod_sound_manager.hh`

### Loading and playing music

`ModSoundManager` plays one `.MOD` file at a time. Load it with a `MOD_FILE` queue entry (`mod_file` in a manifest), or yourself:

```cpp
ModSoundFile *track = nullptr;
co_await g_madnightEngine.m_modSoundManager.LoadMODSound("MUSIC/LEVEL01.MOD", &track);
if (track != nullptr) {
  g_madnightEngine.m_modSoundManager.PlayMusic(DEFAULT_MUSIC_VOLUME);
}
```

Always pass a real pointer as the second argument. It is set to `nullptr` if loading fails. `CurrentMODSoundFile()` returns the last file that loaded.

Loading another MOD stops the current music and replaces it; there is no unload call. A hard load does not unload music.

### Controlling playback

```cpp
auto &music = g_madnightEngine.m_modSoundManager;
music.PauseMusic();             // stop the timer and mute
music.PlayMusic();              // carry on at the last volume you set
music.SetMusicVolume(8192);     // change volume while playing
music.StopMusic();              // cut every voice off
```

Volume runs from 0 to `MAX_MUSIC_VOLUME` (65535). `DEFAULT_MUSIC_VOLUME` is 16384.

All of these do nothing until a MOD file has loaded, including `SetMusicVolume`, so you cannot set the volume ahead of loading.

Limits:

- Calling `PlayMusic` while the music is already playing restarts its update timer and sets the volume, it does not start a second copy. Use `SetMusicVolume` to change volume without touching the timer.
- `PlayMusic()` with no argument uses the last volume you set, which starts at `DEFAULT_MUSIC_VOLUME`.
- Neither `PauseMusic` nor `StopMusic` rewinds the track. To start a track from the top, load it again.
- The music uses the low SPU channels, one per MOD channel (0 to 3 for a 4-channel MOD). Keep your own channel choices clear of them.

### Playing the MOD's instruments as effects

`PlaySoundEffect` and `PlayNote` play one of the loaded MOD's own samples on a channel.

```cpp
// channel 3, instrument 5, note 12, volume 63
g_madnightEngine.m_modSoundManager.PlaySoundEffect(3, 5, 12, 63);
```

The third argument is a note number from 0 to 35 (three octaves of 12), even though `PlaySoundEffect` names it `pitch`. Volume is 0 to 63. `PlaySoundEffect` ignores the music volume; `PlayNote` is scaled by it.
