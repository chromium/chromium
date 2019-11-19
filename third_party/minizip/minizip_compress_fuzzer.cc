// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <vector>

#include "base/scoped_generic.h"
#include "third_party/minizip/src/mz.h"
#include "third_party/minizip/src/mz_strm_mem.h"
#include "third_party/minizip/src/mz_zip.h"

const char kTestFileName[] = "test.zip";

namespace {

struct MzTraitsBase {
  static void* InvalidValue() { return nullptr; }
};

struct MzStreamMemTraits : public MzTraitsBase {
  static void Free(void* stream) { mz_stream_mem_delete(&stream); }
};
typedef base::ScopedGeneric<void*, MzStreamMemTraits> ScopedMzStreamMem;

struct MzZipTraits : public MzTraitsBase {
  static void Free(void* stream) {
    mz_zip_close(stream);
    mz_zip_delete(&stream);
  }
};
typedef base::ScopedGeneric<void*, MzZipTraits> ScopedMzZip;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  mz_zip_file file_info = {};
  file_info.flag = MZ_ZIP_FLAG_UTF8;
  if (data_provider.ConsumeIntegral<uint8_t>() < 0x08) {
    file_info.flag = data_provider.ConsumeIntegral<uint16_t>();
  }
  file_info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
  if (data_provider.ConsumeIntegral<uint8_t>() < 0x08) {
    file_info.compression_method = MZ_COMPRESS_METHOD_STORE;
  } else if (data_provider.ConsumeIntegral<uint8_t>() < 0x08) {
    file_info.compression_method = data_provider.ConsumeIntegral<uint16_t>();
  }
  if (data_provider.ConsumeIntegral<uint8_t>() < 0x08) {
    file_info.zip64 = data_provider.ConsumeIntegral<uint16_t>();
  }
  file_info.filename = kTestFileName;
  file_info.filename_size = sizeof(kTestFileName);

  int16_t compress_level = MZ_COMPRESS_LEVEL_DEFAULT;
  if (data_provider.ConsumeIntegral<uint8_t>() < 0x08) {
    compress_level =
        static_cast<int16_t>(data_provider.ConsumeIntegral<uint16_t>());
  }

  ScopedMzStreamMem out_stream(mz_stream_mem_create(nullptr));

  ScopedMzZip zip_file(mz_zip_create(nullptr));
  int result = mz_zip_open(zip_file.get(), out_stream.get(),
                           MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE);
  if (result != MZ_OK) {
    return 0;
  }

  result = mz_zip_entry_write_open(zip_file.get(), &file_info, compress_level,
                                   0, nullptr);
  if (result != MZ_OK) {
    return 0;
  }

  std::vector<uint8_t> remaining_data =
      data_provider.ConsumeRemainingBytes<uint8_t>();
  result = mz_zip_entry_write(zip_file.get(), remaining_data.data(),
                              remaining_data.size());
  if (result != MZ_OK) {
    return 0;
  }

  result = mz_zip_entry_close(zip_file.get());
  if (result != MZ_OK) {
    return 0;
  }

  result = mz_zip_close(zip_file.get());
  if (result != MZ_OK) {
    return 0;
  }

  return 0;
}
