// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/file_stream.h"

#include <fcntl.h>

#include <algorithm>
#include <cstdint>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "puffin/src/include/puffin/common.h"
#include "puffin/src/logging.h"

using std::string;

namespace puffin {

UniqueStreamPtr FileStream::Open(const string& path, bool read, bool write) {
  TEST_AND_RETURN_VALUE(read || write, nullptr);

  uint32_t flags = 0;
  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(path.c_str());
  if (read && write) {
    flags |= base::File::Flags::FLAG_READ | base::File::Flags::FLAG_WRITE |
             base::File::Flags::FLAG_OPEN_ALWAYS;
  } else if (read) {
    flags |= base::File::Flags::FLAG_READ | base::File::Flags::FLAG_OPEN;
    TEST_AND_RETURN_VALUE(PathExists(file_path), nullptr);
  } else {
    flags |=
        base::File::Flags::FLAG_WRITE | base::File::Flags::FLAG_OPEN_ALWAYS;
  }
  return UniqueStreamPtr(new FileStream(file_path, flags));
}

UniqueStreamPtr FileStream::CreateStreamFromFile(base::File file) {
  return UniqueStreamPtr(new FileStream(std::move(file)));
}

bool FileStream::GetSize(uint64_t* size) {
  TEST_AND_RETURN_FALSE(file_.IsValid());
  int64_t result = file_.GetLength();
  TEST_AND_RETURN_FALSE(result >= 0);
  *size = base::as_unsigned(result);
  return true;
}

bool FileStream::GetOffset(uint64_t* offset) {
  int64_t off = file_.Seek(base::File::Whence::FROM_CURRENT, 0);
  TEST_AND_RETURN_FALSE(off >= 0);
  *offset = base::as_unsigned(off);
  return true;
}

bool FileStream::Seek(uint64_t u_offset) {
  TEST_AND_RETURN_FALSE(base::IsValueInRangeForNumericType<int64_t>(u_offset));
  int64_t offset = base::as_signed(u_offset);
  int64_t off =
      base::as_signed(file_.Seek(base::File::Whence::FROM_BEGIN, offset));
  TEST_AND_RETURN_FALSE(off == offset);
  return true;
}

bool FileStream::Read(void* buffer, size_t length) {
  base::span<uint8_t> bytes(static_cast<uint8_t*>(buffer), length);
  while (!bytes.empty()) {
    std::optional<size_t> bytes_read = file_.ReadAtCurrentPos(bytes);
    TEST_AND_RETURN_FALSE(bytes_read > 0);
    bytes.take_first(*bytes_read);
  }
  return true;
}

bool FileStream::Write(const void* buffer, size_t length) {
  base::span<const uint8_t> bytes(static_cast<const uint8_t*>(buffer), length);
  while (!bytes.empty()) {
    std::optional<size_t> bytes_written = file_.WriteAtCurrentPos(bytes);
    TEST_AND_RETURN_FALSE(bytes_written.has_value());
    bytes.take_first(*bytes_written);
  }
  return true;
}

bool FileStream::Close() {
  if (!file_.IsValid()) {
    return false;
  }
  file_.Close();
  return true;
}

}  // namespace puffin
