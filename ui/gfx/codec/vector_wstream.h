// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CODEC_VECTOR_WSTREAM_H_
#define UI_GFX_CODEC_VECTOR_WSTREAM_H_

#include <stddef.h>

#include <vector>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkStream.h"

namespace gfx {

class VectorWStream : public SkWStream {
 public:
  // We do not take ownership of dst
  VectorWStream(std::vector<unsigned char>* dst) : dst_(dst) {
    DCHECK(dst_);
    DCHECK_EQ(0UL, dst_->size());
  }

  bool write(const void* buffer, size_t size) override;

  size_t bytesWritten() const override;

 private:
  // Does not have ownership.
  raw_ptr<std::vector<unsigned char>> dst_;
};

}  // namespace gfx

#endif  // UI_GFX_CODEC_VECTOR_WSTREAM_H_
