// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/file_stream.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <utility>

#include "puffin/src/include/puffin/common.h"
#include "puffin/src/logging.h"

using std::string;

namespace puffin {

UniqueStreamPtr FileStream::Open(const string& path, bool read, bool write) {
  TEST_AND_RETURN_VALUE(read || write, nullptr);
  int flags = O_CLOEXEC;
  if (read && write) {
    flags |= O_RDWR | O_CREAT;
  } else if (read) {
    flags |= O_RDONLY;
  } else {
    flags |= O_WRONLY | O_CREAT;
  }

  mode_t mode = 0644;  // -rw-r--r--
  int fd = open(path.c_str(), flags, mode);
  TEST_AND_RETURN_VALUE(fd >= 0, nullptr);
  return UniqueStreamPtr(new FileStream(fd));
}

bool FileStream::GetSize(uint64_t* size) const {
  auto cur_off = lseek(fd_, 0, SEEK_CUR);
  TEST_AND_RETURN_FALSE(cur_off >= 0);
  auto fsize = lseek(fd_, 0, SEEK_END);
  TEST_AND_RETURN_FALSE(fsize >= 0);
  cur_off = lseek(fd_, cur_off, SEEK_SET);
  TEST_AND_RETURN_FALSE(cur_off >= 0);
  *size = fsize;
  return true;
}

bool FileStream::GetOffset(uint64_t* offset) const {
  auto off = lseek(fd_, 0, SEEK_CUR);
  TEST_AND_RETURN_FALSE(off >= 0);
  *offset = off;
  return true;
}

bool FileStream::Seek(uint64_t offset) {
  auto off = lseek(fd_, offset, SEEK_SET);
  TEST_AND_RETURN_FALSE(off == static_cast<off_t>(offset));
  return true;
}

bool FileStream::Read(void* buffer, size_t length) {
  auto c_bytes = static_cast<uint8_t*>(buffer);
  size_t total_bytes_read = 0;
  while (total_bytes_read < length) {
    auto bytes_read =
        read(fd_, c_bytes + total_bytes_read, length - total_bytes_read);
    // if bytes_read is zero then EOF is reached and we should not be here.
    TEST_AND_RETURN_FALSE(bytes_read > 0);
    total_bytes_read += bytes_read;
  }
  return true;
}

bool FileStream::Write(const void* buffer, size_t length) {
  auto c_bytes = static_cast<const uint8_t*>(buffer);
  size_t total_bytes_wrote = 0;
  while (total_bytes_wrote < length) {
    auto bytes_wrote =
        write(fd_, c_bytes + total_bytes_wrote, length - total_bytes_wrote);
    TEST_AND_RETURN_FALSE(bytes_wrote >= 0);
    total_bytes_wrote += bytes_wrote;
  }
  return true;
}

bool FileStream::Close() {
  return close(fd_) == 0;
}

}  // namespace puffin
