#include "memory_card.hh"
#include "../render/renderer.hh"
#include <psyqo/xprintf.h>

void MemoryCard::Prepare(void) {
	m_memoryCard.prepare();
	m_isPrepared = true;
}

uint8_t MemoryCard::GetCardsInPorts(void) {
	if (!m_isPrepared) {
		printf("[MEMORY CARD] You must call `MemoryCard::Prepare` first.\n");
		return 0;
	}

	uint8_t activePorts = 0;
	if (IsCardInPort(MC::Port::Port0)) {
		activePorts |= CardPort::Port0;
	}

	if (IsCardInPort(MC::Port::Port1)) {
		activePorts |= CardPort::Port1;
	}

	return activePorts;
}

MC::Error MemoryCard::GetCardState(const MC::Port& port) {
	if (!m_isPrepared) {
		printf("[MEMORY CARD] You must call `MemoryCard::Prepare` first.\n");
		return MC::Error::NoCard;
	}

	return m_fs.getCardStateBlocking(Renderer::Instance().GPU(), port);
}

void MemoryCard::GetCardState(const MC::Port& port, eastl::function<void(MC::Error)>&& callback) {
	if (!m_isPrepared) {
		printf("[MEMORY CARD] You must call `MemoryCard::Prepare` first.\n");
		callback(MC::Error::NoCard);
		return;
	}
		
	m_fs.getCardState(port, eastl::move(callback));
}

MC::Error MemoryCard::DoesSaveFileExist(const MC::Port& port, bool* outFileExists) {
	if (!m_isPrepared) {
		printf("[MEMORY CARD] You must call `MemoryCard::Prepare` first.\n");
		return MC::Error::NoCard;
	}

	return m_fs.fileExistsBlocking(Renderer::Instance().GPU(), port, m_fileName.data(), outFileExists);
}

MC::Error MemoryCard::DoesSaveFileExist(const MC::Port& port, const eastl::string_view& name, bool* outFileExists) {
	m_fileName.assign(name.data(), name.length());
	return DoesSaveFileExist(port, outFileExists);
}

MC::Error MemoryCard::GetFreeBlockCount(const MC::Port& port, uint32_t* outFreeBlocks) {
	if (!m_isPrepared) {
		printf("[MEMORY CARD] You must call `MemoryCard::Prepare` first.\n");
		return MC::Error::NoCard;
	}

	return m_fs.getFreeBlockCountBlocking(Renderer::Instance().GPU(), port, outFreeBlocks);
}

void MemoryCard::GetFreeBlockCount(const MC::Port& port, uint32_t* outFreeBlocks,
								   eastl::function<void(MC::Error)>&& callback) {
	if (!m_isPrepared) {
		printf("[MEMORY CARD] You must call `MemoryCard::Prepare` first.\n");
		callback(MC::Error::NoCard);
		return;
	}

	m_fs.getFreeBlockCount(port, outFreeBlocks, eastl::move(callback));
}

MC::Error MemoryCard::ReadFile(const MC::Port& port, void* buffer, uint32_t bufferSize, uint32_t* outLength) {
	return m_fs.readFileBlocking(Renderer::Instance().GPU(), port, m_fileName.data(), buffer, bufferSize, outLength);
}

MC::Error MemoryCard::ReadFile(const MC::Port& port, const eastl::string_view& fileName, void* buffer,
							   uint32_t bufferSize, uint32_t* outLength) {
	m_fileName.assign(fileName.data(), fileName.length());
	return ReadFile(port, buffer, bufferSize, outLength);
}

void MemoryCard::ReadFile(const MC::Port& port, void* buffer, uint32_t bufferSize, uint32_t* outLength,
						  eastl::function<void(MC::Error)>&& callback) {
	m_fs.readFile(port, m_fileName.data(), buffer, bufferSize, outLength, eastl::move(callback));
}

void MemoryCard::ReadFile(const MC::Port& port, const eastl::string_view& fileName, void* buffer, uint32_t bufferSize,
						  uint32_t* outLength, eastl::function<void(MC::Error)>&& callback) {
	m_fileName.assign(fileName.data(), fileName.length());
	ReadFile(port, buffer, bufferSize, outLength, eastl::move(callback));
}

MC::Error MemoryCard::WriteFile(const MC::Port& port, const eastl::string_view& title, const MCFS::Icon& icon,
								const void* data, uint32_t dataSize) {
	m_title.assign(title.data(), title.length());
	return m_fs.writeFileBlocking(Renderer::Instance().GPU(), port, m_fileName.data(), m_title.data(), icon, data,
								  dataSize);
}

MC::Error MemoryCard::WriteFile(const MC::Port& port, const eastl::string_view& fileName,
								const eastl::string_view& title, const MCFS::Icon& icon, const void* data,
								uint32_t dataSize) {
	m_fileName.assign(fileName.data(), fileName.length());
	return WriteFile(port, title, icon, data, dataSize);
}

void MemoryCard::WriteFile(const MC::Port& port, const eastl::string_view& title, const MCFS::Icon& icon,
						   const void* data, uint32_t dataSize, eastl::function<void(MC::Error)>&& callback) {
	m_title.assign(title.data(), title.length());
	m_fs.writeFile(port, m_fileName.data(), m_title.data(), icon, data, dataSize, eastl::move(callback));
}

void MemoryCard::WriteFile(const MC::Port& port, const eastl::string_view& fileName, const eastl::string_view& title,
						   const MCFS::Icon& icon, const void* data, uint32_t dataSize,
						   eastl::function<void(MC::Error)>&& callback) {
	m_fileName.assign(fileName.data(), fileName.length());
	WriteFile(port, title, icon, data, dataSize, eastl::move(callback));
}
