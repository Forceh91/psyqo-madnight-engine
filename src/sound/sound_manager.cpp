#include "sound_manager.hh"
#include "../helpers/cdrom.hh"
#include "psyqo/spu.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/xprintf.h"

using namespace psyqo::fixed_point_literals;

#define SWAP32(x) ((x>>24) | ((x>>8)&0xFF00) | ((x<<8)&0xFF0000) | (x<<24))

bool SoundManager::m_isInitialized = false;
eastl::fixed_vector<VagEntry, MAX_VAG_FILE_COUNT> SoundManager::m_vagFiles;
uint32_t SoundManager::m_spuAllocPtr = psyqo::SPU::BASE_ALLOC_ADDR;

void SoundManager::Init(void) {
    psyqo::SPU::initialize();
    m_isInitialized = true;
}

psyqo::Coroutine<> SoundManager::LoadVAGFile(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>& fileName, VagEntry** out) {
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

    // get the actual data off the cd and make sure its valid
    auto buffer = co_await CDRomHelper::LoadFile(fileName.c_str());
    void *data = buffer.data();
    size_t size = buffer.size();

    if (!data || !size) {
        buffer.clear();
        printf("VAG: Failed to load VAG or it has no file size.\n");
        co_return;
    }

    // begin loading data
    VagEntry vag;
    __builtin_memset(&vag, 0, sizeof(VagEntry));

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

    // store the data size
    __builtin_memcpy(&vag.size, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    vag.size = SWAP32(vag.size);

    // make sure it fits
    if (SPU_MEMORY_SIZE - m_spuAllocPtr < vag.size) {
        printf("VAG: Not enough space in SPU, aborting.\n");
        buffer.clear();
        co_return;
    }

    // store the pitch based off of sample rate
    uint32_t sampleRate;
    __builtin_memcpy(&sampleRate, ptr, sizeof(uint32_t));
    vag.pitch = SWAP32(sampleRate) * SPU_NOMINAL_PITCH / psyqo::SPU::BASE_SAMPLE_RATE;
    ptr += sizeof(uint32_t);

    // store our name for it
    vag.name.assign(fileName);

    // skip past the rest of the header
    ptr += 28;

    // upload data to spu ram and update where in ram we are
    psyqo::SPU::dmaWrite(m_spuAllocPtr, ptr, vag.size, 16);
    vag.spuAddr = m_spuAllocPtr;
    m_spuAllocPtr += vag.size;

    // all done?
    m_vagFiles.push_back(vag);
    if (out) *out = &m_vagFiles.back();

    // dump it from memory
    buffer.clear();
    printf("VAG: Successfully uploaded VAG of %d bytes into the SPU.\n", size);
}

VagEntry* SoundManager::IsVAGLoaded(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>& fileName) {
    VagEntry* loadedVAG;
    for (auto& vag : m_vagFiles) {
        if (vag.name == fileName)
            return &vag;
    }

    // not loaded yet
    return nullptr;
}

VagEntry* SoundManager::IsVAGLoaded(const uint8_t& id) {
    VagEntry* loadedVAG;
    for (auto& vag : m_vagFiles) {
        if (vag.id == id)
            return &vag;
    }

    // not loaded yet
    return nullptr;
}

void SoundManager::SilenceChannels(const uint32_t channelMask) {
    psyqo::SPU::silenceChannels(channelMask);
}

void SoundManager::PlayVAGFile(const eastl::fixed_string<char, MAX_CDROM_FILE_NAME_LEN>& fileName, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut) {
    auto vag = IsVAGLoaded(fileName);
    if (vag) PlayVAGFile(vag, channelId, config, hardCut);
}

void SoundManager::PlayVAGFile(const uint8_t& vagID, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut) {
    auto vag = IsVAGLoaded(vagID);
    if (vag) PlayVAGFile(vag, channelId, config, hardCut);
}

void SoundManager::PlayVAGFile(const VagEntry* vag, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig &config, bool hardCut) {
    if (!vag || !vag->size || vag->spuAddr < psyqo::SPU::BASE_ALLOC_ADDR) return;

    psyqo::SPU::playADPCM(channelId, vag->spuAddr, config, hardCut);
}

void SoundManager::Dump(void) {
    psyqo::SPU::silenceChannels(0xffffffff);
    m_spuAllocPtr = psyqo::SPU::BASE_ALLOC_ADDR;
    m_vagFiles.clear();
}
