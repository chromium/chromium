// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/image/buffer_w_stream.h"

#include <utility>

namespace gfx {

BufferWStream::BufferWStream() = default;

BufferWStream::~BufferWStream() = default;

std::vector<uint8_t> BufferWStream::TakeBuffer() {
  return std::move(result_);
}

bool BufferWStream::write(const void* buffer, size_t size) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(buffer);
  result_.insert(result_.end(), bytes, bytes + size);
  return true;
}

size_t BufferWStream::bytesWritten() const {
  return result_.size();
}

}  // namespace gfx
