# Binary name (no extension)
TARGET = madnight-engine-game

# Output type
TYPE = ps-exe

# make sure we know about the engine dir and psyqo
CPPFLAGS += -Imadnight_engine/src
CPPFLAGS += -Imadnight_engine/third_party/nugget

# Source files (recursive find)
rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SRCS := $(call rwildcard,src/,*.cpp)

# C++ standard
CXXFLAGS = -std=c++20

# CPPFLAGS_msan = -DUSE_PCSXMSAN -O0 -g
CPPFLAGS_debug = -g -O0

# Output directory
OUTDIR = build

# Include engine + PSYQo build system
include madnight_engine/madnight-engine.mk
include madnight_engine/third_party/nugget/psyqo-paths/psyqo-paths.mk
include madnight_engine/third_party/nugget/psyqo/psyqo.mk

# Custom rule: after building the .ps-exe, move all artifacts to build/
.PHONY: all
all: $(TARGET).$(TYPE)
	@mkdir -p $(OUTDIR)
	@mv -f $(TARGET).ps-exe $(OUTDIR)/
	@mv -f $(TARGET).elf $(OUTDIR)/ 2>/dev/null || true
	@mv -f $(TARGET).map $(OUTDIR)/ 2>/dev/null || true
	@echo "Build complete: $(OUTDIR)/$(TARGET).ps-exe"

