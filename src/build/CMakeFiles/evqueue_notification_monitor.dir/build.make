# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /mnt/dev/evqueue-core

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /mnt/dev/evqueue-core/src/build

# Include any dependencies generated for this target.
include CMakeFiles/evqueue_notification_monitor.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/evqueue_notification_monitor.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/evqueue_notification_monitor.dir/flags.make

CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o: CMakeFiles/evqueue_notification_monitor.dir/flags.make
CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o: ../evqueue_notification_monitor.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /mnt/dev/evqueue-core/src/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o"
	/usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o -c /mnt/dev/evqueue-core/src/evqueue_notification_monitor.cpp

CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.i"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /mnt/dev/evqueue-core/src/evqueue_notification_monitor.cpp > CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.i

CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.s"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /mnt/dev/evqueue-core/src/evqueue_notification_monitor.cpp -o CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.s

CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o.requires:
.PHONY : CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o.requires

CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o.provides: CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o.requires
	$(MAKE) -f CMakeFiles/evqueue_notification_monitor.dir/build.make CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o.provides.build
.PHONY : CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o.provides

CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o.provides.build: CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o

CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o: CMakeFiles/evqueue_notification_monitor.dir/flags.make
CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o: ../tools_ipc.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /mnt/dev/evqueue-core/src/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o"
	/usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o -c /mnt/dev/evqueue-core/src/tools_ipc.cpp

CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.i"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /mnt/dev/evqueue-core/src/tools_ipc.cpp > CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.i

CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.s"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /mnt/dev/evqueue-core/src/tools_ipc.cpp -o CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.s

CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o.requires:
.PHONY : CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o.requires

CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o.provides: CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o.requires
	$(MAKE) -f CMakeFiles/evqueue_notification_monitor.dir/build.make CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o.provides.build
.PHONY : CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o.provides

CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o.provides.build: CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o

# Object files for target evqueue_notification_monitor
evqueue_notification_monitor_OBJECTS = \
"CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o" \
"CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o"

# External object files for target evqueue_notification_monitor
evqueue_notification_monitor_EXTERNAL_OBJECTS =

evqueue_notification_monitor: CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o
evqueue_notification_monitor: CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o
evqueue_notification_monitor: CMakeFiles/evqueue_notification_monitor.dir/build.make
evqueue_notification_monitor: CMakeFiles/evqueue_notification_monitor.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable evqueue_notification_monitor"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/evqueue_notification_monitor.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/evqueue_notification_monitor.dir/build: evqueue_notification_monitor
.PHONY : CMakeFiles/evqueue_notification_monitor.dir/build

CMakeFiles/evqueue_notification_monitor.dir/requires: CMakeFiles/evqueue_notification_monitor.dir/src/evqueue_notification_monitor.cpp.o.requires
CMakeFiles/evqueue_notification_monitor.dir/requires: CMakeFiles/evqueue_notification_monitor.dir/src/tools_ipc.cpp.o.requires
.PHONY : CMakeFiles/evqueue_notification_monitor.dir/requires

CMakeFiles/evqueue_notification_monitor.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/evqueue_notification_monitor.dir/cmake_clean.cmake
.PHONY : CMakeFiles/evqueue_notification_monitor.dir/clean

CMakeFiles/evqueue_notification_monitor.dir/depend:
	cd /mnt/dev/evqueue-core/src/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/dev/evqueue-core /mnt/dev/evqueue-core /mnt/dev/evqueue-core/src/build /mnt/dev/evqueue-core/src/build /mnt/dev/evqueue-core/src/build/CMakeFiles/evqueue_notification_monitor.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/evqueue_notification_monitor.dir/depend

