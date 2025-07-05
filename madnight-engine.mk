ifndef ENGINEDIR
ENGINEDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

LIBRARIES += $(ENGINEDIR)libmadnight-engine.a

include $(ENGINEDIR)third_party/nugget/psyqo-paths/psyqo-paths.mk
include $(ENGINEDIR)third_party/nugget/psyqo/psyqo.mk

$(ENGINEDIR)libmadnight-engine.a:
	$(MAKE) -C $(ENGINEDIR) BUILD=$(BUILD) CPPFLAGS_$(BUILD)="$(CPPFLAGS_$(BUILD))" LDFLAGS_$(BUILD)="$(LDFLAGS_$(BUILD))"

clean::
	$(MAKE) -C $(ENGINEDIR) clean

.PHONY: clean $(ENGINEDIR)libmadnight-engine.a
endif
