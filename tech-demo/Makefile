# The tech demo builds against the engine checkout it lives in.
ENGINE := ..

TARGET = tech-demo

# The engine's face loop runs on a 712-byte scratchpad stack, which subdivision
# overflows at -Os (the nugget default). Build the demo at -O3 unless told otherwise.
BUILD ?= ReleaseFast
TYPE = ps-exe

CPPFLAGS += -I$(ENGINE)/src
CPPFLAGS += -I$(ENGINE)/third_party/nugget

# `make BENCH=1` runs one lap, saves the numbers to the memory card, reads them
# back and exits the emulator. CI uses it.
ifeq ($(BENCH),1)
CPPFLAGS += -DTECHDEMO_BENCH
endif

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SRCS := $(call rwildcard,src/,*.cpp)

CXXFLAGS = -std=c++20

# nugget's word-at-a-time memcpy and memset (common/crt0/memory-s.s)
LDFLAGS += -Wl,-wrap,memcpy -Wl,-wrap,memset

include $(ENGINE)/madnight-engine.mk

# The asset image a PCDRV build loads (techdemo.bin, see tools/make_image.py).
# For a real disc, run pcsx-redux's authoring tool on toc.json instead.
.PHONY: image
image: build/techdemo.bin

build/techdemo.bin: tools/make_image.py $(call rwildcard,assets/,*)
	python3 -I tools/make_image.py assets $@
