# Binary name (no extension)
TARGET = madnight

# Output type
TYPE = ps-exe

# Source files (recursive find)
SRCS := $(shell find src -name '*.cpp')

# C++ standard
CXXFLAGS = -std=c++20

CPPFLAGS_msan = -g -O0 -DUSE_PCSXMSAN

# Output directory
OUTDIR = build

# Include PSYQo build system
include third_party/nugget/psyqo-paths/psyqo-paths.mk
include third_party/nugget/psyqo/psyqo.mk

# Custom rule: after building the .ps-exe, move all artifacts to build/
.PHONY: all
all: $(TARGET).$(TYPE)
	@mkdir -p $(OUTDIR)
	@mv -f $(TARGET).ps-exe $(OUTDIR)/
	@mv -f $(TARGET).elf $(OUTDIR)/ 2>/dev/null || true
	@mv -f $(TARGET).map $(OUTDIR)/ 2>/dev/null || true
	@echo "Build complete: $(OUTDIR)/$(TARGET).ps-exe"
