#pragma once

#include "EASTL/fixed_string.h"
#include "../helpers/file_defs.hh"
#include "EASTL/fixed_vector.h"
#include "psyqo/coroutine.hh"
#include "psyqo/spu.hh"
#include <cstdint>

static constexpr uint8_t VAG_FILE_NAME_LEN = 16;
static constexpr uint8_t MAX_VAG_FILE_COUNT = 24; // same as the PS1's SPU channel count for now
static constexpr int8_t INVALID_VAG_FILE_ID = -1;
static constexpr uint32_t SPU_NOMINAL_PITCH = 4096;
static constexpr uint32_t SPU_MEMORY_SIZE = 0x80000;

static constexpr uint32_t SPU_ADR_INSTANT_ATTACK_NO_DECAY = 0x80000000;
static constexpr uint8_t SPU_MAX_CHANNEL_ID = 23;

typedef struct _VagEntry {
    int8_t id = INVALID_VAG_FILE_ID; // for quick reference
    eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN> name; // the name we supplied for the cd rom, not the one from the header
    uint32_t spuAddr;                       // where it lives in SPU RAM
    uint32_t pitch;                         // precomputed from sample rate
    uint32_t size;                          // how much SPU RAM it occupies
} VagEntry;

class SoundManager final {
public:
    // automatically called by the engine
    static void Init(void);

    // resets the spuAllocPtr to initial, but doesn't clear anything from spu
    static void Dump(void);
    static psyqo::Coroutine<> LoadVAGFile(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>& fileName, VagEntry** out);
    static VagEntry* IsVAGLoaded(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>& fileName);
    static VagEntry* IsVAGLoaded(const uint8_t& fileName);
    static void SilenceChannels(const uint32_t channels);
    static void PlayVAGFile(const VagEntry* vag, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut = false);
    static void PlayVAGFile(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>& fileName, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut = false);
    static void PlayVAGFile(const uint8_t& vagID, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut = false);
    static psyqo::SPU::ChannelPlaybackConfig CreatePlaybackConfig(const VagEntry* vag, uint16_t volume, uint32_t adsr = SPU_ADR_INSTANT_ATTACK_NO_DECAY);
    static psyqo::SPU::ChannelPlaybackConfig CreatePlaybackConfig(const VagEntry* vag, uint16_t volumeL, uint16_t volumeR, uint32_t adsr = SPU_ADR_INSTANT_ATTACK_NO_DECAY);

private:
    static eastl::fixed_vector<VagEntry, MAX_VAG_FILE_COUNT> m_vagFiles;
    static bool m_isInitialized;
    static uint32_t m_spuAllocPtr;
};
