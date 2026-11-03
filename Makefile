# Linux-only ITSV build (C++17 + system-installed Wt + locally built libmseed 2.19.8)
CXX ?= g++
TARGET := itsv
BUILD_DIR := build
WT_PREFIX ?= /usr/local
WT_INC_DIR := $(WT_PREFIX)/include
WT_LIB_DIR := $(WT_PREFIX)/lib

LIBMSEED_VERSION := 2.19.8
LIBMSEED_DIR := third_party/libmseed
LIBMSEED_FETCH := third_party/fetch-libmseed-2.19.8.sh
LIBMSEED_A := $(LIBMSEED_DIR)/libmseed.a

CPPFLAGS += -Isrc -I$(WT_INC_DIR) -I$(WT_INC_DIR)/Wt -I$(LIBMSEED_DIR)
CXXFLAGS ?= -O2 -g -std=c++17 -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS += -L$(WT_LIB_DIR) -Wl,-rpath,$(WT_LIB_DIR)
WT_LIBS ?= -lwt -lwthttp
COMMON_LIBS ?= -lboost_system -lboost_thread -lboost_filesystem -lboost_program_options -lssl -lcrypto -lz -lpthread -ldl
LDLIBS := $(LIBMSEED_A) $(WT_LIBS) $(COMMON_LIBS)

SRCS := \
  src/Recording.cpp \
  src/ViewState.cpp \
  src/MiniSeedReader.cpp \
  src/WaveformDisplay.cpp \
  src/WaveformPreview.cpp \
  src/ChannelSelectionDialog.cpp \
  src/main.cpp

OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean clean-libmseed vendor-libmseed run test check-wt print-config dist-src

all: check-wt $(TARGET)

check-wt:
	@test -f "$(WT_INC_DIR)/Wt/WApplication.h" || ( \
	  echo "ERROR: Wt headers not found at $(WT_INC_DIR)/Wt/WApplication.h"; \
	  echo "Set WT_PREFIX, for example: make WT_PREFIX=/usr"; exit 1 )
	@test -d "$(WT_LIB_DIR)" || ( echo "ERROR: Wt library directory not found: $(WT_LIB_DIR)"; exit 1 )

$(LIBMSEED_DIR)/libmseed.h: $(LIBMSEED_FETCH)
	./$(LIBMSEED_FETCH)

$(LIBMSEED_A): $(LIBMSEED_DIR)/libmseed.h
	$(MAKE) -C $(LIBMSEED_DIR)

$(OBJS): | $(LIBMSEED_DIR)/libmseed.h

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(LIBMSEED_A)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

vendor-libmseed: $(LIBMSEED_DIR)/libmseed.h
	@echo "libmseed $(LIBMSEED_VERSION) is ready in $(LIBMSEED_DIR)"

run: $(TARGET)
	./run

dist-src:
	python3 scripts/make_source_release.py

test:
	bash tests/run_tests.sh
	python3 tests/check_source_release.py

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

clean-libmseed:
	@if [ -f "$(LIBMSEED_DIR)/Makefile" ]; then $(MAKE) -C $(LIBMSEED_DIR) clean; fi

print-config:
	@echo "WT_PREFIX=$(WT_PREFIX)"
	@echo "CXXFLAGS=$(CXXFLAGS)"
	@echo "CPPFLAGS=$(CPPFLAGS)"
	@echo "LDLIBS=$(LDLIBS)"

-include $(DEPS)
