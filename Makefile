# # The name of the binary to be created.
# TARGET = hello-3d

# # The type of the binary to be created - ps-exe is most common.
# TYPE = ps-exe

# # The list of sources files to compile within the binary.
# SRCS := $(shell find src -name '*.cpp')

# # Setting the minimum version of the C++. C++-20 is the minimum required version by PSYQo.
# CXXFLAGS = -std=c++20

# # This will activate the PSYQo library and the rest of the toolchain.
# include third_party/nugget/psyqo/psyqo.mk

# Binary name (no extension)
TARGET = hello-3d

# Output type
TYPE = ps-exe

# Source files (recursive find)
SRCS := $(shell find src -name '*.cpp')

# C++ standard
CXXFLAGS = -std=c++20

# Output directory
OUTDIR = build

# Include PSYQo build system
include third_party/nugget/psyqo/psyqo.mk

# Custom rule: after building the .ps-exe, move all artifacts to build/
.PHONY: all
all: $(TARGET).$(TYPE)
	@mkdir -p $(OUTDIR)
	@mv -f $(TARGET).ps-exe $(OUTDIR)/
	@mv -f $(TARGET).elf $(OUTDIR)/ 2>/dev/null || true
	@mv -f $(TARGET).map $(OUTDIR)/ 2>/dev/null || true
	@echo "Build complete: $(OUTDIR)/$(TARGET).ps-exe"
