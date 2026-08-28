CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra

BIN_DIR := bin

# one subfolder per section (tests/pool/, tests/billboard/, ...),
# each containing its own test_*.cc — add a new section by adding
# a new folder, nothing here needs to change
TEST_SRCS := $(wildcard */test_*.cc)
TEST_BINS := $(patsubst %.cc,$(BIN_DIR)/%,$(TEST_SRCS))

.PHONY: test clean

# build + run every section's test binary
test: $(TEST_BINS)
	@for bin in $(TEST_BINS); do \
		echo "== running $$bin =="; \
		./$$bin || exit 1; \
	done

$(BIN_DIR)/%: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(BIN_DIR)
