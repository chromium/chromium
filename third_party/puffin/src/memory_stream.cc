// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/memory_stream.h"

#include <fcntl.h>
#include <algorithm>
#include <cstring>
#include <utility>

#include "base/files/file.h"
#include "base/numerics/safe_conversions.h"

#include "puffin/src/include/puffin/common.h"
#include "puffin/src/logging.h"

namespace puffin {

UniqueStreamPtr MemoryStream::CreateForRead(const Buffer& memory) {
  return UniqueStreamPtr(new MemoryStream(&memory, nullptr));
}

UniqueStreamPtr MemoryStream::CreateForWrite(Buffer* memory) {
  return UniqueStreamPtr(new MemoryStream(nullptr, memory));
}

MemoryStream::MemoryStream(const Buffer* read_memory, Buffer* write_memory)
    : read_memory_(read_memory), write_memory_(write_memory) {}

bool MemoryStream::GetSize(uint64_t* size) {
  *size =
      read_memory_ != nullptr ? read_memory_->size() : write_memory_->size();
  return true;
}

bool MemoryStream::GetOffset(uint64_t* offset) {
  *offset = offset_;
  return true;
}

bool MemoryStream::Seek(uint64_t offset) {
  TEST_AND_RETURN_FALSE(open_);
  uint64_t size;
  GetSize(&size);
  TEST_AND_RETURN_FALSE(offset <= size);
  offset_ = offset;
  return true;
}

bool MemoryStream::Read(void* buffer, size_t length) {
  TEST_AND_RETURN_FALSE(open_);
  TEST_AND_RETURN_FALSE(read_memory_ != nullptr);
  TEST_AND_RETURN_FALSE(base::IsValueInRangeForNumericType<int64_t>(length));
  TEST_AND_RETURN_FALSE(offset_ + length <= read_memory_->size());
  memcpy(buffer, read_memory_->data() + offset_, length);
  offset_ += length;
  return true;
}

bool MemoryStream::Write(const void* buffer, size_t length) {
  // TODO(ahassani): Add a maximum size limit to prevent malicious attacks.
  TEST_AND_RETURN_FALSE(open_);
  TEST_AND_RETURN_FALSE(write_memory_ != nullptr);
  TEST_AND_RETURN_FALSE(base::IsValueInRangeForNumericType<int64_t>(length));
  if (offset_ + length > write_memory_->size()) {
    write_memory_->resize(offset_ + length);
  }
  memcpy(write_memory_->data() + offset_, buffer, length);
  offset_ += length;
  return true;
}

bool MemoryStream::Close() {
  open_ = false;
  return true;
}

}  // namespace puffin
