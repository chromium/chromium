set(LIBRARYNAME BlinkGCPlugin)

set(plugin_sources
  BadPatternFinder.cpp
  BlinkGCPlugin.cpp
  BlinkGCPluginConsumer.cpp
  CheckDispatchVisitor.cpp
  CheckFieldsVisitor.cpp
  CheckFinalizerVisitor.cpp
  CheckForbiddenFieldsVisitor.cpp
  CheckGCRootsVisitor.cpp
  CheckTraceVisitor.cpp
  CollectVisitor.cpp
  Config.cpp
  DiagnosticsReporter.cpp
  Edge.cpp
  RecordInfo.cpp)

# Clang doesn't support loadable modules on Windows. Unfortunately, building
# the plugin as a static library and linking clang against it doesn't work.
# Since clang doesn't reference any symbols in our static library, the linker
# strips it out completely.
# Instead, we rely on the fact that the SOURCES property of a target is not
# read-only after CMake 3.1 and use it to compile the plugin directly into
# clang.
cmake_minimum_required(VERSION 3.1)
# Paths must be absolute, since we're modifying a target in another directory.
set(absolute_sources "")
foreach(source ${plugin_sources})
  list(APPEND absolute_sources ${CMAKE_CURRENT_SOURCE_DIR}/${source})
endforeach()
set_property(TARGET clang APPEND PROPERTY SOURCES ${absolute_sources})

cr_add_test(blink_gc_plugin_test
  python3 tests/test.py
  ${CMAKE_BINARY_DIR}/bin/clang
  )
