TARGET = engine-tests
TYPE = ps-exe

SRCS = \
	entry.cpp \
	test_gameobject.cpp \

CXXFLAGS = -std=c++20

SNITCH := ../../third_party/nugget/psyqo/snitch/
LIBRARIES += $(SNITCH)libsnitch.a

CPPFLAGS += -I../../src
CPPFLAGS += -I../../third_party/nugget/psyqo/snitch/

include ../../madnight-engine.mk
include ../../third_party/nugget/psyqo-paths/psyqo-paths.mk
include ../../third_party/nugget/psyqo/psyqo.mk

$(SNITCH)libsnitch.a:
	$(MAKE) -C $(SNITCH) BUILD=$(BUILD)
