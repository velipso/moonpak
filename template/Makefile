# SPDX-License-Identifier: 0BSD

# project settings
PROJECT_DIR    := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
MOONPAK        := $(PROJECT_DIR)/../moonpak
NAME           := template
GAME_TITLE     := "MP TEMPLATE"
GAME_CODE      := "CMPE"
MAKER_CODE     := "77"
VERSION        := 0

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

include $(MOONPAK)/moonpak.mk
