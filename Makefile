ENGINEDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

TARGET = madnight-engine
TYPE = library

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

# Source files (recursive find)
SRCS := $(call rwildcard,$(ENGINEDIR)src/,*.cpp)
SRCS += $(ENGINEDIR)third_party/nugget/modplayer/modplayer.c

EXTRA_DEPS += $(ENGINEDIR)madnight-engine.mk

include $(ENGINEDIR)third_party/nugget/psyqo/psyqo.mk
include $(ENGINEDIR)third_party/nugget/psyqo-paths/psyqo-paths.mk
