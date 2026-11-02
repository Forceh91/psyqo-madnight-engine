#pragma once

#include <EASTL/fixed_string.h>
#include <EASTL/internal/function.h>
#include <EASTL/string_view.h>

#include <cstdint>
#include <psyqo/memory-card-filesystem.hh>
#include <psyqo/memory-card.hh>

using MC = psyqo::MemoryCard;
using MCFS = psyqo::MemoryCardFileSystem;

enum CardPort : uint8_t { Port0 = (1 << 0), Port1 = (1 << 1) };

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
	MC::Error DoesSaveFileExist(const MC::Port& port, bool* outFileExists);

	// does a save file with the provided name exist (will overwrite anything set via `SetFileName`)?
	MC::Error DoesSaveFileExist(const MC::Port& port, const eastl::string_view& name, bool* outFileExists);

	// how many blocks are free on the card in this port?
	MC::Error GetFreeBlockCount(const MC::Port& port, uint32_t* outFreeBlocks);

	// (non-blocking) how many blocks are free on the card in this port?
	void GetFreeBlockCount(const MC::Port& port, uint32_t* outFreeBlocks, eastl::function<void(MC::Error)>&& callback);

	// (blocking) call to read the specified file name (set with `SetFileName`)
	MC::Error ReadFile(const MC::Port& port, void* buffer, uint32_t bufferSize, uint32_t* outLength);

	// (blocking) call to read the specified file name
	MC::Error ReadFile(const MC::Port& port, const eastl::string_view& fileName, void* buffer, uint32_t bufferSize,
					   uint32_t* outLength);

	// (non-blocking) call to read the specified file name (set with `SetFileName`)
	void ReadFile(const MC::Port& port, void* buffer, uint32_t bufferSize, uint32_t* outLength,
				  eastl::function<void(MC::Error)>&& callback);

	// (non-blocking) call to read the specified file name
	void ReadFile(const MC::Port& port, const eastl::string_view& fileName, void* buffer, uint32_t bufferSize,
				  uint32_t* outLength, eastl::function<void(MC::Error)>&& callback);

	// (blocking) call to write the specified file name (set with `SetFileName`)
	MC::Error WriteFile(const MC::Port& port, const eastl::string_view& title, const MCFS::Icon& icon, void* data,
						uint32_t dataSize);

	// (blocking) call to write the specified file name
	MC::Error WriteFile(const MC::Port& port, const eastl::string_view& fileName, const eastl::string_view& title,
						const MCFS::Icon& icon, void* data, uint32_t dataSize);

	// (non-blocking) call to write the specified file name (set with `SetFileName`)
	void WriteFile(const MC::Port& port, const eastl::string_view& title, const MCFS::Icon& icon, void* data,
				   uint32_t dataSize, eastl::function<void(MC::Error)>&& callback);

	// (non-blocking) call to write the specified file name
	void WriteFile(const MC::Port& port, const eastl::string_view& fileName, const eastl::string_view& title,
				   const MCFS::Icon& icon, void* data, uint32_t dataSize, eastl::function<void(MC::Error)>&& callback);

	constexpr MCFS* FileSystem(void) { return &m_fs; }

  private:
	MC m_memoryCard;
	MCFS m_fs{m_memoryCard};
	bool m_isPrepared = false;

	eastl::fixed_string<char, 21> m_fileName = "";
};
