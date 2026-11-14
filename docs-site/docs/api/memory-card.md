---
title: Memory Card
sidebar_position: 12
---

# Memory Card

`src/memory_card/memory_card.hh`

`MemoryCard` is a thin wrapper over psyqo's `MemoryCardFileSystem`. It checks which ports have a card, reads and writes save files, and counts free blocks. Files it writes are normal BIOS saves: they show up in the BIOS memory card manager with their title and icon, and can be copied or deleted there.

It is a member of `g_madnightEngine`: `m_memoryCardHelper`. The header also adds two short type names that the examples below use:

```cpp
using MC = psyqo::MemoryCard;              // MC::Port, MC::Error
using MCFS = psyqo::MemoryCardFileSystem;  // MCFS::Icon
```

The engine calls `Prepare()` during start-up, before your `InitialLoad` runs, so you never call it yourself.

## Ports and errors

Every call that talks to one card takes an `MC::Port`: `MC::Port::Port0` or `MC::Port::Port1`. Most calls report back with an `MC::Error`. The ones you are likely to see:

| Error | Meaning |
| --- | --- |
| `OK` | It worked. |
| `NoCard` | No card in that port. |
| `NotFormatted` | A card is there but it does not have a valid Sony filesystem. |
| `FileNotFound` | `ReadFile` asked for a name that is not on the card. |
| `NameTooLong` | `WriteFile` was given a file name over 20 characters. |
| `FileTooLarge` | The file would need more than 15 blocks. |
| `OutOfSpace` | Not enough free blocks on the card for the file. |
| `BadChecksum`, `Timeout`, `ProtocolError`, ... | The transfer itself failed. |

`MC::errorMessage(err)` gives you a readable string for any of them.

## Blocking and callback calls

Most calls come in two forms:

- **Blocking**: returns the `MC::Error` directly. It keeps pumping the GPU's callbacks until the card operation finishes, but your scene's `frame()` does not run in the meantime. A read or write of a real save takes a few hundred milliseconds, so use these at a deliberate save point, for example behind a "Saving..." message.
- **Callback**: takes an `eastl::function<void(MC::Error)>` as its last argument and returns straight away. The game keeps running and the callback is called from the main loop once the operation is done.

Only one card operation can run at a time, blocking or not. Every call on this page apart from `IsCardInPort` and `GetCardsInPorts` is one. Starting another while a callback call is still in progress trips psyqo's assert ("an operation is already in flight"), so wait for the callback before you start the next one.

While an operation runs, the memory card has the controller bus to itself, so `AdvancedPad` input is not read until it finishes.

## Checking for a card

```cpp
auto &card = g_madnightEngine.m_memoryCardHelper;

if (card.IsCardInPort(MC::Port::Port0)) {
  // something answered in slot 1
}

uint8_t ports = card.GetCardsInPorts();
if (ports & CardPort::Port1) {
  // slot 2 has a card
}
```

`IsCardInPort` (blocking) returns `true` if a card answers on that port. It does not check that the card is formatted.

`GetCardsInPorts` (blocking) checks both ports and returns a bitmask. `CardPort::Port0` and `CardPort::Port1` are the bits to test. These are only for the bitmask: every other call takes an `MC::Port`.

To find out whether a card is usable, ask for its state:

```cpp
MC::Error state = card.GetCardState(MC::Port::Port0);
if (state == MC::Error::NotFormatted) {
  // offer to format it, see "Anything else" below
}
```

`GetCardState` returns `OK` for a formatted card, `NoCard` if nothing is there, and `NotFormatted` if a card is there without a Sony filesystem. The callback form is the same check without blocking:

```cpp
card.GetCardState(MC::Port::Port0, [](MC::Error state) {
  // state is OK, NoCard, NotFormatted or a transfer error
});
```

## File names

Each save on a card has a name of up to 20 characters. You can give the name in two ways:

- Call `SetFileName(name)` once, then use the `ReadFile`, `WriteFile` and `DoesSaveFileExist` overloads that do not take a name.
- Pass the name straight to the overloads that take one. These store the name as if you had called `SetFileName`, so later calls without a name use it too.

```cpp
card.SetFileName("BESLES-00000MYGAME");
```

The engine does not check what the name looks like, only `WriteFile` checks its length.

:::caution[The name is shared by every call]
There is one stored name. The callback forms of `ReadFile` and `WriteFile` read it while they run, so calling `SetFileName`, or any overload that takes a name, before their callback fires changes the name the in-flight operation uses. Wait for the callback first.
:::

## Checking for a save

```cpp
bool exists = false;
MC::Error err = card.DoesSaveFileExist(MC::Port::Port0, "BESLES-00000MYGAME", &exists);
if (err == MC::Error::OK && !exists) {
  // new game
}
```

`DoesSaveFileExist` (blocking only) sets `*outFileExists`. A missing file is not an error: you get `OK` and `false`. The overload without a name uses the stored one.

## Free space

A card has 15 blocks of 8 KB for saves.

```cpp
uint32_t freeBlocks = 0;
if (card.GetFreeBlockCount(MC::Port::Port0, &freeBlocks) == MC::Error::OK && freeBlocks == 0) {
  // card is full
}
```

`GetFreeBlockCount` sets `*outFreeBlocks` to 0 to 15. Some third-party 64 KB cards only count up to 7. There is also a callback form, `GetFreeBlockCount(port, &freeBlocks, callback)`. With that form, `freeBlocks` has to stay alive until the callback is called, so do not use a local variable.

## Writing a save

A save needs a title, an icon and the data itself.

```cpp
struct SaveData {
  uint32_t magic;
  uint16_t level;
  uint16_t lives;
};

static SaveData s_save;
static MCFS::Icon s_icon;

void WriteSave(void) {
  s_save = {.magic = 0x4D41444E, .level = 3, .lives = 5};

  s_icon.frameCount = 1;
  s_icon.clut[1] = 0x7FFF; // palette entry 1 is white
  // fill s_icon.pixels[0] with your 16x16 4bpp image

  MC::Error err = g_madnightEngine.m_memoryCardHelper.WriteFile(
      MC::Port::Port0, "BESLES-00000MYGAME", "MY GAME LEVEL 3", s_icon, &s_save, sizeof(s_save));
  if (err != MC::Error::OK) {
    printf("save failed: %s\n", MC::errorMessage(err));
  }
}
```

The arguments, after the port and optional file name:

- `title`: the name the BIOS manager shows. Printable ASCII is turned into fullwidth Shift-JIS, two bytes a character, into a 64-byte field, so only the first 32 characters fit. Anything outside printable ASCII, and the characters `" ' - \ ~`, become `?`.
- `icon`: an `MCFS::Icon`. `frameCount` is 1 to 3 (1 is a still icon, more frames animate). `clut` is a 16-colour BGR555 palette, and `pixels` holds up to 3 frames of 16x16 4bpp, 128 bytes a frame, two pixels a byte, low nibble first. The icon is copied, so it does not need to outlive the call.
- `data` and `dataSize`: the bytes to save.

The file takes whole 8 KB blocks. Its size is a 128-byte title frame, one 128-byte frame per icon frame, then `dataSize`, rounded up to the next block. If a save with the same name already exists it is replaced, with no error. The data is written before the directory, so a write that is interrupted does not leave a broken save behind.

The callback form takes the same arguments plus the callback. `data` must stay alive until the callback fires, so use a static or member, not a local.

```cpp
g_madnightEngine.m_memoryCardHelper.WriteFile(
    MC::Port::Port0, "BESLES-00000MYGAME", "MY GAME LEVEL 3", s_icon, &s_save, sizeof(s_save),
    [](MC::Error err) {
      // saved, or err says why not
    });
```

## Reading a save

```cpp
static SaveData s_save;
uint32_t length = 0;

MC::Error err = g_madnightEngine.m_memoryCardHelper.ReadFile(
    MC::Port::Port0, "BESLES-00000MYGAME", &s_save, sizeof(s_save), &length);
if (err == MC::Error::OK && s_save.magic == 0x4D41444E) {
  // loaded
}
```

`ReadFile` copies the data part of the save, everything after the title and icon frames, into `buffer`, up to `bufferSize` bytes. It returns `FileNotFound` if there is no save with that name.

`*outLength` is not the `dataSize` you wrote. It is the size of the whole data area, which is rounded up to the file's blocks, capped at `bufferSize`. Keep your own size or a magic number at the start of the data if you need to know what is real.

The callback form adds the callback at the end. `buffer` and `outLength` must both stay alive until it fires.

```cpp
static uint32_t s_length = 0;

g_madnightEngine.m_memoryCardHelper.ReadFile(MC::Port::Port0, "BESLES-00000MYGAME", &s_save, sizeof(s_save),
                                             &s_length, [](MC::Error err) {
                                               // s_save and s_length are filled in if err is OK
                                             });
```

## Anything else

`FileSystem()` returns the underlying `psyqo::MemoryCardFileSystem`. Use it for what the wrapper does not cover: formatting a card, listing its files, deleting a file, or reading a file's title and icon.

```cpp
MCFS *fs = g_madnightEngine.m_memoryCardHelper.FileSystem();
MC::Error err = fs->formatBlocking(Renderer::Instance().GPU(), MC::Port::Port0); // erases the whole card
```

The same one-operation-at-a-time rule applies to these calls.
