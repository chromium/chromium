set(FUZZ_PROGRAMS
  zip_read_file_fuzzer
  zip_read_fuzzer
  zip_read_metadata_fuzzer
  zip_write_encrypt_aes256_file_fuzzer
  zip_write_encrypt_pkware_file_fuzzer
  zip_write_roundtrip_fuzzer
)

foreach(PROGRAM IN LISTS FUZZ_PROGRAMS)
  add_executable(${PROGRAM} ${PROGRAM}.c)
  target_sources(${PROGRAM} PRIVATE fuzz_main.c)
  target_include_directories(${PROGRAM} PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR})
  target_link_libraries(${PROGRAM} zip)
endforeach()

add_custom_target(list-fuzzers
        COMMAND echo FUZZERS: ${FUZZ_PROGRAMS}
)
