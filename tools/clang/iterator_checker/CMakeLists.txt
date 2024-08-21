# This plugin is the only one depending on the clangAnalysisFlowSensitive
# library, so it is not built inside /tools/clang/plugin/, but uses its own
# directory. This is similar to the blink_gc_plugin.

# Clang doesn't support loadable modules on Windows. Unfortunately, building
# the plugin as a static library and linking clang against it doesn't work.
# Since clang doesn't reference any symbols in our static library, the linker
# strips it out completely.
#
# Instead we add our sources directly into clang:
# - Adding sources into target created from different directory is allowed by
#   default since CMake 3.1.
# - Adding link libraries into target created from different directory is
#   allowed by default since CMake 3.13.
cmake_minimum_required(VERSION 3.13)

target_sources(clang PRIVATE IteratorChecker.cpp)
target_link_libraries(clang PRIVATE clangAnalysisFlowSensitive)
target_link_libraries(clang PRIVATE clangAnalysisFlowSensitiveModels)

cr_add_test(iterator_checker_test
  python3 tests/test.py
  ${CMAKE_BINARY_DIR}/bin/clang
)
