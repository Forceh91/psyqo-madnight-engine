#ifdef PCDRV
#include "archive.hh"
#else
#include "cdrom.hh"
#endif
#include "../madnight.hh"

#include <psyqo/xprintf.h>
#include <psyqo-paths/archive-manager.hh>

#ifdef PCDRV
psyqo::CDRomPCDrv ArchiveHelper::m_cdrom(g_gameIsoName);
#endif
psyqo::paths::ArchiveManager ArchiveHelper::m_archiveManager;
bool ArchiveHelper::m_archiveManagerInit = false;
char ArchiveHelper::m_loadingFileName[MAX_ARCHIVE_FILE_NAME_LEN];

void ArchiveHelper::init(eastl::function<void()> cb) {
	m_archiveManager.registerUCL_NRV2EDecompressor();

#ifndef PCDRV
	auto& cdrom = CDRomHelper::CDRomDevice();
#else
	auto& cdrom = m_cdrom;
#endif
	
    m_archiveManager.initialize(23, cdrom, [cb](bool success) {
		m_archiveManagerInit = success;
		printf("ARCHIVE: Initialize result=%d\n", m_archiveManagerInit);
		cb();
	});
}

psyqo::Coroutine<psyqo::Buffer<uint8_t>> ArchiveHelper::LoadFile(const char *fileName) {
	if (m_archiveManagerInit) {
		printf("ARCHIVE: Attempting to read %s...\n", fileName);

#ifndef PCDRV
	auto& cdrom = CDRomHelper::CDRomDevice();
#else
	auto& cdrom = m_cdrom;
#endif		

#ifdef PCDRV
		auto buffer = co_await m_archiveManager.readFile(fileName, cdrom);
#else
		auto buffer = co_await m_archiveManager.readFile(fileName, cdrom);
#endif
		if (buffer.empty())
			printf("ARCHIVE: File %s not found or empty\n", fileName);
		else
			printf("ARCHIVE: Read %d bytes\n", buffer.size());
		
		co_return eastl::move(buffer);
	} else {
		printf("ARCHIVE: Manager not initialized.\n");
		co_return psyqo::Buffer<uint8_t>{};
	}
}
