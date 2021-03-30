add_executable(in-memory in-memory.c)
target_link_libraries(in-memory zip)
target_include_directories(in-memory PRIVATE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR})
