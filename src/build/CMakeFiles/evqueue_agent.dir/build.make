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
include CMakeFiles/evqueue_agent.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/evqueue_agent.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/evqueue_agent.dir/flags.make

CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o: CMakeFiles/evqueue_agent.dir/flags.make
CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o: ../evqueue_agent.cpp
	$(CMAKE_COMMAND) -E cmake_progress_report /mnt/dev/evqueue-core/src/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o"
	/usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o -c /mnt/dev/evqueue-core/src/evqueue_agent.cpp

CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.i"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /mnt/dev/evqueue-core/src/evqueue_agent.cpp > CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.i

CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.s"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /mnt/dev/evqueue-core/src/evqueue_agent.cpp -o CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.s

CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o.requires:
.PHONY : CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o.requires

CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o.provides: CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o.requires
	$(MAKE) -f CMakeFiles/evqueue_agent.dir/build.make CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o.provides.build
.PHONY : CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o.provides

CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o.provides.build: CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o

# Object files for target evqueue_agent
evqueue_agent_OBJECTS = \
"CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o"

# External object files for target evqueue_agent
evqueue_agent_EXTERNAL_OBJECTS =

evqueue_agent: CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o
evqueue_agent: CMakeFiles/evqueue_agent.dir/build.make
evqueue_agent: CMakeFiles/evqueue_agent.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX executable evqueue_agent"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/evqueue_agent.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/evqueue_agent.dir/build: evqueue_agent
.PHONY : CMakeFiles/evqueue_agent.dir/build

CMakeFiles/evqueue_agent.dir/requires: CMakeFiles/evqueue_agent.dir/src/evqueue_agent.cpp.o.requires
.PHONY : CMakeFiles/evqueue_agent.dir/requires

CMakeFiles/evqueue_agent.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/evqueue_agent.dir/cmake_clean.cmake
.PHONY : CMakeFiles/evqueue_agent.dir/clean

CMakeFiles/evqueue_agent.dir/depend:
	cd /mnt/dev/evqueue-core/src/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /mnt/dev/evqueue-core /mnt/dev/evqueue-core /mnt/dev/evqueue-core/src/build /mnt/dev/evqueue-core/src/build /mnt/dev/evqueue-core/src/build/CMakeFiles/evqueue_agent.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/evqueue_agent.dir/depend

