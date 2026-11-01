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
		return false;
	}

	uint8_t activePorts = 0;
	if (IsCardInPort(MC::Port::Port0)) {
		activePorts |= CardPort::Port1;
	}

	if (IsCardInPort(MC::Port::Port1)) {
		activePorts |= CardPort::Port2;
	}

	return activePorts;
}

bool MemoryCard::DoesSaveFileExist(const MC::Port& port) {
	if (!m_isPrepared) {
		printf("[MEMORY CARD] You must call `MemoryCard::Prepare` first.\n");
		return false;
	}

	bool doesFileExist = false;
	auto error = m_fs.fileExistsBlocking(Renderer::Instance().GPU(), port, m_fileName.data(), &doesFileExist);
	if (error != MC::Error::OK) {
		return false;
	}

	return doesFileExist;
}

bool MemoryCard::DoesSaveFileExist(const MC::Port& port, const eastl::string_view& name) {
	m_fileName.assign(name.data(), name.length());
	return DoesSaveFileExist(port);
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

	m_fs.getFreeBlockCount(port, outFreeBlocks, [callback](auto error) { callback(error); });
}
