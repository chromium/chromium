// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/gl_surface_egl_readback.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/ozone/common/egl_util.h"

namespace ui {
namespace {

constexpr size_t kBytesPerPixelBGRA = 4;

}  // namespace

GLSurfaceEglReadback::GLSurfaceEglReadback()
    : PbufferGLSurfaceEGL(gfx::Size(1, 1)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

bool GLSurfaceEglReadback::Resize(const gfx::Size& size,
                                  float scale_factor,
                                  ColorSpace color_space,
                                  bool has_alpha) {
  pixels_.reset();

  if (!PbufferGLSurfaceEGL::Resize(size, scale_factor, color_space, has_alpha))
    return false;

  // Allocate a new buffer for readback.
  const size_t buffer_size = size.width() * size.height() * kBytesPerPixelBGRA;
  pixels_ = std::make_unique<uint8_t[]>(buffer_size);

  return true;
}

bool GLSurfaceEglReadback::IsOffscreen() {
  return false;
}

gfx::SwapResult GLSurfaceEglReadback::SwapBuffers(
    PresentationCallback callback) {
  const gfx::Size size = GetSize();
  glReadPixels(0, 0, size.width(), size.height(), GL_BGRA, GL_UNSIGNED_BYTE,
               pixels_.get());

  gfx::SwapResult swap_result = gfx::SwapResult::SWAP_FAILED;
  gfx::PresentationFeedback feedback;

  if (HandlePixels(pixels_.get())) {
    // Swap is succesful, so return SWAP_ACK and provide the current time with
    // presentation feedback.
    swap_result = gfx::SwapResult::SWAP_ACK;
    feedback.timestamp = base::TimeTicks::Now();
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), feedback));
  return swap_result;
}

bool GLSurfaceEglReadback::FlipsVertically() const {
  return true;
}

GLSurfaceEglReadback::~GLSurfaceEglReadback() {
  Destroy();
}

bool GLSurfaceEglReadback::HandlePixels(uint8_t* pixels) {
  return true;
}

}  // namespace ui
