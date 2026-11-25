# Rata Workspace — Plain GNU Makefile
# Works with gcc on NixOS/Linux and MinGW gcc on Windows 11 (native MSYS2 or cross-compile).

ifeq ($(origin CC),default)
  CC := gcc
endif
PKG_CONFIG ?= pkg-config
BUILD ?= debug
PREFIX ?= $(HOME)/.local

# Single source of truth for semantic version
VERSION := $(shell tr -d ' \r\n' < VERSION)
VER_PARTS := $(subst ., ,$(VERSION))
VER_MAJOR := $(word 1,$(VER_PARTS))
VER_MINOR := $(word 2,$(VER_PARTS))
VER_PATCH := $(word 3,$(VER_PARTS))

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

# Windres auto-detection for Windows icon + version resource embedding
ifeq ($(TARGET),windows)
  ifndef WINDRES
    ifneq (,$(findstring x86_64-w64-mingw32-,$(CC)))
      WINDRES := x86_64-w64-mingw32-windres
    else
      WINDRES := windres
    endif
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

APP_BIN      := $(BIN_DIR)/rata-workspace$(EXE_EXT)
TEST_BIN     := $(BIN_DIR)/rata-tests$(EXE_EXT)
UI_SMOKE_BIN := $(BIN_DIR)/ui-smoke$(EXE_EXT)

# Compiler flags (strict ISO C11, zero GNU extensions)
WARN_FLAGS   := -Wall -Wextra -Wpedantic -Werror -Wvla
VER_DEFS     := -DRATA_VERSION=\"$(VERSION)\"
BASE_CFLAGS  := -std=c11 $(WARN_FLAGS) $(VER_DEFS) -Isrc
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
# Set RAYLIB_STATIC=1 (default on Windows when RAYLIB_DIR is given) to link libraylib.a statically.
ifeq ($(TARGET),windows)
  RAYLIB_STATIC ?= 1
else
  RAYLIB_STATIC ?= 0
endif

ifneq ($(RAYLIB_DIR),)
  RAYLIB_CFLAGS := -isystem $(RAYLIB_DIR)/include
  ifeq ($(RAYLIB_STATIC),1)
    RAYLIB_LIBS := $(RAYLIB_DIR)/lib/libraylib.a
  else ifeq ($(TARGET),linux)
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

# Static C runtime / winpthread on Windows so rata-workspace.exe is a single self-contained GUI binary
ifeq ($(TARGET),windows)
  APP_LDFLAGS   := $(LDFLAGS) -mwindows -static -static-libgcc
  PLATFORM_LIBS := -Wl,-Bstatic -lpthread -Wl,-Bdynamic -lopengl32 -lgdi32 -lwinmm -lwinhttp
else
  APP_LDFLAGS   := $(LDFLAGS)
  PLATFORM_LIBS := -lGL -lpthread -ldl -lrt -lX11
endif

ENGINE_CFLAGS := $(CFLAGS) -isystem vendor/monocypher
UI_CFLAGS := $(CFLAGS) $(RAYLIB_CFLAGS) -isystem vendor/raygui
# Third-party vendor implementations compiled without -Werror on unmodified upstream code
RAYGUI_IMPL_CFLAGS := -std=c11 $(OPT_FLAGS) $(SAN_FLAGS) -Isrc $(RAYLIB_CFLAGS) -isystem vendor/raygui -Wno-unused-parameter -Wno-unused-function -Wno-pedantic
VENDOR_CRYPTO_CFLAGS := -std=c11 -O2 -Ivendor/monocypher

# Source lists (explicit per layer to preserve strict layering)
CORE_SRCS := $(wildcard src/core/*.c)
ENGINE_SRCS := $(wildcard src/engines/*.c)
MONOCYPHER_SRCS := vendor/monocypher/monocypher.c vendor/monocypher/monocypher-ed25519.c

UI_RAYGUI_IMPL_SRC := src/ui/ui_raygui_impl.c
UI_SRCS := $(filter-out $(UI_RAYGUI_IMPL_SRC),$(wildcard src/ui/*.c))
APP_SRCS := $(wildcard src/app/*.c)

UI_SMOKE_SRC := tests/ui_smoke.c
TEST_SRCS := $(filter-out $(UI_SMOKE_SRC),$(wildcard tests/*.c))
FUZZ_SRCS := $(wildcard fuzz/*.c)

CORE_OBJS       := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
ENGINE_OBJS     := $(patsubst %.c,$(OBJ_DIR)/%.o,$(ENGINE_SRCS))
MONOCYPHER_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(MONOCYPHER_SRCS))
UI_OBJS         := $(patsubst %.c,$(OBJ_DIR)/%.o,$(UI_SRCS))
UI_RAYGUI_OBJ   := $(if $(wildcard $(UI_RAYGUI_IMPL_SRC)),$(OBJ_DIR)/$(UI_RAYGUI_IMPL_SRC:.c=.o),)
APP_OBJS        := $(patsubst %.c,$(OBJ_DIR)/%.o,$(APP_SRCS))
TEST_OBJS       := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))
UI_SMOKE_OBJ    := $(OBJ_DIR)/$(UI_SMOKE_SRC:.c=.o)
FUZZ_BINS       := $(patsubst fuzz/%.c,$(BIN_DIR)/%$(EXE_EXT),$(FUZZ_SRCS))

ifeq ($(TARGET),windows)
  WIN_RES_OBJ := $(OBJ_DIR)/assets/rata-workspace.res.o
  SIGN_TOOL_LIBS := -ladvapi32
else
  WIN_RES_OBJ :=
  SIGN_TOOL_LIBS :=
endif

.PHONY: all app test ui-smoke fuzz-build fuzz-test fuzz-libfuzzer check-layering check icons sign-tool install uninstall clean info

all: app

app: $(APP_BIN)

$(APP_BIN): $(CORE_OBJS) $(ENGINE_OBJS) $(MONOCYPHER_OBJS) $(UI_OBJS) $(UI_RAYGUI_OBJ) $(APP_OBJS) $(WIN_RES_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(APP_LDFLAGS) $^ -o $@ $(RAYLIB_LIBS) $(PLATFORM_LIBS) $(MATH_LIB)

GEN_ICON_BIN  := $(BIN_DIR)/gen_icon$(EXE_EXT)
SIGN_TOOL_BIN := $(BIN_DIR)/rata-sign$(EXE_EXT)

$(GEN_ICON_BIN): tools/gen_icon.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $< $(LDFLAGS) -o $@ $(RAYLIB_LIBS) $(PLATFORM_LIBS) $(MATH_LIB)

icons: $(GEN_ICON_BIN)
	./$(GEN_ICON_BIN)

$(SIGN_TOOL_BIN): tools/rata_sign.c $(CORE_OBJS) $(ENGINE_OBJS) $(MONOCYPHER_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(CORE_OBJS) $(ENGINE_OBJS) $(MONOCYPHER_OBJS) $(LDFLAGS) -o $@ $(SIGN_TOOL_LIBS) $(MATH_LIB)

sign-tool: $(SIGN_TOOL_BIN)

$(OBJ_DIR)/assets/rata-workspace.res.o: assets/rata-workspace.rc assets/rata-workspace.ico VERSION
	@mkdir -p $(dir $@)
	$(WINDRES) -DRATA_VER_MAJOR=$(VER_MAJOR) -DRATA_VER_MINOR=$(VER_MINOR) \
		-DRATA_VER_PATCH=$(VER_PATCH) -i $< -O coff -o $@

$(OBJ_DIR)/src/core/%.o: src/core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/src/engines/%.o: src/engines/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ENGINE_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/vendor/monocypher/%.o: vendor/monocypher/%.c
	@mkdir -p $(dir $@)
	$(CC) $(VENDOR_CRYPTO_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/src/ui/ui_raygui_impl.o: src/ui/ui_raygui_impl.c
	@mkdir -p $(dir $@)
	$(CC) $(RAYGUI_IMPL_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/src/ui/%.o: src/ui/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UI_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/src/app/%.o: src/app/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UI_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/tests/ui_smoke.o: tests/ui_smoke.c
	@mkdir -p $(dir $@)
	$(CC) $(UI_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/tests/%.o: tests/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Tests link ONLY core + engines + vendored Monocypher (no raylib / no UI), verifying layering at link time
$(TEST_BIN): $(CORE_OBJS) $(ENGINE_OBJS) $(MONOCYPHER_OBJS) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@ $(MATH_LIB)

test: $(TEST_BIN)
	./$(TEST_BIN)

# UI smoke test renders all RataWave tabs, pole clamping, Coming Soon, and About dialog
$(UI_SMOKE_BIN): $(CORE_OBJS) $(ENGINE_OBJS) $(MONOCYPHER_OBJS) $(UI_OBJS) $(UI_RAYGUI_OBJ) $(UI_SMOKE_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(APP_LDFLAGS) $^ -o $@ $(RAYLIB_LIBS) $(PLATFORM_LIBS) $(MATH_LIB)

ui-smoke: $(UI_SMOKE_BIN)
	@if [ "$(TARGET)" = "linux" ] && [ -z "$$DISPLAY" ] && command -v xvfb-run >/dev/null 2>&1; then \
		ASAN_OPTIONS=detect_leaks=0 xvfb-run -a -s "-screen 0 1280x800x24" ./$(UI_SMOKE_BIN); \
	else \
		ASAN_OPTIONS=detect_leaks=0 ./$(UI_SMOKE_BIN); \
	fi

# Standalone fuzz harnesses (work with gcc + ASan/UBSan or MinGW)
$(BIN_DIR)/fuzz_%$(EXE_EXT): fuzz/fuzz_%.c $(CORE_OBJS) $(ENGINE_OBJS) $(MONOCYPHER_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DFUZZ_STANDALONE $< $(CORE_OBJS) $(ENGINE_OBJS) $(MONOCYPHER_OBJS) $(LDFLAGS) -o $@ $(MATH_LIB)

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
		clang -std=c11 $(WARN_FLAGS) $(VER_DEFS) -g -O1 -Isrc -isystem vendor/monocypher -fsanitize=fuzzer,address,undefined \
			$$src $(CORE_SRCS) $(ENGINE_SRCS) $(MONOCYPHER_SRCS) -o $$bin $(MATH_LIB) || exit 1; \
		./$$bin -max_total_time=$(FUZZ_TIME) -max_len=1024 || exit 1; \
	done

# Verify strict architectural layering and language constraints
check-layering:
	@echo "Checking strict layering rules..."
	@if grep -rnE '#\s*include\s*["<](engines/|ui/|app/|raylib\.h|raygui\.h|complex\.h|monocypher)' src/core/ 2>/dev/null; then \
		echo "ERROR: src/core/ violated layering or <complex.h> ban!"; exit 1; \
	fi
	@if grep -rnE '#\s*include\s*["<](ui/|app/|raylib\.h|raygui\.h|complex\.h|windows\.h|winhttp\.h|wincred\.h)' src/engines/ 2>/dev/null; then \
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

# Install / uninstall (Linux / NixOS)
install: app
	install -d "$(PREFIX)/bin"
	install -d "$(PREFIX)/share/applications"
	install -d "$(PREFIX)/share/icons/hicolor/256x256/apps"
	install -d "$(PREFIX)/share/icons/hicolor/scalable/apps"
	install -d "$(PREFIX)/share/licenses/rata-workspace"
	install -m 755 "$(APP_BIN)" "$(PREFIX)/bin/rata-workspace$(EXE_EXT)"
	install -m 644 assets/rata-workspace.desktop "$(PREFIX)/share/applications/rata-workspace.desktop"
	install -m 644 assets/rata-workspace.png "$(PREFIX)/share/icons/hicolor/256x256/apps/rata-workspace.png"
	install -m 644 assets/rata-workspace.svg "$(PREFIX)/share/icons/hicolor/scalable/apps/rata-workspace.svg"
	install -m 644 LICENSE "$(PREFIX)/share/licenses/rata-workspace/LICENSE"
	install -m 644 vendor/raygui/LICENSE "$(PREFIX)/share/licenses/rata-workspace/raygui-LICENSE.txt"
	install -m 644 vendor/jetbrains-mono/OFL.txt "$(PREFIX)/share/licenses/rata-workspace/jetbrains-mono-OFL.txt"
	install -m 644 vendor/lora/OFL.txt "$(PREFIX)/share/licenses/rata-workspace/lora-OFL.txt"
	install -m 644 vendor/monocypher/LICENCE.md "$(PREFIX)/share/licenses/rata-workspace/monocypher-LICENCE.txt"

uninstall:
	rm -f "$(PREFIX)/bin/rata-workspace$(EXE_EXT)"
	rm -f "$(PREFIX)/share/applications/rata-workspace.desktop"
	rm -f "$(PREFIX)/share/icons/hicolor/256x256/apps/rata-workspace.png"
	rm -f "$(PREFIX)/share/icons/hicolor/scalable/apps/rata-workspace.svg"
	rm -rf "$(PREFIX)/share/licenses/rata-workspace"

info:
	@echo "VERSION=$(VERSION) CC=$(CC) TARGET=$(TARGET) BUILD=$(BUILD) SANITIZE=$(SANITIZE) RAYLIB_STATIC=$(RAYLIB_STATIC)"
	@echo "RAYLIB_CFLAGS=$(RAYLIB_CFLAGS)"
	@echo "RAYLIB_LIBS=$(RAYLIB_LIBS)"

clean:
	rm -rf build

-include $(wildcard $(OBJ_DIR)/src/core/*.d $(OBJ_DIR)/src/engines/*.d $(OBJ_DIR)/src/ui/*.d $(OBJ_DIR)/src/app/*.d $(OBJ_DIR)/tests/*.d)
