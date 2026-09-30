# SPDX-License-Identifier: 0BSD

# initial settings
PROJECT_DIR    := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
MOONPAK        := $(PROJECT_DIR)/../../moonpak
NAME           := overthemoon
GAME_TITLE     := "OVERTHEMOON"
GAME_CODE      := "COTE"
MAKER_CODE     := "77"
VERSION        := 0

# initialize moonpak
include $(MOONPAK)/moonpak-init.mk

# GBA ARM compiler settings
DEFINES        :=
INCLUDES       :=
CFLAGS         := -Wall -O3
CXXFLAGS       := -Wall -Wno-unused-function -O3
ASFLAGS        :=
LDFLAGS        :=
LDLIBS         :=

# tests compiler settings
TESTS_DEFINES  :=
TESTS_INCLUDES :=
TESTS_CFLAGS   := -Wall -O3
TESTS_CXXFLAGS := -Wall -Wno-unused-function -O3
TESTS_LDFLAGS  :=
TESTS_LDLIBS   :=

#
# Tilemap build
#

P_TILEMAPS_DIR := $(P_DATA_DIR)/tilemaps
B_TILEMAPS_DIR := $(B_DATA_DIR)/tilemaps
WORLD_JSON     := $(P_TILEMAPS_DIR)/world.json
WORLD_BIN      := $(B_TILEMAPS_DIR)/world.bin
WORLD_HPP      := $(B_TILEMAPS_DIR)/world.hpp
WORLD_CPP      := $(B_TILEMAPS_DIR)/world.cpp

# add world hpp/cpp to build
COMMON_HPP     += $(WORLD_HPP)
COMMON_CPP     += $(WORLD_CPP)

$(WORLD_BIN): $(WORLD_JSON) $(P_SCRIPTS_DIR)/map.js
	@mkdir -p $(@D)
	$(NODE) $(P_SCRIPTS_DIR)/map.js -o $(WORLD_BIN) $(WORLD_JSON)

$(WORLD_HPP) $(WORLD_CPP): $(WORLD_BIN) $(SCRIPT_EMBED)
	@mkdir -p $(@D)
	$(NODE) $(SCRIPT_EMBED) -o $(WORLD_HPP) -o $(WORLD_CPP) -n $(B_COMMON_DIR) $(WORLD_BIN)

# finish and build
include $(MOONPAK)/moonpak-done.mk
