# SPDX-License-Identifier: 0BSD

#
# Directories and targets
#

MOONPAK_DIR     := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

M_SCRIPTS_DIR   := $(MOONPAK_DIR)/scripts
M_SRC_DIR       := $(MOONPAK_DIR)/src
M_TESTS_DIR     := $(MOONPAK_DIR)/tests
M_TYPES_DIR     := $(MOONPAK_DIR)/types
M_XFORM_DIR     := $(MOONPAK_DIR)/xform

SCRIPT_EMBED    := $(M_SCRIPTS_DIR)/embed.ts
SCRIPT_FAMI     := $(M_SCRIPTS_DIR)/famistudio.ts
SCRIPT_SFXS     := $(M_SCRIPTS_DIR)/sfxs.ts
SCRIPT_TYPELIB  := $(M_SCRIPTS_DIR)/typelib.ts

project-dir      = $(if $(filter .,$(PROJECT_DIR)),$(1),$(PROJECT_DIR)/$(1))
P_BUILD_DIR     := $(call project-dir,build)
P_DATA_DIR      := $(call project-dir,data)
P_SCRIPTS_DIR   := $(call project-dir,scripts)
P_SRC_DIR       := $(call project-dir,src)
P_TESTS_DIR     := $(call project-dir,tests)
P_TYPES_DIR     := $(call project-dir,types)

D_DPCM_DIR      := $(P_DATA_DIR)/dpcm
D_SFXS_DIR      := $(P_DATA_DIR)/sfxs
D_SONGS_DIR     := $(P_DATA_DIR)/songs
D_SPSH_DIR      := $(P_DATA_DIR)/spritesheets
D_TLSH_DIR      := $(P_DATA_DIR)/tilesheets

B_COMMON_DIR    := $(P_BUILD_DIR)/common
B_GBA_DIR       := $(P_BUILD_DIR)/gba
B_TESTS_DIR     := $(P_BUILD_DIR)/tests
B_XFORM_DIR     := $(P_BUILD_DIR)/xform

BM_TYPES_DIR    := $(B_COMMON_DIR)/moonpak/types
BP_TYPES_DIR    := $(B_COMMON_DIR)/types
B_DATA_DIR      := $(B_COMMON_DIR)/data

B_DPCM_DIR      := $(B_DATA_DIR)/dpcm
B_SFXS_DIR      := $(B_DATA_DIR)/sfxs
B_SONGS_DIR     := $(B_DATA_DIR)/songs
B_SPSH_DIR      := $(B_DATA_DIR)/spritesheets
B_TLSH_DIR      := $(B_DATA_DIR)/tilesheets

BG_COMMON_DIR   := $(B_GBA_DIR)/common
BG_PROJECT_DIR  := $(B_GBA_DIR)/project
BG_MOONPAK_DIR  := $(B_GBA_DIR)/moonpak

GBA_ELF         := $(P_BUILD_DIR)/$(NAME).elf
GBA_DUMP        := $(P_BUILD_DIR)/$(NAME).dump
GBA_ROM         := $(P_BUILD_DIR)/$(NAME).gba
GBA_MAP         := $(P_BUILD_DIR)/$(NAME).map
TESTS           := $(P_BUILD_DIR)/tests/tests
XFORM           := $(P_BUILD_DIR)/xform/xform
