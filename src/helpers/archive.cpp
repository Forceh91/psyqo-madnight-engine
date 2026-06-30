#include "archive.hh"
#include "cdrom.hh"
#include "psyqo-paths/archive-manager.hh"
#include "psyqo/xprintf.h"
#include "../render/renderer.hh"

#ifdef PCDRV
psyqo::CDRomPCDrv ArchiveHelper::m_cdrom("ssweep.iso");
#endif
psyqo::paths::ArchiveManager ArchiveHelper::m_archiveManager;
bool ArchiveHelper::m_archiveManagerInit = false;
char ArchiveHelper::m_loadingFileName[MAX_ARCHIVE_FILE_NAME_LEN];

void ArchiveHelper::init(eastl::function<void()> cb) {
	m_archiveManager.registerUCL_NRV2EDecompressor();

#ifndef PCDRV
	auto& cdrom = CDRomHelper::CDRomDevice();
#else
	auto cdrom = m_cdrom;
#endif
	
    m_archiveManager.initialize(23, cdrom, [cb](bool success) {
		m_archiveManagerInit = success;
		printf("Archive: Initialize result=%d\n", m_archiveManagerInit);
		cb();
	});
}

psyqo::Coroutine<psyqo::Buffer<uint8_t>> ArchiveHelper::LoadFile(const char *fileName) {
	if (m_archiveManagerInit) {
		snprintf(m_loadingFileName, MAX_ARCHIVE_FILE_NAME_LEN, "%s", fileName);

		printf("Archive: Attempting to read %s...\n", m_loadingFileName);

#ifdef PCDRV
		auto buffer = co_await m_archiveManager.readFile(m_loadingFileName, m_cdrom);
#else
		auto buffer = co_await m_archiveManager.readFile(m_loadingFileName, CDRomHelper::CDRomDevice());
#endif
		if (buffer.empty())
			printf("Archive: File %s not found or empty\n", m_loadingFileName);
		else
			printf("Archive: Read %d bytes\n", buffer.size());
		
		co_return eastl::move(buffer);
	} else {
		printf("Archive: Manager not initialized.\n");
		co_return psyqo::Buffer<uint8_t>{};
	}
}
