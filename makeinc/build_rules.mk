# Copyright (C) 2018 Martin Ejdestig <marejde@gmail.com>
# SPDX-License-Identifier: GPL-2.0+
#
# Build rules to include when all variables used for targets (CC_EXES and
# CXX_EXES) have been set (avoid .SECONDEXPANSION for simplicity). Also
# includes all header dependency files to avoid forcing use of .DEFAULT_GOAL
# which would be required if they were to be included before default target.
#
# See build.mk for details.

-include $(deps)

$(CC_EXES):
	@mkdir -p $(dir $@)
	$(hush_link) $(CC) -o $@ $^ $(LDFLAGS)

$(CXX_EXES):
	@mkdir -p $(dir $@)
	$(hush_link) $(CXX) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(hush_compile) $(CC) $(CFLAGS) $(cflags_deps) -c -o $@ $<

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(hush_compile) $(CXX) $(CXXFLAGS) $(cflags_deps) -c -o $@ $<

$(BUILD_DIR)/%.a:
	@mkdir -p $(dir $@)
	$(hush_create) ar -r $@ $^ 2> /dev/null
