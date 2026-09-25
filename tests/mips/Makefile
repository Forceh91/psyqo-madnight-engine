TARGET = engine-tests
TYPE = ps-exe

SRCS = \
	entry.cpp \

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SRCS += $(call rwildcard,./,test_*.cpp)

CXXFLAGS = -std=c++20

CPPFLAGS += -I../../src

# Listed before the includes below, which pull in common.mk: the elf rule's
# prerequisites are expanded when it is read, and snitch.mk adds its library
# after that point.
LIBRARIES += $(abspath ../../third_party/nugget/psyqo/snitch)/libsnitch.a

include ../../madnight-engine.mk
include ../../third_party/nugget/psyqo-paths/psyqo-paths.mk
include ../../third_party/nugget/psyqo/snitch.mk
