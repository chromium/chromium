// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image.h"

namespace gl {

bool GLImage::BindTexImageWithInternalformat(unsigned target,
                                             unsigned internalformat) {
  return false;
}

bool GLImage::EmulatingRGB() const {
  return false;
}

GLImage::Type GLImage::GetType() const {
  return Type::NONE;
}

#if defined(OS_ANDROID)
std::unique_ptr<GLImage::ScopedHardwareBuffer> GLImage::GetAHardwareBuffer() {
  return nullptr;
}

GLImage::ScopedHardwareBuffer::ScopedHardwareBuffer(
    base::android::ScopedHardwareBufferHandle handle,
    base::ScopedFD fence_fd)
    : handle_(std::move(handle)), fence_fd_(std::move(fence_fd)) {}

GLImage::ScopedHardwareBuffer::~ScopedHardwareBuffer() = default;

base::ScopedFD GLImage::ScopedHardwareBuffer::TakeFence() {
  return std::move(fence_fd_);
}
#endif

}  // namespace gl
