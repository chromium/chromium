// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_BUFFER_W_STREAM_H_
#define UI_GFX_IMAGE_BUFFER_W_STREAM_H_

#include <stdint.h>
#include <vector>

#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Writes bytes to a std::vector that can be fetched. This is used to record the
// output of skia image encoding.
class GFX_EXPORT BufferWStream : public SkWStream {
 public:
  BufferWStream();
  BufferWStream(const BufferWStream&) = delete;
  BufferWStream& operator=(const BufferWStream&) = delete;
  ~BufferWStream() override;

  // Returns the output buffer by moving.
  std::vector<uint8_t> TakeBuffer();

  // SkWStream:
  bool write(const void* buffer, size_t size) override;
  size_t bytesWritten() const override;

 private:
  std::vector<uint8_t> result_;
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_BUFFER_W_STREAM_H_
