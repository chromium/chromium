check_function_exists(getopt HAVE_GETOPT)

set(TEST_PROGRAMS
  add_from_filep
  can_clone_file
  fopen_unchanged
  fseek
  fuzz_main
  nonrandomopentest
)

set(GETOPT_USERS
  fread
  tryopen
)

set(HOLE_USERS
  hole
  ziptool_regress
)

set(ZIP_PROGRAMS ${TEST_PROGRAMS} ${GETOPT_USERS} ${HOLE_USERS})

foreach(PROGRAM IN LISTS ZIP_PROGRAMS)
  add_executable(${PROGRAM} ${PROGRAM}.c)
  target_include_directories(${PROGRAM} PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR})
  target_link_libraries(${PROGRAM} zip)
endforeach()

# both programs using source_hole.c also use getopt
if(NOT HAVE_GETOPT)
  foreach(PROGRAM IN LISTS GETOPT_USERS HOLE_USERS)
    target_sources(${PROGRAM} PRIVATE ../src/getopt.c)
    target_include_directories(${PROGRAM} PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/src)
  endforeach()
endif(NOT HAVE_GETOPT)

foreach(PROGRAM IN LISTS HOLE_USERS)
  target_sources(${PROGRAM} PRIVATE source_hole.c)
endforeach()

# for including ziptool.c
target_include_directories(ziptool_regress PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/src)

set(DL_USERS
  # malloc
  nonrandomopen
)

foreach(PROGRAM IN LISTS DL_USERS)
  add_library(${PROGRAM} MODULE ${PROGRAM}.c)
  target_link_libraries(${PROGRAM} ${CMAKE_DL_LIBS})
  target_include_directories(${PROGRAM} PRIVATE BEFORE ${PROJECT_SOURCE_DIR}/lib ${PROJECT_BINARY_DIR})
endforeach()

add_custom_target(cleanup
    COMMAND ${CMAKE_COMMAND} -DDIR=${PROJECT_BINARY_DIR}/regress -P ${PROJECT_SOURCE_DIR}/regress/cleanup.cmake
    )

add_custom_target(testinput
  ALL
  VERBATIM
  COMMAND ${CMAKE_COMMAND} -E tar x ${PROJECT_SOURCE_DIR}/regress/manyfiles-zip.zip
  COMMAND ${CMAKE_COMMAND} -E tar x ${PROJECT_SOURCE_DIR}/regress/bigzero-zip.zip
  DEPENDS ${PROJECT_SOURCE_DIR}/regress/manyfiles-zip.zip ${PROJECT_SOURCE_DIR}/regress/bigzero-zip.zip
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

set(EXTRA_TESTS
	add_dir.test
	add_from_buffer.test
	add_from_file.test
	add_from_file_duplicate.test
	add_from_file_twice_duplicate.test
	add_from_filep.test
	add_from_stdin.test
	add_from_zip_closed.test
	add_from_zip_deflated.test
	add_from_zip_deflated2.test
	add_from_zip_partial_deflated.test
	add_from_zip_partial_stored.test
	add_from_zip_stored.test
	add_stored.test
	add_stored_in_memory.test
	buffer-fragment-read.test
	buffer-fragment-write.test
	cancel_45.test
	cancel_90.test
	clone-buffer-add.test
	clone-buffer-delete.test
	clone-buffer-replace.test
	clone-fs-add.test
	clone-fs-delete.test
	clone-fs-replace.test
	cm-default.test
	count_entries.test
	decrypt-correct-password-aes128.test
	decrypt-correct-password-aes192.test
	decrypt-correct-password-aes256.test
	decrypt-correct-password-pkware.test
	decrypt-no-password-aes256.test
	decrypt-wrong-password-aes128.test
	decrypt-wrong-password-aes192.test
	decrypt-wrong-password-aes256.test
	decrypt-wrong-password-pkware.test
	delete_add_same.test
	delete_invalid.test
	delete_last.test
	delete_multiple_last.test
	delete_multiple_partial.test
	delete_renamed_rename.test
	encrypt.test
	encryption-nonrandom-aes128.test
	encryption-nonrandom-aes192.test
	encryption-nonrandom-aes256.test
	encryption-nonrandom-pkware.test
	encryption-remove.test
	extra_add.test
	extra_add_multiple.test
	extra_count.test
	extra_count_by_id.test
	extra_count_ignore_zip64.test
	extra_delete.test
	extra_delete_by_id.test
	extra_field_align.test
	extra_get.test
	extra_get_by_id.test
	extra_set.test
	extra_set_modify_c.test
	extra_set_modify_l.test
	fdopen_ok.test
	file_comment_encmismatch.test
	fopen_unchanged.test
	fread.test
	fseek_deflated.test
	fseek_fail.test
	fseek_ok.test
	get_comment.test
	junk_at_end.test
	junk_at_start.test
	name_locate.test
	open_cons_extrabytes.test
	open_empty.test
	open_empty_2.test
	open_extrabytes.test
	open_file_count.test
	open_filename_duplicate.test
	open_filename_duplicate_consistency.test
	open_filename_duplicate_empty.test
	open_filename_duplicate_empty_consistency.test
	open_filename_empty.test
	open_incons.test
	open_many_fail.test
	open_many_ok.test
	open_multidisk.test
	open_new_but_exists.test
	open_new_ok.test
	open_nonarchive.test
	open_nosuchfile.test
	open_ok.test
	open_too_short.test
	open_truncate.test
	open_zip64_3mf.test
	open_zip64_ok.test
	preload.test
	progress.test
	rename_ascii.test
	rename_cp437.test
	rename_deleted.test
	rename_fail.test
	rename_ok.test
	rename_utf8.test
	rename_utf8_encmismatch.test
	set_comment_all.test
	set_comment_localonly.test
	set_comment_removeglobal.test
	set_comment_revert.test
	set_compression_bzip2_to_deflate.test
	set_compression_deflate_to_bzip2.test
	set_compression_deflate_to_deflate.test
	set_compression_deflate_to_store.test
	set_compression_store_to_bzip2.test
	set_compression_store_to_deflate.test
	set_compression_store_to_store.test
	set_compression_store_to_xz.test
	set_compression_unknown.test
	set_compression_xz_to_store.test
	set_file_dostime.test
	stat_index_cp437_guess.test
	stat_index_cp437_raw.test
	stat_index_cp437_strict.test
	stat_index_fileorder.test
	stat_index_streamed.test
	stat_index_streamed_zip64.test
	stat_index_utf8_guess.test
	stat_index_utf8_raw.test
	stat_index_utf8_strict.test
	stat_index_utf8_unmarked_strict.test
	stat_index_zip64.test
	utf-8-standardization.test
	zip-in-archive-comment.test
	zip64_creation.test
	zip64_stored_creation.test
)


set(path "$<TARGET_FILE_DIR:zip>;$ENV{PATH}")
if (TARGET zlib)
  set(path "$<TARGET_FILE_DIR:zlib>;${path}")
endif()
string(REPLACE ";" "\\;" path "${path}")

set(ENV{srcdir} ${PROJECT_SOURCE_DIR}/regress)

foreach(CASE IN LISTS EXTRA_TESTS)
  add_test(NAME ${CASE} COMMAND ${PERL_EXECUTABLE} ${PROJECT_BINARY_DIR}/regress/runtest --bin-sub-directory $<CONFIG> ${PROJECT_SOURCE_DIR}/regress/${CASE})
  set_tests_properties(${CASE} PROPERTIES SKIP_RETURN_CODE 77)
  set_tests_properties(${CASE} PROPERTIES ENVIRONMENT "PATH=${path}")
endforeach()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
