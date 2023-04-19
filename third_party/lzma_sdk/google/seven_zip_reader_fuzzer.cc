// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "third_party/lzma_sdk/google/seven_zip_reader.h"

extern "C" {
#include "third_party/lzma_sdk/C/7zCrc.h"
}

namespace {

base::NoDestructor<base::File> archive_file;
base::NoDestructor<base::File> temp_file;

class Delegate : public seven_zip::Delegate {
 public:
  Delegate() : buffer_(4096) {}

  // seven_zip::Delegate implementation
  void OnOpenError(seven_zip::Result result) override {}
  base::File OnTempFileRequest() override { return temp_file->Duplicate(); }
  bool OnEntry(const seven_zip::EntryInfo& entry,
               base::span<uint8_t>& output) override {
    if (entry.file_size < 4096) {
      buffer_.resize(entry.file_size);
      output = base::make_span(buffer_);
      return true;
    } else {
      return false;
    }
  }
  bool OnDirectory(const seven_zip::EntryInfo& entry) override { return true; }
  bool EntryDone(seven_zip::Result result,
                 const seven_zip::EntryInfo& entry) override {
    return true;
  }

 private:
  std::vector<uint8_t> buffer_;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  [[maybe_unused]] static bool initialized = []() {
    base::FilePath path;
    if (base::CreateTemporaryFile(&path)) {
      archive_file->Initialize(
          path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                 base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                 base::File::FLAG_DELETE_ON_CLOSE |
                 base::File::FLAG_WIN_SHARE_DELETE));
    }

    base::FilePath temp_path;
    if (base::CreateTemporaryFile(&temp_path)) {
      temp_file->Initialize(
          temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                      base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                      base::File::FLAG_DELETE_ON_CLOSE |
                      base::File::FLAG_WIN_SHARE_DELETE));
    }

    // Create a fake CRC table that always returns 0 for the target value. This
    // significantly increases the fuzzer's ability to safely mutate the
    // headers. The values here were chosen to keep the CRC internal state as
    // 0xffffffff in CrcUpdateT8. Other processors may choose a different CRC
    // update function, and would need different values here. See the
    // CrcGenerateTable function in //third_party/lzma_sdk/C/7zCrc.c.
    seven_zip::EnsureLzmaSdkInitialized();
    for (size_t i = 0; i < 256; i++) {
      g_CrcTable[i] = 0xff000000;
      g_CrcTable[i + 0x100] = 0xff000000;
      g_CrcTable[i + 0x200] = 0;
      g_CrcTable[i + 0x300] = 0;
    }

    for (size_t i = 0x0; i < 256; i++) {
      g_CrcTable[i + 0x400] = 0xffffffff;
      g_CrcTable[i + 0x500] = 0;
      g_CrcTable[i + 0x600] = 0;
      g_CrcTable[i + 0x700] = 0;
    }

    return true;
  }();

  if (!archive_file->IsValid() || !temp_file->IsValid())
    return 0;

  archive_file->SetLength(size);
  archive_file->Write(0, reinterpret_cast<const char*>(data), size);

  Delegate delegate;

  std::unique_ptr<seven_zip::SevenZipReader> reader =
      seven_zip::SevenZipReader::Create(archive_file->Duplicate(), delegate);
  if (reader) {
    reader->Extract();
  }

  return 0;
}
