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
