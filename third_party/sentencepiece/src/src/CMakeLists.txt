# Copyright 2018 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.!

if (SPM_USE_EXTERNAL_ABSL)
  set(ABSL_FLAGS_SRCS "")
  set(ABSL_STRINGS_SRCS "")
  list(APPEND SPM_LIBS absl::strings)
  list(APPEND SPM_LIBS absl::flags)
  list(APPEND SPM_LIBS absl::flags_parse)
  if (MSVC)
   add_definitions("/D_USE_EXTERNAL_ABSL")
  else()
   add_definitions("-D_USE_EXTERNAL_ABSL")
  endif()
else()
  set(ABSL_FLAGS_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/absl/flags/flag.cc)
endif()

if (SPM_USE_BUILTIN_PROTOBUF)
  set(SPM_PROTO_HDRS builtin_pb/sentencepiece.pb.h)
  set(SPM_PROTO_SRCS builtin_pb/sentencepiece.pb.cc)
  set(SPM_MODEL_PROTO_HDRS builtin_pb/sentencepiece_model.pb.h)
  set(SPM_MODEL_PROTO_SRCS builtin_pb/sentencepiece_model.pb.cc)
  set(PROTOBUF_LITE_LIBRARY "")
  set(PROTOBUF_LITE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/arena.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/arenastring.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/bytestream.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/coded_stream.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/common.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/extension_set.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/generated_enum_util.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/generated_message_table_driven_lite.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/generated_message_util.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/implicit_weak_message.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/int128.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/io_win32.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/message_lite.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/parse_context.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/repeated_field.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/status.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/statusor.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/stringpiece.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/stringprintf.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/structurally_valid.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/strutil.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/time.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/wire_format_lite.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/zero_copy_stream.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/zero_copy_stream_impl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite/zero_copy_stream_impl_lite.cc)
  if (MSVC)
    add_definitions("/DHAVE_PTHREAD /wd4018 /wd4514")
  else()
    add_definitions("-pthread -DHAVE_PTHREAD=1 -Wno-sign-compare -Wno-deprecated-declarations")
  endif()
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/protobuf-lite)
  include_directories(builtin_pb)
else()
  find_package(Protobuf REQUIRED)
  include_directories(${Protobuf_INCLUDE_DIRS})
  protobuf_generate_cpp(SPM_PROTO_SRCS SPM_PROTO_HDRS sentencepiece.proto)
  protobuf_generate_cpp(SPM_MODEL_PROTO_SRCS SPM_MODEL_PROTO_HDRS sentencepiece_model.proto)
  set(PROTOBUF_LITE_SRCS "")
  include_directories(${PROTOBUF_INCLUDE_DIR})
  if (MSVC)
    add_definitions("/D_USE_EXTERNAL_PROTOBUF")
  else()
    add_definitions("-D_USE_EXTERNAL_PROTOBUF")
  endif()
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../third_party)

if (MSVC)
  add_definitions("/D_USE_INTERNAL_STRING_VIEW")
else()
  add_definitions("-D_USE_INTERNAL_STRING_VIEW")
endif()

set(SPM_SRCS
  ${PROTOBUF_LITE_SRCS}
  ${SPM_PROTO_HDRS}
  ${SPM_PROTO_SRCS}
  ${SPM_MODEL_PROTO_HDRS}
  ${SPM_MODEL_PROTO_SRCS}
  bpe_model.h
  common.h
  normalizer.h
  util.h
  freelist.h
  filesystem.h
  init.h
  sentencepiece_processor.h
  word_model.h
  model_factory.h
  char_model.h
  model_interface.h
  testharness.h
  unigram_model.h
  bpe_model.cc
  char_model.cc
  error.cc
  filesystem.cc
  model_factory.cc
  model_interface.cc
  normalizer.cc
  sentencepiece_processor.cc
  unigram_model.cc
  util.cc
  word_model.cc
  ${ABSL_STRINGS_SRCS}
  ${ABSL_FLAGS_SRCS})

set(SPM_TRAIN_SRCS
  ${SPM_PROTO_HDRS}
  ${SPM_MODEL_PROTO_HDRS}
  builder.h
  normalization_rule.h
  unicode_script.h
  unicode_script_map.h
  trainer_factory.h
  trainer_interface.h
  unigram_model_trainer.h
  word_model_trainer.h
  char_model_trainer.h
  bpe_model_trainer.h
  sentencepiece_trainer.h
  pretokenizer_for_training.h
  builder.cc
  unicode_script.cc
  trainer_factory.cc
  trainer_interface.cc
  unigram_model_trainer.cc
  word_model_trainer.cc
  char_model_trainer.cc
  bpe_model_trainer.cc
  sentencepiece_trainer.cc
  pretokenizer_for_training.cc)

set(SPM_TEST_SRCS
  ${SPM_PROTO_HDRS}
  ${SPM_MODEL_PROTO_HDRS}
  testharness.h
  bpe_model_test.cc
  bpe_model_trainer_test.cc
  builder_test.cc
  char_model_test.cc
  char_model_trainer_test.cc
  filesystem_test.cc
  init_test.cc
  model_factory_test.cc
  model_interface_test.cc
  normalizer_test.cc
  sentencepiece_processor_test.cc
  sentencepiece_trainer_test.cc
  test_main.cc
  testharness.cc
  trainer_factory_test.cc
  trainer_interface_test.cc
  unicode_script_test.cc
  unigram_model_test.cc
  unigram_model_trainer_test.cc
  util_test.cc
  word_model_test.cc
  word_model_trainer_test.cc
  pretokenizer_for_training_test.cc)

find_package(Threads REQUIRED)

list(APPEND SPM_LIBS ${PROTOBUF_LITE_LIBRARY} Threads::Threads)

if (SPM_ENABLE_NFKC_COMPILE)
  find_package(ICU 4.4 COMPONENTS i18n data uc REQUIRED)
  include_directories(${ICU_INCLUDE_DIRS})
  add_definitions(-DENABLE_NFKC_COMPILE)
  list(APPEND SPM_LIBS ICU::i18n ICU::data ICU::uc)
endif()

if (SPM_ENABLE_TCMALLOC)
  if (SPM_TCMALLOC_STATIC)
    find_library(TCMALLOC_LIB NAMES libtcmalloc_minimal.a)
  else()
    find_library(TCMALLOC_LIB NAMES tcmalloc_minimal)
  endif()
  if (TCMALLOC_LIB)
    message(STATUS "Found TCMalloc: ${TCMALLOC_LIB}")
    list(APPEND SPM_LIBS ${TCMALLOC_LIB})
    add_definitions(-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free)
  else()
    message(STATUS "Not Found TCMalloc: ${TCMALLOC_LIB}")
  endif()
endif()

if ((${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm") OR
    (${CMAKE_SYSTEM_PROCESSOR} MATCHES "mips") OR
    (${CMAKE_SYSTEM_PROCESSOR} MATCHES "m68k") OR
    (${CMAKE_SYSTEM_PROCESSOR} MATCHES "ppc") OR
    (${CMAKE_SYSTEM_PROCESSOR} MATCHES "powerpc") OR
    (${CMAKE_SYSTEM_PROCESSOR} MATCHES "aarch") OR
    (${CMAKE_SYSTEM_PROCESSOR} MATCHES "sh4"))
  find_library(ATOMIC_LIB NAMES atomic libatomic.so libatomic.so.1)
  if (ATOMIC_LIB)
    message(STATUS "Found atomic: ${ATOMIC_LIB}")
    list(APPEND SPM_LIBS "atomic")
  endif()
endif()


if (SPM_ENABLE_SHARED)
  add_library(sentencepiece SHARED ${SPM_SRCS})
  add_library(sentencepiece_train SHARED ${SPM_TRAIN_SRCS})
endif()

add_library(sentencepiece-static STATIC ${SPM_SRCS})
add_library(sentencepiece_train-static STATIC ${SPM_TRAIN_SRCS})

target_link_libraries(sentencepiece-static INTERFACE ${SPM_LIBS})
target_link_libraries(sentencepiece_train-static INTERFACE sentencepiece-static ${SPM_LIBS})

if (SPM_ENABLE_SHARED)
  target_link_libraries(sentencepiece ${SPM_LIBS})
  target_link_libraries(sentencepiece_train ${SPM_LIBS} sentencepiece)
  set(SPM_INSTALLTARGETS sentencepiece sentencepiece_train sentencepiece-static sentencepiece_train-static)
  set_target_properties(sentencepiece sentencepiece_train PROPERTIES SOVERSION 0 VERSION 0.0.0)
  set_target_properties(sentencepiece PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS YES)
  set_target_properties(sentencepiece_train PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS YES)
  if (MSVC)
    set_target_properties(sentencepiece PROPERTIES IMPORT_SUFFIX "_import.lib")
    set_target_properties(sentencepiece_train PROPERTIES IMPORT_SUFFIX "_import.lib")
  elseif (MINGW)
    set_target_properties(sentencepiece PROPERTIES IMPORT_SUFFIX ".dll.a")
    set_target_properties(sentencepiece_train PROPERTIES IMPORT_SUFFIX ".dll.a")
  endif()
else()
  add_library(sentencepiece ALIAS sentencepiece-static)
  add_library(sentencepiece_train ALIAS sentencepiece_train-static)
  set(SPM_INSTALLTARGETS sentencepiece-static sentencepiece_train-static)
endif()

set_target_properties(sentencepiece-static PROPERTIES OUTPUT_NAME "sentencepiece")
set_target_properties(sentencepiece_train-static PROPERTIES OUTPUT_NAME "sentencepiece_train")

if (NOT MSVC)
  if (SPM_COVERAGE)
    set(CMAKE_CXX_FLAGS "-O0 -Wall -fPIC -coverage ${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "-O3 -Wall -fPIC ${CMAKE_CXX_FLAGS}")
  endif()
  if (SPM_ENABLE_TENSORFLOW_SHARED)
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
  endif()
  if (SPM_NO_THREADLOCAL)
    add_definitions(-DSPM_NO_THREADLOCAL=1)
    add_definitions(-DGOOGLE_PROTOBUF_NO_THREADLOCAL=1)
  endif()
  set_source_files_properties(
    sentencepiece.pb.cc sentencepiece_model.pb.cc
    PROPERTIES COMPILE_FLAGS "-Wno-misleading-indentation")
  set_source_files_properties(${SPM_TEST_SRCS}
    PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
  if (SPM_ENABLE_SHARED)
    set_property(TARGET sentencepiece APPEND_STRING PROPERTY COMPILE_FLAGS " -DPIC")
    set_property(TARGET sentencepiece_train APPEND_STRING PROPERTY COMPILE_FLAGS " -DPIC")
  endif()
endif()

add_executable(spm_encode spm_encode_main.cc)
add_executable(spm_decode spm_decode_main.cc)
add_executable(spm_normalize spm_normalize_main.cc)
add_executable(spm_train spm_train_main.cc)
add_executable(spm_export_vocab spm_export_vocab_main.cc)

target_link_libraries(spm_encode sentencepiece)
target_link_libraries(spm_decode sentencepiece)
target_link_libraries(spm_normalize sentencepiece sentencepiece_train)
target_link_libraries(spm_train sentencepiece sentencepiece_train)
target_link_libraries(spm_export_vocab sentencepiece)

if (SPM_ENABLE_NFKC_COMPILE)
  add_executable(compile_charsmap compile_charsmap_main.cc)
  target_link_libraries(compile_charsmap sentencepiece sentencepiece_train)
endif()

list(APPEND SPM_INSTALLTARGETS
  spm_encode spm_decode spm_normalize spm_train spm_export_vocab)

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
  install(TARGETS ${SPM_INSTALLTARGETS}
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
else()
install(TARGETS ${SPM_INSTALLTARGETS}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

install(FILES sentencepiece_trainer.h sentencepiece_processor.h
  DESTINATION ${CMAKE_INSTALL_INCDIR})
if (NOT SPM_USE_BUILTIN_PROTOBUF)
  install(FILES ${SPM_PROTO_HDRS} DESTINATION ${CMAKE_INSTALL_INCDIR})
endif()

file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/data" data_dir)

if (SPM_BUILD_TEST OR SPM_COVERAGE)
  enable_testing()
  add_executable(spm_test test_main.cc ${SPM_TEST_SRCS})

  if (SPM_COVERAGE)
    target_link_libraries(spm_test sentencepiece sentencepiece_train "-lgcov")
  else()
    target_link_libraries(spm_test sentencepiece sentencepiece_train)
  endif()

  set(MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --show-leak-kinds=definite,possible --error-exitcode=1")
  find_program(CTEST_MEMORYCHECK_COMMAND NAMES valgrind)
  include(Dart)

  add_test(NAME sentencepiece_test
    COMMAND $<TARGET_FILE:spm_test> --test_srcdir=${data_dir})
endif()

if (SPM_COVERAGE)
  add_custom_target(coverage
    COMMAND mkdir -p coverage
    COMMAND $<TARGET_FILE:spm_test> --test_srcdir=${data_dir}
    COMMAND lcov -c -d . -o coverage.info
    COMMAND lcov --remove coverage.info "include*" "/c++" "_test*" "testharness*" "third_party*" ".pb.*" -o coverage.info
    COMMAND mkdir -p lcov_html
    COMMAND genhtml -o lcov_html coverage.info)
  add_dependencies(coverage spm_test)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set_xcode_property(spm_encode PRODUCT_BUNDLE_IDENTIFIER "SentencePiece" All)
  set_xcode_property(spm_decode PRODUCT_BUNDLE_IDENTIFIER "SentencePiece" All)
  set_xcode_property(spm_normalize PRODUCT_BUNDLE_IDENTIFIER "SentencePiece" All)
  set_xcode_property(spm_train PRODUCT_BUNDLE_IDENTIFIER "SentencePiece" All)
  set_xcode_property(spm_export_vocab PRODUCT_BUNDLE_IDENTIFIER "SentencePiece" All)
endif()
