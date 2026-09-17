# SPDX-License-Identifier: 0BSD

MOONPAK_DIR     := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

ARM_CC          ?= arm-none-eabi-gcc
ARM_CXX         ?= arm-none-eabi-g++
ARM_OBJDUMP     ?= arm-none-eabi-objdump
ARM_OBJCOPY     ?= arm-none-eabi-objcopy
FFMPEG          ?= ffmpeg
NODE            ?= node

CFLAGS          ?= -Wall -O3
CXXFLAGS        ?= -Wall -O3

M_SCRIPTS_DIR   := $(MOONPAK_DIR)/scripts
M_SRC_DIR       := $(MOONPAK_DIR)/src
M_TESTS_DIR     := $(MOONPAK_DIR)/tests
M_TYPES_DIR     := $(MOONPAK_DIR)/types
M_XFORM_DIR     := $(MOONPAK_DIR)/xform

SCRIPT_EMBED    := $(M_SCRIPTS_DIR)/embed.ts
SCRIPT_FAMI     := $(M_SCRIPTS_DIR)/famistudio.ts
SCRIPT_TYPELIB  := $(M_SCRIPTS_DIR)/typelib.ts

project-dir      = $(if $(filter .,$(PROJECT_DIR)),$(1),$(PROJECT_DIR)/$(1))
P_BUILD_DIR     := $(call project-dir,build)
P_DATA_DIR      := $(call project-dir,data)
P_SRC_DIR       := $(call project-dir,src)
P_TESTS_DIR     := $(call project-dir,tests)
P_TYPES_DIR     := $(call project-dir,types)

D_DPCM_DIR      := $(P_DATA_DIR)/dpcm
D_SONGS_DIR     := $(P_DATA_DIR)/songs
D_SPSH_DIR      := $(P_DATA_DIR)/spritesheets

B_COMMON_DIR    := $(P_BUILD_DIR)/common
B_GBA_DIR       := $(P_BUILD_DIR)/gba
B_TESTS_DIR     := $(P_BUILD_DIR)/tests
B_XFORM_DIR     := $(P_BUILD_DIR)/xform

BCM_TYPES_DIR   := $(B_COMMON_DIR)/moonpak/types
BCP_TYPES_DIR   := $(B_COMMON_DIR)/types
BCP_DATA_DIR    := $(B_COMMON_DIR)/data

BCPD_DPCM_DIR   := $(BCP_DATA_DIR)/dpcm
BCPD_SONGS_DIR  := $(BCP_DATA_DIR)/songs
BCPD_SPSH_DIR   := $(BCP_DATA_DIR)/spritesheets

BG_COMMON_DIR   := $(B_GBA_DIR)/common
BG_PROJECT_DIR  := $(B_GBA_DIR)/project
BG_MOONPAK_DIR  := $(B_GBA_DIR)/moonpak

GBA_ELF         := $(P_BUILD_DIR)/$(NAME).elf
GBA_DUMP        := $(P_BUILD_DIR)/$(NAME).dump
GBA_ROM         := $(P_BUILD_DIR)/$(NAME).gba
GBA_MAP         := $(P_BUILD_DIR)/$(NAME).map
TESTS           := $(P_BUILD_DIR)/tests/tests
XFORM           := $(P_BUILD_DIR)/xform/xform

# -----

TESTS_CXXFLAGS     :=         \
	-std=gnu++20                \
	-Wall                       \
	-Wno-unused-function        \
	-O3                         \
	-DTESTS                     \
	-DPLATFORM_HOST             \
	-I$(P_SRC_DIR)              \
	-I$(M_SRC_DIR)              \
	-I$(B_COMMON_DIR)           \
	$(addprefix -D,$(DEFINES))  \
	$(addprefix -I,$(INCLUDES))

# -----

XFORM_CPPFLAGS     :=         \
	-std=gnu++20                \
	-Wall                       \
	-Wno-unused-function        \
	-O3                         \
	-DXFORM                     \
	-DPLATFORM_HOST             \
	-I$(M_XFORM_DIR)

# -----

ARM_COMMON_FLAGS   :=         \
	-mcpu=arm7tdmi              \
	-mtune=arm7tdmi             \
	-mthumb-interwork           \
	-ffunction-sections         \
	-fdata-sections
ARM_PREPROC_FLAGS  :=         \
	-DPLATFORM_GBA              \
	-I$(P_SRC_DIR)              \
	-I$(M_SRC_DIR)              \
	-I$(B_COMMON_DIR)           \
	$(addprefix -D,$(DEFINES))  \
	$(addprefix -I,$(INCLUDES))
ARM_ASFLAGS        :=         \
	-x assembler-with-cpp       \
	-mthumb                     \
	$(ARM_COMMON_FLAGS)         \
	$(ARM_PREPROC_FLAGS)        \
	$(ASFLAGS)
ARM_CFLAGS         :=         \
	-std=gnu23                  \
	-mthumb                     \
	$(ARM_COMMON_FLAGS)         \
	$(ARM_PREPROC_FLAGS)        \
	$(CFLAGS)
ARM_CXXFLAGS       :=         \
	-std=gnu++20                \
	-fno-exceptions             \
	-fno-rtti                   \
	-fno-threadsafe-statics     \
	$(ARM_COMMON_FLAGS)         \
	$(ARM_PREPROC_FLAGS)        \
	$(CXXFLAGS)
ARM_LDFLAGS        :=         \
	-Wl,-Map,$(GBA_MAP)         \
	-Wl,--gc-sections           \
	-specs=nano.specs           \
	-T $(M_SRC_DIR)/moonpak/link.ld \
	-Wl,--start-group           \
	-lc                         \
	-Wl,--end-group             \
	$(ARM_COMMON_FLAGS)         \
	$(LDFLAGS)

# -----

.PHONY: all clean dump tests test test-v xform dpcm-export dpcm-export-overwrite
.DEFAULT_GOAL := all
.SECONDARY:

all: $(GBA_ROM)

tests: $(TESTS)

test: $(TESTS)
	$(TESTS) $(FILTER)

test-v: $(TESTS)
	$(TESTS) -v $(FILTER)

xform: $(XFORM)

clean:
	rm -rf $(P_BUILD_DIR)

dump: $(GBA_DUMP)

# -----

TESTS_CPP       := $(shell find $(M_TESTS_DIR) -type f -name '*.cpp')
TESTS_OBJS      := $(patsubst $(M_TESTS_DIR)/%.cpp,$(B_TESTS_DIR)/%.cpp.o,$(TESTS_CPP))
TESTS_DEPS      := $(TESTS_OBJS:.o=.d)

$(B_TESTS_DIR)/%.cpp.o: $(M_TESTS_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(TESTS_CXXFLAGS) -MMD -MP -c -o $@ $<

$(TESTS): $(TESTS_OBJS)
	$(CXX) -o $@ $(TESTS_OBJS)

# -----

XFORM_CPP       := $(shell find $(M_XFORM_DIR) -type f -name '*.cpp')
XFORM_OBJS      := $(patsubst $(M_XFORM_DIR)/%.cpp,$(B_XFORM_DIR)/%.cpp.o,$(XFORM_CPP))
XFORM_DEPS      := $(XFORM_OBJS:.o=.d)

$(B_XFORM_DIR)/%.cpp.o: $(M_XFORM_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(XFORM_CPPFLAGS) -MMD -MP -c -o $@ $<

$(XFORM): $(XFORM_OBJS)
	$(CXX) -o $@ $(XFORM_OBJS)

# -----

TYPELIB_HPP     := $(B_COMMON_DIR)/typelib.hpp
TYPELIB_CPP     := $(B_COMMON_DIR)/typelib.cpp
TYPELIB_JS      := $(B_COMMON_DIR)/typelib.js
COMMON_HPP      += $(TYPELIB_HPP)
COMMON_CPP      += $(TYPELIB_CPP)

$(TYPELIB_HPP) $(TYPELIB_CPP) $(TYPELIB_JS): $(SCRIPT_TYPELIB)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_TYPELIB) -s $@

# -----

M_TYPES         := $(shell find $(M_TYPES_DIR) -type f -name '*.type')
M_TYPE_HPP      := $(patsubst $(M_TYPES_DIR)/%.type,$(BCM_TYPES_DIR)/%.hpp,$(M_TYPES))
M_TYPE_CPP      := $(patsubst $(M_TYPES_DIR)/%.type,$(BCM_TYPES_DIR)/%.cpp,$(M_TYPES))
M_TYPE_JS       := $(patsubst $(M_TYPES_DIR)/%.type,$(BCM_TYPES_DIR)/%.js,$(M_TYPES))
COMMON_HPP      += $(M_TYPE_HPP)
COMMON_CPP      += $(M_TYPE_CPP)

$(BCM_TYPES_DIR)/%.cpp: $(M_TYPES_DIR)/%.type $(SCRIPT_TYPELIB)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_TYPELIB) -i $< -o $@

$(BCM_TYPES_DIR)/%.hpp: $(M_TYPES_DIR)/%.type $(SCRIPT_TYPELIB)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_TYPELIB) -i $< -o $@

$(BCM_TYPES_DIR)/%.js: $(M_TYPES_DIR)/%.type $(SCRIPT_TYPELIB)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_TYPELIB) -i $< -o $@

# -----

P_TYPES         := $(shell find $(P_TYPES_DIR) -type f -name '*.type')
P_TYPE_HPP      := $(patsubst $(P_TYPES_DIR)/%.type,$(BCP_TYPES_DIR)/%.hpp,$(P_TYPES))
P_TYPE_CPP      := $(patsubst $(P_TYPES_DIR)/%.type,$(BCP_TYPES_DIR)/%.cpp,$(P_TYPES))
P_TYPE_JS       := $(patsubst $(P_TYPES_DIR)/%.type,$(BCP_TYPES_DIR)/%.js,$(P_TYPES))
COMMON_HPP      += $(P_TYPE_HPP)
COMMON_CPP      += $(P_TYPE_CPP)

$(BCP_TYPES_DIR)/%.cpp: $(P_TYPES_DIR)/%.type $(SCRIPT_TYPELIB)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_TYPELIB) -i $< -o $@

$(BCP_TYPES_DIR)/%.hpp: $(P_TYPES_DIR)/%.type $(SCRIPT_TYPELIB)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_TYPELIB) -i $< -o $@

$(BCP_TYPES_DIR)/%.js: $(P_TYPES_DIR)/%.type $(SCRIPT_TYPELIB)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_TYPELIB) -i $< -o $@

# -----

# TODO: separate palettes for obj/bg?
# TODO: user can provide .png files too?

PALETTE_BIN     := $(BCP_DATA_DIR)/palette.bin
PALETTE_HPP     := $(PALETTE_BIN:.bin=.hpp)
PALETTE_CPP     := $(PALETTE_BIN:.bin=.cpp)
COMMON_HPP      += $(PALETTE_HPP)
COMMON_CPP      += $(PALETTE_CPP)

PALETTE_PNGS    := \
	$(shell find $(D_SPSH_DIR) -type f -name '*.png')

$(PALETTE_BIN): $(PALETTE_PNGS) $(XFORM)
	@mkdir -p $(@D)
	$(XFORM) palette256 -o $(PALETTE_BIN) $(PALETTE_PNGS)

$(PALETTE_HPP) $(PALETTE_CPP): $(SCRIPT_EMBED)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_EMBED) -o $(PALETTE_HPP) -o $(PALETTE_CPP) -n $(B_COMMON_DIR) $(PALETTE_BIN)

# -----

SPSH_PNG        := $(shell find $(D_SPSH_DIR) -type f -name '*.png')
SPSH_BIN        := $(patsubst $(D_SPSH_DIR)/%.png,$(BCPD_SPSH_DIR)/%.bin,$(SPSH_PNG))
SPSH_HPP        := $(patsubst $(D_SPSH_DIR)/%.png,$(BCPD_SPSH_DIR)/%.hpp,$(SPSH_PNG))
SPSH_CPP        := $(patsubst $(D_SPSH_DIR)/%.png,$(BCPD_SPSH_DIR)/%.cpp,$(SPSH_PNG))
COMMON_HPP      += $(SPSH_HPP)
COMMON_CPP      += $(SPSH_CPP)

$(BCPD_SPSH_DIR)/%.bin: $(D_SPSH_DIR)/%.png $(PALETTE_BIN) $(XFORM)
	@mkdir -p $(@D)
	$(XFORM) copyTiles256 -p $(PALETTE_BIN) -o $@ $<

$(BCPD_SPSH_DIR)/%.hpp $(BCPD_SPSH_DIR)/%.cpp: $(BCPD_SPSH_DIR)/%.bin $(SCRIPT_EMBED)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_EMBED)       \
		-o $(BCPD_SPSH_DIR)/$*.hpp  \
		-o $(BCPD_SPSH_DIR)/$*.cpp  \
		-n $(B_COMMON_DIR)          \
		$(BCPD_SPSH_DIR)/$*.bin

# -----

ANIMATIONS      := $(P_DATA_DIR)/animations.js
ANIMATIONS_HPP  := $(BCP_DATA_DIR)/animations.hpp
ANIMATIONS_CPP  := $(BCP_DATA_DIR)/animations.cpp
COMMON_HPP      += $(ANIMATIONS_HPP)
COMMON_CPP      += $(ANIMATIONS_CPP)

$(ANIMATIONS_HPP) $(ANIMATIONS_CPP): $(ANIMATIONS) $(M_SCRIPTS_DIR)/animations.js
	@mkdir -p $(@D)
	$(NODE) $(M_SCRIPTS_DIR)/animations.js -o $(ANIMATIONS_HPP) -o $(ANIMATIONS_CPP) $(ANIMATIONS)

# -----

DPCM_TABLE      := $(BCPD_DPCM_DIR)/dpcm-table.json
DPCM_TABLE_HPP  := $(BCPD_DPCM_DIR)/DpcmTable.hpp
DPCM_TABLE_CPP  := $(BCPD_DPCM_DIR)/DpcmTable.cpp
COMMON_HPP      += $(DPCM_TABLE_HPP)
COMMON_CPP      += $(DPCM_TABLE_CPP)

SONGS_TXT       := $(shell find $(D_SONGS_DIR) -type f -name '*.txt')
SONGS_BIN       := $(patsubst $(D_SONGS_DIR)/%.txt,$(BCPD_SONGS_DIR)/%.bin,$(SONGS_TXT))
SONGS_HPP       := $(patsubst $(D_SONGS_DIR)/%.txt,$(BCPD_SONGS_DIR)/%.hpp,$(SONGS_TXT))
SONGS_CPP       := $(patsubst $(D_SONGS_DIR)/%.txt,$(BCPD_SONGS_DIR)/%.cpp,$(SONGS_TXT))
COMMON_HPP      += $(SONGS_HPP)
COMMON_CPP      += $(SONGS_CPP)

$(DPCM_TABLE) $(DPCM_TABLE_HPP) $(DPCM_TABLE_CPP): $(SONGS_TXT) $(SCRIPT_FAMI)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_FAMI) dpcm-table -t $(DPCM_TABLE) -d $(D_DPCM_DIR) -x $(BCPD_DPCM_DIR) \
		$(SONGS_TXT)

define fami_dpcm_hint
	$(info $(1))
	@$(1); status=$$?;                                               \
	if [ $$status -eq 2 ]; then                                      \
		echo "\nMissing DPCM samples when building song:\n  $<" >&2;   \
		echo "\nTry to extract the missing samples by running:\n" >&2; \
		echo "  make dpcm-export\n" >&2;                               \
	fi;                                                              \
	exit $$status;
endef

$(BCPD_SONGS_DIR)/%.bin: $(D_SONGS_DIR)/%.txt $(DPCM_TABLE) $(SCRIPT_FAMI)
	@mkdir -p $(@D)
	$(call fami_dpcm_hint,$(NODE) $(SCRIPT_FAMI) song -o $@ -t $(DPCM_TABLE) $<)

$(BCPD_SONGS_DIR)/%.hpp $(BCPD_SONGS_DIR)/%.cpp: $(BCPD_SONGS_DIR)/%.bin $(SCRIPT_EMBED)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_EMBED) -o $(BCPD_SONGS_DIR)/$*.hpp -o $(BCPD_SONGS_DIR)/$*.cpp \
		-n $(B_COMMON_DIR) $(BCPD_SONGS_DIR)/$*.bin

# -----

dpcm-export:
	@mkdir -p $(D_DPCM_DIR)
	@set -e; for input in $(SONGS_TXT); do                            \
		$(NODE) $(SCRIPT_FAMI) dpcm-export -d $(D_DPCM_DIR) "$$input";  \
	done

dpcm-export-overwrite:
	@mkdir -p $(D_DPCM_DIR)
	@set -e; for input in $(SONGS_TXT); do                               \
		$(NODE) $(SCRIPT_FAMI) dpcm-export -d $(D_DPCM_DIR) -f "$$input";  \
	done

# -----

P_SRC_S         := $(shell find $(P_SRC_DIR) -type f -name '*.s')
P_SRC_C         := $(shell find $(P_SRC_DIR) -type f -name '*.c')
P_SRC_IWRAM_CPP := $(shell find $(P_SRC_DIR) -type f -name '*.iwram.cpp')
P_SRC_CPP       := $(shell find $(P_SRC_DIR) -type f -name '*.cpp' ! -name '*.iwram.cpp')

M_SRC_S         := $(shell find $(M_SRC_DIR) -type f -name '*.s')
M_SRC_C         := $(shell find $(M_SRC_DIR) -type f -name '*.c')
M_SRC_IWRAM_CPP := $(shell find $(M_SRC_DIR) -type f -name '*.iwram.cpp')
M_SRC_CPP       := $(shell find $(M_SRC_DIR) -type f -name '*.cpp' ! -name '*.iwram.cpp')

ARM_OBJS        := \
	$(patsubst $(B_COMMON_DIR)/%.cpp,$(BG_COMMON_DIR)/%.cpp.o,$(COMMON_CPP))                 \
	$(patsubst $(P_SRC_DIR)/%.s,$(BG_PROJECT_DIR)/%.s.o,$(P_SRC_S))                          \
	$(patsubst $(P_SRC_DIR)/%.c,$(BG_PROJECT_DIR)/%.c.o,$(P_SRC_C))                          \
	$(patsubst $(P_SRC_DIR)/%.iwram.cpp,$(BG_PROJECT_DIR)/%.iwram.cpp.o,$(P_SRC_IWRAM_CPP))  \
	$(patsubst $(P_SRC_DIR)/%.cpp,$(BG_PROJECT_DIR)/%.cpp.o,$(P_SRC_CPP))                    \
	$(patsubst $(M_SRC_DIR)/%.s,$(BG_MOONPAK_DIR)/%.s.o,$(M_SRC_S))                          \
	$(patsubst $(M_SRC_DIR)/%.c,$(BG_MOONPAK_DIR)/%.c.o,$(M_SRC_C))                          \
	$(patsubst $(M_SRC_DIR)/%.iwram.cpp,$(BG_MOONPAK_DIR)/%.iwram.cpp.o,$(M_SRC_IWRAM_CPP))  \
	$(patsubst $(M_SRC_DIR)/%.cpp,$(BG_MOONPAK_DIR)/%.cpp.o,$(M_SRC_CPP))
ARM_DEPS        := $(ARM_OBJS:.o=.d)

$(BG_PROJECT_DIR)/%.s.o: $(P_SRC_DIR)/%.s
	@mkdir -p $(@D)
	$(ARM_CC) $(ARM_ASFLAGS) -I$(dir $<) -MMD -MP -c -o $@ $<

$(BG_PROJECT_DIR)/%.c.o: $(P_SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(ARM_CC) $(ARM_CFLAGS) -MMD -MP -c -o $@ $<

$(BG_PROJECT_DIR)/%.iwram.cpp.o: $(P_SRC_DIR)/%.iwram.cpp
	@mkdir -p $(@D)
	$(ARM_CXX) $(ARM_CXXFLAGS) -marm -MMD -MP -c -o $@ $<

$(BG_PROJECT_DIR)/%.cpp.o: $(P_SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(ARM_CXX) $(ARM_CXXFLAGS) -mthumb -MMD -MP -c -o $@ $<

$(BG_COMMON_DIR)/%.cpp.o: $(B_COMMON_DIR)/%.cpp
	@mkdir -p $(@D)
	$(ARM_CXX) $(ARM_CXXFLAGS) -mthumb -MMD -MP -c -o $@ $<

$(BG_MOONPAK_DIR)/%.s.o: $(M_SRC_DIR)/%.s
	@mkdir -p $(@D)
	$(ARM_CC) $(ARM_ASFLAGS) -I$(dir $<) -MMD -MP -c -o $@ $<

$(BG_MOONPAK_DIR)/%.c.o: $(M_SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(ARM_CC) $(ARM_CFLAGS) -MMD -MP -c -o $@ $<

$(BG_MOONPAK_DIR)/%.iwram.cpp.o: $(M_SRC_DIR)/%.iwram.cpp
	@mkdir -p $(@D)
	$(ARM_CXX) $(ARM_CXXFLAGS) -marm -MMD -MP -c -o $@ $<

$(BG_MOONPAK_DIR)/%.cpp.o: $(M_SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(ARM_CXX) $(ARM_CXXFLAGS) -mthumb -MMD -MP -c -o $@ $<

$(GBA_ELF): $(ARM_OBJS)
	$(ARM_CXX) -o $@ $(ARM_OBJS) $(ARM_LDFLAGS)

$(GBA_ROM): $(GBA_ELF) $(XFORM)
	$(ARM_OBJCOPY) -O binary $< $@
	$(XFORM) gbaFix $@ -p -t $(GAME_TITLE) -g $(GAME_CODE) -m $(MAKER_CODE) -v $(VERSION)

$(GBA_DUMP): $(GBA_ELF)
	$(ARM_OBJDUMP) -h -C -S $< > $@

$(ARM_OBJS): $(COMMON_HPP)

-include $(ARM_DEPS)
-include $(TESTS_DEPS)
-include $(XFORM_DEPS)
