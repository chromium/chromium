// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/extent_stream.h"

#include <algorithm>
#include <utility>

#include "puffin/src/logging.h"

using std::vector;

namespace puffin {

UniqueStreamPtr ExtentStream::CreateForWrite(
    UniqueStreamPtr stream, const vector<ByteExtent>& extents) {
  return UniqueStreamPtr(new ExtentStream(std::move(stream), extents, true));
}

UniqueStreamPtr ExtentStream::CreateForRead(UniqueStreamPtr stream,
                                            const vector<ByteExtent>& extents) {
  return UniqueStreamPtr(new ExtentStream(std::move(stream), extents, false));
}

ExtentStream::ExtentStream(UniqueStreamPtr stream,
                           const vector<ByteExtent>& extents,
                           bool is_for_write)
    : stream_(std::move(stream)),
      extents_(extents),
      cur_extent_offset_(0),
      is_for_write_(is_for_write),
      offset_(0) {
  extents_upper_bounds_.reserve(extents_.size() + 1);
  extents_upper_bounds_.emplace_back(0);
  uint64_t total_size = 0;
  uint64_t extent_end = 0;
  for (const auto& extent : extents_) {
    total_size += extent.length;
    extents_upper_bounds_.emplace_back(total_size);
    extent_end = extent.offset + extent.length;
  }
  size_ = total_size;

  // Adding one extent at the end to avoid doing extra checks in:
  // - Seek: when seeking to the end of extents
  // - DoReadOrWrite: when changing the current extent.
  extents_.emplace_back(extent_end, 0);
  cur_extent_ = extents_.begin();
}

bool ExtentStream::GetSize(uint64_t* size) const {
  *size = size_;
  return true;
}

bool ExtentStream::GetOffset(uint64_t* offset) const {
  *offset = offset_;
  return true;
}

bool ExtentStream::Seek(uint64_t offset) {
  TEST_AND_RETURN_FALSE(offset <= size_);

  // The first item is zero and upper_bound never returns it because it always
  // return the item which is greater than the given value.
  auto extent_idx = std::upper_bound(extents_upper_bounds_.begin(),
                                     extents_upper_bounds_.end(), offset) -
                    extents_upper_bounds_.begin() - 1;
  cur_extent_ = std::next(extents_.begin(), extent_idx);
  offset_ = offset;
  cur_extent_offset_ = offset_ - extents_upper_bounds_[extent_idx];
  TEST_AND_RETURN_FALSE(
      stream_->Seek(cur_extent_->offset + cur_extent_offset_));
  return true;
}

bool ExtentStream::Close() {
  return stream_->Close();
}

bool ExtentStream::Read(void* buffer, size_t length) {
  TEST_AND_RETURN_FALSE(!is_for_write_);
  TEST_AND_RETURN_FALSE(DoReadOrWrite(buffer, nullptr, length));
  return true;
}

bool ExtentStream::Write(const void* buffer, size_t length) {
  TEST_AND_RETURN_FALSE(is_for_write_);
  TEST_AND_RETURN_FALSE(DoReadOrWrite(nullptr, buffer, length));
  return true;
}

bool ExtentStream::DoReadOrWrite(void* read_buffer,
                                 const void* write_buffer,
                                 size_t length) {
  uint64_t bytes_passed = 0;
  while (bytes_passed < length) {
    if (cur_extent_ == extents_.end()) {
      return false;
    }
    uint64_t bytes_to_pass = std::min(length - bytes_passed,
                                      cur_extent_->length - cur_extent_offset_);
    if (read_buffer != nullptr) {
      TEST_AND_RETURN_FALSE(
          stream_->Read(reinterpret_cast<uint8_t*>(read_buffer) + bytes_passed,
                        bytes_to_pass));
    } else if (write_buffer != nullptr) {
      TEST_AND_RETURN_FALSE(stream_->Write(
          reinterpret_cast<const uint8_t*>(write_buffer) + bytes_passed,
          bytes_to_pass));
    } else {
      LOG(ERROR) << "Either read or write buffer should be given!";
      return false;
    }

    bytes_passed += bytes_to_pass;
    cur_extent_offset_ += bytes_to_pass;
    offset_ += bytes_to_pass;
    if (cur_extent_offset_ == cur_extent_->length) {
      // We have to advance the cur_extent_;
      cur_extent_++;
      cur_extent_offset_ = 0;
      if (cur_extent_ != extents_.end()) {
        TEST_AND_RETURN_FALSE(stream_->Seek(cur_extent_->offset));
      }
    }
  }
  return true;
}

}  // namespace puffin
