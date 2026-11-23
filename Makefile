# Rata Workspace — Plain GNU Makefile
# Works with gcc on NixOS/Linux and MinGW gcc on Windows 11 (native MSYS2 or cross-compile).

ifeq ($(origin CC),default)
  CC := gcc
endif
PKG_CONFIG ?= pkg-config
BUILD ?= debug

# Detect target OS (supports native Linux, native Windows/MinGW, or Linux->Windows cross-compile)
ifndef TARGET
  ifeq ($(OS),Windows_NT)
    TARGET := windows
  else ifneq (,$(findstring mingw,$(CC)))
    TARGET := windows
  else
    TARGET := linux
  endif
endif

# Sanitizers: enabled by default for Linux debug builds, off on Windows MinGW
ifndef SANITIZE
  ifeq ($(TARGET),linux)
    ifeq ($(BUILD),debug)
      SANITIZE := 1
    else
      SANITIZE := 0
    endif
  else
    SANITIZE := 0
  endif
endif

ifeq ($(TARGET),windows)
  EXE_EXT := .exe
else
  EXE_EXT :=
endif

# Directories
BUILD_DIR := build/$(TARGET)-$(BUILD)
OBJ_DIR   := $(BUILD_DIR)/obj
BIN_DIR   := $(BUILD_DIR)

APP_BIN   := $(BIN_DIR)/rata-workspace$(EXE_EXT)
TEST_BIN  := $(BIN_DIR)/rata-tests$(EXE_EXT)

# Compiler flags (strict ISO C11, zero GNU extensions)
WARN_FLAGS   := -Wall -Wextra -Wpedantic -Werror -Wvla
BASE_CFLAGS  := -std=c11 $(WARN_FLAGS) -Isrc
OPT_FLAGS    :=
SAN_FLAGS    :=
SAN_LDFLAGS  :=

ifeq ($(BUILD),release)
  OPT_FLAGS := -O2 -DNDEBUG
else
  OPT_FLAGS := -g -O1
endif

ifeq ($(SANITIZE),1)
  SAN_FLAGS   := -fsanitize=address,undefined -fno-omit-frame-pointer
  SAN_LDFLAGS := -fsanitize=address,undefined
endif

CFLAGS   := $(BASE_CFLAGS) $(OPT_FLAGS) $(SAN_FLAGS)
LDFLAGS  := $(SAN_LDFLAGS)
MATH_LIB := -lm

# Raylib discovery: RAYLIB_DIR > explicit RAYLIB_INC/RAYLIB_LIB > pkg-config > default
ifneq ($(RAYLIB_DIR),)
  RAYLIB_CFLAGS := -isystem $(RAYLIB_DIR)/include
  ifeq ($(TARGET),linux)
    RAYLIB_LIBS := -L$(RAYLIB_DIR)/lib -Wl,-rpath,$(RAYLIB_DIR)/lib -lraylib
  else
    RAYLIB_LIBS := -L$(RAYLIB_DIR)/lib -lraylib
  endif
else ifneq ($(RAYLIB_INC),)
  RAYLIB_CFLAGS := -isystem $(RAYLIB_INC)
  RAYLIB_LIBS   := -L$(RAYLIB_LIB) -lraylib
else
  PKG_RAYLIB_OK := $(shell $(PKG_CONFIG) --exists raylib 2>/dev/null && echo 1 || echo 0)
  ifeq ($(PKG_RAYLIB_OK),1)
    RAYLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags raylib)
    RAYLIB_LIBS   := $(shell $(PKG_CONFIG) --libs raylib)
  else
    RAYLIB_CFLAGS :=
    RAYLIB_LIBS   := -lraylib
  endif
endif

ifeq ($(TARGET),windows)
  PLATFORM_LIBS := -lopengl32 -lgdi32 -lwinmm
else
  PLATFORM_LIBS := -lGL -lpthread -ldl -lrt -lX11
endif

UI_CFLAGS := $(CFLAGS) $(RAYLIB_CFLAGS) -isystem vendor/raygui
# Third-party raygui implementation compiled without -Werror on vendor code
RAYGUI_IMPL_CFLAGS := -std=c11 $(OPT_FLAGS) $(SAN_FLAGS) -Isrc $(RAYLIB_CFLAGS) -isystem vendor/raygui -Wno-unused-parameter -Wno-unused-function -Wno-pedantic

# Source lists (explicit per layer to preserve strict layering)
CORE_SRCS := \
	$(wildcard src/core/*.c)

ENGINE_SRCS := \
	$(wildcard src/engines/*.c)

UI_RAYGUI_IMPL_SRC := src/ui/ui_raygui_impl.c
UI_SRCS := $(filter-out $(UI_RAYGUI_IMPL_SRC),$(wildcard src/ui/*.c))
APP_SRCS := $(wildcard src/app/*.c)
TEST_SRCS := $(wildcard tests/*.c)
FUZZ_SRCS := $(wildcard fuzz/*.c)

CORE_OBJS        := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
ENGINE_OBJS      := $(patsubst %.c,$(OBJ_DIR)/%.o,$(ENGINE_SRCS))
UI_OBJS          := $(patsubst %.c,$(OBJ_DIR)/%.o,$(UI_SRCS))
UI_RAYGUI_OBJ    := $(if $(wildcard $(UI_RAYGUI_IMPL_SRC)),$(OBJ_DIR)/$(UI_RAYGUI_IMPL_SRC:.c=.o),)
APP_OBJS         := $(patsubst %.c,$(OBJ_DIR)/%.o,$(APP_SRCS))
TEST_OBJS        := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))
FUZZ_BINS        := $(patsubst fuzz/%.c,$(BIN_DIR)/%$(EXE_EXT),$(FUZZ_SRCS))

.PHONY: all app test fuzz-build fuzz-test fuzz-libfuzzer check-layering check clean info

all: app

app: $(APP_BIN)

$(APP_BIN): $(CORE_OBJS) $(ENGINE_OBJS) $(UI_OBJS) $(UI_RAYGUI_OBJ) $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@ $(RAYLIB_LIBS) $(PLATFORM_LIBS) $(MATH_LIB)

$(OBJ_DIR)/src/core/%.o: src/core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/src/engines/%.o: src/engines/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/src/ui/ui_raygui_impl.o: src/ui/ui_raygui_impl.c
	@mkdir -p $(dir $@)
	$(CC) $(RAYGUI_IMPL_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/src/ui/%.o: src/ui/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UI_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/src/app/%.o: src/app/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UI_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Tests link ONLY core + engines (no raylib / no UI), verifying layering at link time
$(TEST_BIN): $(CORE_OBJS) $(ENGINE_OBJS) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@ $(MATH_LIB)

test: $(TEST_BIN)
	./$(TEST_BIN)

# Standalone fuzz harnesses (work with gcc + ASan/UBSan or MinGW)
$(BIN_DIR)/fuzz_%$(EXE_EXT): fuzz/fuzz_%.c $(CORE_OBJS) $(ENGINE_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DFUZZ_STANDALONE $< $(CORE_OBJS) $(ENGINE_OBJS) $(LDFLAGS) -o $@ $(MATH_LIB)

fuzz-build: $(FUZZ_BINS)

fuzz-test: $(FUZZ_BINS)
	@for f in $(FUZZ_BINS); do \
		echo "Running standalone fuzz harness: $$f"; \
		./$$f || exit 1; \
	done

# Optional libFuzzer build with clang (run e.g. 'make fuzz-libfuzzer FUZZ_TIME=5')
FUZZ_TIME ?= 3
fuzz-libfuzzer:
	@mkdir -p $(BUILD_DIR)/libfuzzer
	@for src in $(FUZZ_SRCS); do \
		name=$$(basename $$src .c); \
		bin="$(BUILD_DIR)/libfuzzer/$$name"; \
		echo "Building libFuzzer target: $$bin"; \
		clang -std=c11 $(WARN_FLAGS) -g -O1 -Isrc -fsanitize=fuzzer,address,undefined \
			$$src $(CORE_SRCS) $(ENGINE_SRCS) -o $$bin $(MATH_LIB) || exit 1; \
		./$$bin -max_total_time=$(FUZZ_TIME) -max_len=1024 || exit 1; \
	done

# Verify strict architectural layering and language constraints
check-layering:
	@echo "Checking strict layering rules..."
	@if grep -rnE '#\s*include\s*["<](engines/|ui/|app/|raylib\.h|raygui\.h|complex\.h)' src/core/ 2>/dev/null; then \
		echo "ERROR: src/core/ violated layering or <complex.h> ban!"; exit 1; \
	fi
	@if grep -rnE '#\s*include\s*["<](ui/|app/|raylib\.h|raygui\.h|complex\.h)' src/engines/ 2>/dev/null; then \
		echo "ERROR: src/engines/ violated layering or <complex.h> ban!"; exit 1; \
	fi
	@if grep -rnE '#\s*include\s*["<].*\.c[">]' tests/ 2>/dev/null; then \
		echo "ERROR: tests/ must only include public headers, never .c files!"; exit 1; \
	fi
	@if grep -rnE '\b_Generic\b' src/ 2>/dev/null; then \
		echo "ERROR: _Generic is forbidden under portability rules!"; exit 1; \
	fi
	@echo "Layering and portability static checks passed."

check: check-layering test fuzz-test

info:
	@echo "CC=$(CC) TARGET=$(TARGET) BUILD=$(BUILD) SANITIZE=$(SANITIZE)"
	@echo "RAYLIB_CFLAGS=$(RAYLIB_CFLAGS)"
	@echo "RAYLIB_LIBS=$(RAYLIB_LIBS)"

clean:
	rm -rf build

-include $(wildcard $(OBJ_DIR)/src/core/*.d $(OBJ_DIR)/src/engines/*.d $(OBJ_DIR)/src/ui/*.d $(OBJ_DIR)/src/app/*.d $(OBJ_DIR)/tests/*.d)
