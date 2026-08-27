#include "sound_manager.hh"
#include <psyqo/xprintf.h>

using namespace psyqo::fixed_point_literals;

#define SWAP32(x) ((x>>24) | ((x>>8)&0xFF00) | ((x<<8)&0xFF0000) | (x<<24))

bool SoundManager::m_isInitialized = false;
Pool<VagEntry, MAX_VAG_FILE_COUNT> SoundManager::m_pool;
uint32_t SoundManager::m_spuAllocPtr = psyqo::SPU::BASE_ALLOC_ADDR;

void SoundManager::Init(void) {
    psyqo::SPU::initialize();
    m_isInitialized = true;
}

psyqo::Coroutine<> SoundManager::LoadVAGFile(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName, VagEntry** out) {
    if (!m_isInitialized)
        Init();

    // fallback response to nothing
    if (out) *out = nullptr;

    // did we already load this?
    auto existingVag = IsVAGLoaded(fileName);
    if (existingVag) {
        if (out) *out = existingVag;
        co_return;
    }

    auto vagIx = m_pool.Acquire();
    if (vagIx == INVALID_POOL_ID)
        co_return;

    // get the actual data off the cd and make sure its valid
    auto buffer = co_await ArchiveHelper::LoadFile(fileName.c_str());
    void *data = buffer.data();
    size_t size = buffer.size();

    if (!data || !size) {
        buffer.clear();
        printf("VAG: Failed to load VAG or it has no file size.\n");
        co_return;
    }

    // begin loading data
    auto* vag = m_pool.Get(vagIx);
    __builtin_memset(vag, 0, sizeof(VagEntry));
    vag->id = vagIx;

    uint8_t* ptr = (uint8_t*)data;

    // check the magic
    eastl::fixed_string<char, 5> magic;
    magic.assign(reinterpret_cast<char*>(ptr));
    if (magic.compare("VAGp")) {
        printf("VAG: Header magic is invalid, aborting.\n");
        buffer.clear();
        co_return;
    }
    ptr += 4;

    // version check
    uint32_t version;
    __builtin_memcpy(&version, ptr, sizeof(uint32_t));    
    if (SWAP32(version) != 0x00000020) {
        printf("VAG: Header version is invalid, aborting.\n");
        buffer.clear();
        co_return;
    }
    ptr += sizeof(uint32_t);

    // skip over reserved block
    ptr += sizeof(uint32_t);

    // store the data size which is aligned to the nearest 64 bytes (upwards)
    __builtin_memcpy(&vag->size, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    vag->size = (SWAP32(vag->size) + 63) & ~63;

    // make sure it fits
    if (SPU_MEMORY_SIZE - m_spuAllocPtr < vag->size) {
        printf("VAG: Not enough space in SPU, aborting.\n");
        buffer.clear();
        co_return;
    }

    // store the pitch based off of sample rate
    uint32_t sampleRate;
    __builtin_memcpy(&sampleRate, ptr, sizeof(uint32_t));
    vag->pitch = SWAP32(sampleRate) * SPU_NOMINAL_PITCH / psyqo::SPU::BASE_SAMPLE_RATE;
    ptr += sizeof(uint32_t);

    // store our name for it
    vag->nameHash = HashName(fileName);

    // skip past the rest of the header
    ptr += 28;

    // upload data to spu ram and update where in ram we are
    psyqo::SPU::dmaWrite(m_spuAllocPtr, ptr, vag->size, 16);
    vag->spuAddr = m_spuAllocPtr;
    m_spuAllocPtr += vag->size;

    // all done?
    if (out) *out = vag;

    // dump it from memory
    buffer.clear();
    printf("VAG: Successfully uploaded VAG of %d bytes into the SPU.\n", size);
}

VagEntry* SoundManager::IsVAGLoaded(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName) {
    return IsVAGLoaded(HashName(fileName));
}

VagEntry* SoundManager::IsVAGLoaded(uint64_t nameHash) {
    for (auto i = 0; i < MAX_VAG_FILE_COUNT; i++) {
        auto* vag = m_pool.Get(i);
        if (vag->nameHash == nameHash)
            return vag;
    }

    // not loaded yet
    return nullptr;
}

VagEntry* SoundManager::IsVAGLoaded(const uint8_t& id) {
    for (auto i = 0; i < MAX_VAG_FILE_COUNT; i++) {
        auto* vag = m_pool.Get(i);
        if (vag->id == id)
            return vag;
    }

    // not loaded yet
    return nullptr;
}

void SoundManager::SilenceChannels(const uint32_t channelMask) {
    psyqo::SPU::silenceChannels(channelMask);
}

void SoundManager::PlayVAGFile(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut) {
    auto vag = IsVAGLoaded(fileName);
    if (vag) PlayVAGFile(vag, channelId, config, hardCut);
}

void SoundManager::PlayVAGFile(const uint8_t& vagID, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut) {
    auto vag = IsVAGLoaded(vagID);
    if (vag) PlayVAGFile(vag, channelId, config, hardCut);
}

void SoundManager::PlayVAGFile(const VagEntry* vag, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut) {
    if (!vag || !vag->size || vag->spuAddr < psyqo::SPU::BASE_ALLOC_ADDR) return;
    
    channelId = eastl::min(channelId, SPU_MAX_CHANNEL_ID);
    psyqo::SPU::playADPCM(channelId, vag->spuAddr, config, hardCut);
}

psyqo::SPU::ChannelPlaybackConfig SoundManager::CreatePlaybackConfig(const VagEntry* vag, uint16_t volume, uint32_t adsr) {
    return CreatePlaybackConfig(vag, volume, volume, adsr);
}

psyqo::SPU::ChannelPlaybackConfig SoundManager::CreatePlaybackConfig(const VagEntry *vag, uint16_t volumeL, uint16_t volumeR, uint32_t adsr) {
    if (!vag) return {0, 0, 0, 0};

    psyqo::FixedPoint<12, uint16_t> pitch;
    pitch.value = vag->pitch;    

    return psyqo::SPU::ChannelPlaybackConfig{
        pitch,
        volumeL,
        volumeR,
        adsr
    };
}

void SoundManager::Dump(void) {
    psyqo::SPU::silenceChannels(0xffffffff);
    m_spuAllocPtr = psyqo::SPU::BASE_ALLOC_ADDR;

    auto count = m_pool.count();
    for (auto i = 0; i < count; i++) {
        auto vag = m_pool.Get(i);
        if (!vag)
            continue;
        
        __builtin_memset(vag, 0, sizeof(VagEntry));
        vag->id = INVALID_POOL_ID;
    }

    m_pool.Dump();
}
