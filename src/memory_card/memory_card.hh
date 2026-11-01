#pragma once

#include "EASTL/internal/function.h"
#include <EASTL/fixed_string.h>
#include <EASTL/string_view.h>

#include <psyqo/memory-card-filesystem.hh>
#include <psyqo/memory-card.hh>

using MC = psyqo::MemoryCard;
using MCFS = psyqo::MemoryCardFileSystem;

enum CardPort : uint8_t { Port1 = (1 << 0), Port2 = (1 << 1) };

class MemoryCard final {
  public:
	// automatically called by the engine before handing over to the game code
	void Prepare();

	// check if a card is present in the specified port
	bool IsCardInPort(const MC::Port& port) { return m_memoryCard.probeBlocking(port) == MC::Error::OK; }

	// checks all ports and returns a bitmask of ports.
	// to check if a card was present in a port simply check that the return value `& CardPort::PortX` isn't 0
	uint8_t GetCardsInPorts(void);

	// make sure you set this before doing a file read or write command
	void SetFileName(const eastl::string_view& name) { m_fileName.assign(name.data(), name.length()); }

	// does a save file with the name provided in `SetFileName` exist?
	bool DoesSaveFileExist(const MC::Port& port);

	// does a save file with the provided name exist (will overwrite anything set via `SetFileName`)?
	bool DoesSaveFileExist(const MC::Port& port, const eastl::string_view& name);

	// how many blocks are free on the card in this port?
	MC::Error GetFreeBlockCount(const MC::Port& port, uint32_t* outFreeBlocks);

	// (non-blocking) how many blocks are free on the card in this port?
	void GetFreeBlockCount(const MC::Port& port, uint32_t* outFreeBlocks, eastl::function<void(MC::Error)>&& callback);

	constexpr MCFS* FileSystem(void) { return &m_fs; }

  private:
	MC m_memoryCard;
	MCFS m_fs{m_memoryCard};
	bool m_isPrepared = false;

	eastl::fixed_string<char, 32> m_fileName = "";
};
