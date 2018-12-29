# Copyright (C) 2018 Martin Ejdestig <marejde@gmail.com>
# SPDX-License-Identifier: GPL-2.0+
#
# Simple build rules and variables for building C/C++ projects with automatic
# header dependency file generation, pkg-config support for external libraries,
# silencing of compilation commands, etc.
#
# Include this file at the top of your Makefile (at least before using e.g.
# BUILD_DIR) and build_rules.mk at the end (when *_EXES have been set). See
# example below.
#
# Variables to set in other Makefile/*.mk files or on command line:
#   BUILD_DIR
#     Build output directory. Set to build by default.
#   DEBUG
#     Build a debug build or not. A value other than n will result in a
#     non-debug build. Set to n by default.
#   VERBOSE
#     If set to n compilation commands will be hidden and short messages will be
#     printed instead. A value other than n will output compilation commands.
#     Set to n by default.
#
# Variables to set in other Makefile/*.mk files:
#   CC_EXES
#     Executables that will be linked with CC.
#   CXX_EXES
#     Executables that will be linked with CXX.
#   OBJS
#     Object files of executables added as dependencies to CC_EXES and CXX_EXES.
#     Needed for including generated dependency files for Make to automatically
#     rebuild objects when header files change.
#   PKG_CONFIG_LIBS
#     pkg-config modules to use for compilation and linking. Passed directly to
#     pkg-config which means >= etc can be used to require specific versions.
#     If multiple executables/libraries are built in the same Makefile this
#     should be set as a target specific variable. See example below. Defaults
#     to "".
#   PKG_CONFIG_LIBS_CFLAGS
#     pkg-config modules to use for compilation (pkg-config --cflags). Defaults
#     to PKG_CONFIG_LIBS. Use PKG_CONFIG_LIBS if there is no specific need to
#     explicitly discern between modules for compilation and linking. -I flags
#     are replaced with -isystem to prevent warnings from external dependency
#     headers even if they are not located in any of the sysroot include
#     directories (/usr/include, C_INCLUDE_PATH, CPLUS_INCLUDE_PATH etc.)
#   PKG_CONFIG_LIBS_LDFLAGS
#     pkg-config modules to use for linking (pkg-config --libs). Defaults to
#     PKG_CONFIG_LIBS. Use PKG_CONFIG_LIBS if there is no specific need to
#     explicitly discern between modules for compilation and linking.
#   CFLAGS/CXXFLAGS
#     Flags passed to CC/CXX when compiling. Defaults to some warnings, debug or
#     optimization flags depending on DEBUG and pkg-config --cflags if target
#     has PKG_CONFIG_LIBS/PKG_CONFIG_LIBS_CFLAGS set.
#   LDFLAGS
#     Flags passed to CC/CXX when linking. Defaults to pkg-config --libs if
#     target has PKG_CONFIG_LIBS/PKG_CONFIG_LIBS_LDFLAGS set.
#
# Lower case variables are internal implementation details.
#
# Example Makefile that builds a static library that uses libpng and an
# executable that uses the static library plus an executable that uses the
# static library and GTK+:
#
# include makeinc/build.mk
#
# .PHONY: all clean
#
# all:
#
# clean:
#	rm -rf $(BUILD_DIR)
#
# objs_from_src = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(shell find $(1) -name "*.cpp"))
#
# LIB = $(BUILD_DIR)/libfoo.a
# LIB_PKG_CONFIG_LIBS = libpng >= 1.6
# LIB_OBJS = $(call objs_from_src,libfoo)
# OBJS += $(LIB_OBJS)
#
# $(LIB): PKG_CONFIG_LIBS = $(LIB_PKG_CONFIG_LIBS)
# $(LIB): $(LIB_OBJS)
#
# UI_CLI_EXE = $(BUILD_DIR)/foo
# UI_CLI_OBJS = $(call objs_from_src,ui_cli)
# CXX_EXES += $(UI_CLI_EXE)
# OBJS += $(UI_CLI_OBJS)
#
# $(UI_CLI_EXE): PKG_CONFIG_LIBS_LDFLAGS = $(LIB_PKG_CONFIG_LIBS)
# $(UI_CLI_EXE): $(UI_CLI_OBJS) $(LIB)
# all: $(UI_CLI_EXE)
#
# UI_GTK_EXE = $(BUILD_DIR)/foo-gtk
# UI_GTK_PKG_CONFIG_LIBS = gtk+-3.0 >= 3.16 glib-2.0 >= 2.44
# UI_GTK_OBJS = $(call objs_from_src,ui_gtk)
# CXX_EXES += $(UI_GTK_EXE)
# OBJS += $(UI_GTK_OBJS)
#
# $(UI_GTK_EXE): PKG_CONFIG_LIBS = $(UI_GTK_PKG_CONFIG_LIBS)
# $(UI_GTK_EXE): PKG_CONFIG_LIBS_LDFLAGS = $(UI_GTK_PKG_CONFIG_LIBS) $(LIB_PKG_CONFIG_LIBS)
# $(UI_GTK_EXE): $(UI_GTK_OBJS) $(LIB)
# all: $(UI_GTK_EXE)
#
# include makeinc/build_rules.mk
#

BUILD_DIR ?= build
DEBUG ?= n
VERBOSE ?= n

CC_EXES =
CXX_EXES =
OBJS =

PKG_CONFIG_LIBS =
PKG_CONFIG_LIBS_CFLAGS = $(PKG_CONFIG_LIBS)
PKG_CONFIG_LIBS_LDFLAGS = $(PKG_CONFIG_LIBS)

deps = $(OBJS:.o=.d)

ifeq ($(VERBOSE),n)
 hush_create = @echo "Creating $@";
 hush_compile = @echo "Compiling $<";
 hush_link = @echo "Linking $@";
else
 hush_create =
 hush_compile =
 hush_link =
endif

cflags_warnings = -Wall -Wextra -Wpedantic -Werror -Wformat=2
ifneq ($(DEBUG),n)
 cflags_debug_or_optimize = -g
else
 cflags_debug_or_optimize = -DNDEBUG -O3
endif
cflags_deps = -MD -MP -MT $@

pkg_config_cflags = $(if $(PKG_CONFIG_LIBS_CFLAGS),$(patsubst -I%,-isystem%,$(shell pkg-config --cflags '$(PKG_CONFIG_LIBS_CFLAGS)')))
pkg_config_ldflags = $(if $(PKG_CONFIG_LIBS_LDFLAGS),$(shell pkg-config --libs '$(PKG_CONFIG_LIBS_LDFLAGS)'))

CFLAGS += $(cflags_warnings) $(cflags_debug_or_optimize) $(pkg_config_cflags)
CXXFLAGS += $(cflags_warnings) $(cflags_debug_or_optimize) $(pkg_config_cflags)
LDFLAGS += $(pkg_config_ldflags)
