link_directories(${PROJECT_BINARY_DIR}/common ${PROJECT_BINARY_DIR}/hfs ${PROJECT_BINARY_DIR}/dmg)

add_executable(hdutil hdutil.c)

target_link_libraries (hdutil dmg hfs common)

install(TARGETS hdutil DESTINATION bin)

