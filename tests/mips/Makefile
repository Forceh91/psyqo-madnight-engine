TARGET = engine-tests
TYPE = ps-exe

SRCS = \
	entry.cpp \

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SRCS += $(call rwildcard,./,test_*.cpp)

CXXFLAGS = -std=c++20

CPPFLAGS += -I../../src

include ../../madnight-engine.mk
include ../../third_party/nugget/psyqo-paths/psyqo-paths.mk
include ../../third_party/nugget/psyqo/snitch.mk
