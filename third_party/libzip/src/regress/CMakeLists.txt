check_function_exists(getopt HAVE_GETOPT)

set(TEST_PROGRAMS
  add_from_filep
  can_clone_file
  fopen_unchanged
  fseek
)

set(GETOPT_USERS
  fread
  tryopen
)

set(HOLE_USERS
  hole
  ziptool_regress
  ziptool_regress_nonrandom
)

set(ZIP_PROGRAMS ${TEST_PROGRAMS} ${GETOPT_USERS} ${HOLE_USERS})

foreach(PROGRAM IN LISTS ZIP_PROGRAMS)
  if (PROGRAM STREQUAL "ziptool_regress_nonrandom")
    add_executable(${PROGRAM} programs/ziptool_regress.c)
    target_link_libraries(${PROGRAM} zip_nonrandom)
  else()
    add_executable(${PROGRAM} programs/${PROGRAM}.c)
    target_link_libraries(${PROGRAM} zip)
    if (ENABLE_COVERAGE)
      target_compile_options(${PROGRAM} PRIVATE -coverage)
      target_link_options(${PROGRAM} PRIVATE -coverage)
    endif()
  endif()
  target_include_directories(${PROGRAM} PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR})
endforeach()

# both programs using source_hole.c also use getopt
if(NOT HAVE_GETOPT)
  foreach(PROGRAM IN LISTS GETOPT_USERS HOLE_USERS)
    target_sources(${PROGRAM} PRIVATE ../src/getopt.c)
    target_include_directories(${PROGRAM} PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/src)
  endforeach()
endif(NOT HAVE_GETOPT)

foreach(PROGRAM IN LISTS HOLE_USERS)
  target_sources(${PROGRAM} PRIVATE programs/source_hole.c)
endforeach()

foreach(PROGRAM IN LISTS FUZZ_PROGRAMS)
  target_sources(${PROGRAM} PRIVATE programs/fuzz_main.c)
endforeach()

add_executable(files_differ programs/files_differ.c)

# for including ziptool.c
target_include_directories(ziptool_regress PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(ziptool_regress_nonrandom PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/src)


add_custom_target(cleanup
    COMMAND ${CMAKE_COMMAND} -DDIR=${PROJECT_BINARY_DIR}/regress -P ${PROJECT_SOURCE_DIR}/regress/support/cleanup.cmake
    )

add_custom_target(testinput
  ALL
  VERBATIM
  COMMAND ${CMAKE_COMMAND} -E tar x ${PROJECT_SOURCE_DIR}/regress/data/manyfiles-zip.zip
  COMMAND ${CMAKE_COMMAND} -E tar x ${PROJECT_SOURCE_DIR}/regress/data/bigzero-zip.zip
  DEPENDS ${PROJECT_SOURCE_DIR}/regress/data/manyfiles-zip.zip ${PROJECT_SOURCE_DIR}/regress/data/bigzero-zip.zip
)

set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
  bigzero.zip
  manyfiles-133000.zip
  manyfiles-65536.zip
  manyfiles-fewer.zip
  manyfiles-more.zip
  manyfiles-zip64-modulo.zip
  manyfiles-zip64.zip
  manyfiles.zip
)

set(path "$<TARGET_FILE_DIR:zip>;$ENV{PATH}")
if (TARGET zlib)
  set(path "$<TARGET_FILE_DIR:zlib>;${path}")
endif()
string(REPLACE ";" "\\;" path "${path}")

# set(ENV{srcdir} ${PROJECT_SOURCE_DIR}/regress)

file(GLOB EXTRA_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/*.test)
foreach(FULL_CASE IN LISTS EXTRA_TESTS)
  get_filename_component(CASE ${FULL_CASE} NAME)
  add_test(NAME ${CASE} COMMAND ${NIHTEST} -v ${CASE})
  # TODO: add --bin-sub-directory $<CONFIG>
  set_tests_properties(${CASE} PROPERTIES SKIP_RETURN_CODE 77)
  set_tests_properties(${CASE} PROPERTIES ENVIRONMENT "PATH=${path}")
endforeach()

set(XFAIL_TESTS
#  zipcmp_zip_dir_slash.test
)

foreach(CASE ${XFAIL_TESTS})
  set_tests_properties(${CASE} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

configure_file(support/nihtest.conf.in nihtest.conf @ONLY)

if(ENABLE_COVERAGE)
  add_custom_target(coverage
          # gather data
          COMMAND ${LCOV} --directory . --capture --output-file coverage.info
          # generate report
          COMMAND ${GENHTML} --demangle-cpp -o coverage coverage.info WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${CMAKE_COMMAND} -E cmake_echo_color "Open coverage/index.html in browser to view results."
  )
endif()
