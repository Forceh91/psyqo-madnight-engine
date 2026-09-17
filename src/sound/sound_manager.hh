/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "../helpers/archive.hh"
#include "EASTL/fixed_string.h"
#include "EASTL/fixed_vector.h"
#include "psyqo/coroutine.hh"
#include "psyqo/spu.hh"
#include <cstdint>

static constexpr uint8_t MAX_VAG_FILE_COUNT = 24; // same as the PS1's SPU channel count for now
static constexpr int8_t INVALID_VAG_FILE_ID = -1;
static constexpr uint32_t SPU_NOMINAL_PITCH = 4096;
static constexpr uint32_t SPU_MEMORY_SIZE = 0x80000;

static constexpr uint32_t SPU_ADR_INSTANT_ATTACK_NO_DECAY = 0x80000000;
static constexpr uint8_t SPU_MAX_CHANNEL_ID = 23;

typedef struct _VagEntry {
	int8_t id = INVALID_VAG_FILE_ID; // for quick reference
	uint64_t nameHash = 0;			 // hash of the name we supplied for the cd rom, not the one from the header
	uint32_t spuAddr = 0;			 // where it lives in SPU RAM
	uint32_t pitch = 0;				 // precomputed from sample rate
	uint32_t size = 0;				 // how much SPU RAM it occupies
} VagEntry;

class SoundManager final {
  public:
	// automatically called by the engine
	void Init(void);

	// resets the spuAllocPtr to initial, but doesn't clear anything from spu
	void Dump(void);
	psyqo::Coroutine<> LoadVAGFile(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName,
								   VagEntry** out);
	VagEntry* IsVAGLoaded(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName);
	VagEntry* IsVAGLoaded(uint64_t nameHash);
	VagEntry* IsVAGLoaded(const uint8_t& id);
	void SilenceChannels(const uint32_t channels);
	void PlayVAGFile(const VagEntry* vag, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig& config,
					 bool hardCut = false);
	void PlayVAGFile(const eastl::fixed_string<char, MAX_ARCHIVE_FILE_NAME_LEN>& fileName, uint8_t channelId,
					 const psyqo::SPU::ChannelPlaybackConfig& config, bool hardCut = false);
	void PlayVAGFile(const uint8_t& vagID, uint8_t channelId, const psyqo::SPU::ChannelPlaybackConfig& config,
					 bool hardCut = false);
	psyqo::SPU::ChannelPlaybackConfig CreatePlaybackConfig(const VagEntry* vag, uint16_t volume,
														   uint32_t adsr = SPU_ADR_INSTANT_ATTACK_NO_DECAY);
	psyqo::SPU::ChannelPlaybackConfig CreatePlaybackConfig(const VagEntry* vag, uint16_t volumeL, uint16_t volumeR,
														   uint32_t adsr = SPU_ADR_INSTANT_ATTACK_NO_DECAY);

  private:
	eastl::fixed_vector<VagEntry, MAX_VAG_FILE_COUNT> m_vagFiles;
	bool m_isInitialized = false;
	uint32_t m_spuAllocPtr = 0;
};
