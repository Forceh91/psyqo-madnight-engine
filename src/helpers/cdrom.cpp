/*
 * Copyright (C) 2025-2026 Matt Hadden / Madnight Games
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "cdrom.hh"
#include "../madnight.hh"
#include "../render/renderer.hh"
#include "archive.hh"
#include <psyqo/alloc.h>
#include <psyqo/coroutine.hh>
#include <psyqo/xprintf.h>

#ifdef PCDRV
#include "common/kernel/pcdrv.h"
#endif

void CDRomHelper::init(eastl::function<void()> cb) {
#ifndef PCDRV
	m_cdrom.prepare();
	m_cdrom.resetBlocking(Renderer::Instance().GPU());
#else
	PCinit();
#endif

	g_madnightEngine.m_archiveHelper.init(cb);
}

psyqo::Coroutine<psyqo::Buffer<uint8_t>> CDRomHelper::LoadFile(const eastl::string_view& fileName) {
#ifndef PCDRV
	get_iso_file_name(fileName, m_loadingFileName);
	printf("CDRom: Attempting to read %s...\n", m_loadingFileName);

	auto buffer = co_await m_cdromLoader.readFile(m_loadingFileName, m_isoParser);
	if (buffer.empty()) {
		printf("CDRom: File %s not found or empty\n", m_loadingFileName);
	} else {
		printf("CDRom: Read %d bytes\n", buffer.size());
	}

	co_return eastl::move(buffer);
#else
	printf("PCDrv: Attempting to read %s...\n", fileName);

	psyqo::Buffer<uint8_t> buffer;

	int fd = PCopen(fileName.data(), 0, 0);
	if (fd < 0) {
		printf("PCDrv: Failed to open %s\n", fileName);
		co_return eastl::move(buffer);
	}

	int size = PClseek(fd, 0, 2); // SEEK_END
	if (size <= 0) {
		printf("PCDrv: Failed to get size of %s\n", fileName);
		PCclose(fd);
		co_return eastl::move(buffer);
	}
	PClseek(fd, 0, 0); // SEEK_SET

	buffer = psyqo::Buffer<uint8_t>(static_cast<size_t>(size));
	int bytesRead = PCread(fd, buffer.data(), size);
	PCclose(fd);

	if (bytesRead != size) {
		printf("PCDrv: Short read on %s (%d of %d bytes)\n", fileName, bytesRead, size);
		buffer.clear();
		co_return eastl::move(buffer);
	}

	printf("PCDrv: Read %d bytes from %s\n", size, fileName);
	co_return eastl::move(buffer);
#endif
}

#ifndef PCDRV
void CDRomHelper::get_iso_file_name(const eastl::string_view& file_name, char* iso_filename) {
	snprintf(iso_filename, 32, "%.*s;1", int(file_name.size()), file_name.data());
}
#endif
