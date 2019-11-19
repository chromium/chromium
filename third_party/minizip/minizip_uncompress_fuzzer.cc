// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "third_party/minizip/src/mz.h"
#include "third_party/minizip/src/mz_strm_mem.h"
#include "third_party/minizip/src/mz_zip.h"

namespace {
const char kTestPassword[] = "test123";
const char kTestFileName[] = "foo";
const char kTestFileNameUppercase[] = "FOO";
const int kMaxFiles = 128;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  void* stream = mz_stream_mem_create(nullptr);
  mz_stream_mem_set_buffer(
      stream, reinterpret_cast<void*>(const_cast<uint8_t*>(data)), size);

  constexpr int kReadBufferSize = 1024;
  std::vector<char> read_buffer(kReadBufferSize);

  void* zip_file = mz_zip_create(nullptr);
  int result = mz_zip_open(zip_file, stream, MZ_OPEN_MODE_READ);
  if (result == MZ_OK) {
    // Some archive properties that are non-fatal for reading the archive.
    const char* archive_comment = nullptr;
    mz_zip_get_comment(zip_file, &archive_comment);
    uint16_t version_madeby = 0;
    mz_zip_get_version_madeby(zip_file, &version_madeby);
    uint64_t num_entries = 0;
    mz_zip_get_number_entry(zip_file, &num_entries);

    result = mz_zip_goto_first_entry(zip_file);
    for (int i = 0; result == MZ_OK && i < kMaxFiles; i++) {
      mz_zip_file* file_info = nullptr;
      result = mz_zip_entry_get_info(zip_file, &file_info);
      if (result != MZ_OK) {
        break;
      }

      bool is_encrypted = (file_info->flag & MZ_ZIP_FLAG_ENCRYPTED);
      result = mz_zip_entry_read_open(zip_file, 0,
                                      is_encrypted ? kTestPassword : nullptr);
      if (result != MZ_OK) {
        break;
      }

      result = mz_zip_entry_is_open(zip_file);
      if (result != MZ_OK) {
        break;
      }

      // Return value isn't checked here because we can't predict what the value
      // will be.
      mz_zip_entry_is_dir(zip_file);

      result = mz_zip_get_entry(zip_file);
      if (result < 0) {
        break;
      }

      result = mz_zip_entry_read(zip_file, read_buffer.data(), kReadBufferSize);
      if (result < 0) {
        break;
      }

      result = mz_zip_entry_close(zip_file);
      if (result != MZ_OK) {
        break;
      }

      result = mz_zip_goto_next_entry(zip_file);
    }

    mz_zip_entry_close(zip_file);

    // Return value isn't checked here because we can't predict what the value
    // will be.
    mz_zip_locate_entry(zip_file, kTestFileName, 0);
    mz_zip_locate_entry(zip_file, kTestFileNameUppercase, 0);
    mz_zip_locate_entry(zip_file, kTestFileName, 1);
    mz_zip_locate_entry(zip_file, kTestFileNameUppercase, 1);

    mz_zip_close(zip_file);
  }

  mz_zip_delete(&zip_file);
  mz_stream_mem_delete(&stream);

  return 0;
}
