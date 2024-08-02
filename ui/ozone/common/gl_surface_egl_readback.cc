// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/common/gl_surface_egl_readback.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/common/egl_util.h"

namespace ui {
namespace {

constexpr size_t kBytesPerPixelBGRA = 4;

}  // namespace

GLSurfaceEglReadback::GLSurfaceEglReadback(gl::GLDisplayEGL* display)
    : PbufferGLSurfaceEGL(display, gfx::Size(1, 1)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

bool GLSurfaceEglReadback::Resize(const gfx::Size& size,
                                  float scale_factor,
                                  const gfx::ColorSpace& color_space,
                                  bool has_alpha) {
  pixels_ = base::HeapArray<uint8_t>();

  if (!PbufferGLSurfaceEGL::Resize(size, scale_factor, color_space, has_alpha))
    return false;

  // For NullDrawGLBindings, all draws will be skipped,
  // so reading back pixels will causes use-of-uninitialized-value problem
  // on MemorySanitizer enabled trybots, so don't allocate the pixel_ and skip
  // pixels reading back.
  // See crbug.com/1259170 for detail.
  if (gl::HasInitializedNullDrawGLBindings())
    return true;

  // Allocate a new buffer for readback.
  const size_t buffer_size = size.width() * size.height() * kBytesPerPixelBGRA;
  pixels_ = base::HeapArray<uint8_t>::Uninit(buffer_size);

  return true;
}

gfx::SwapResult GLSurfaceEglReadback::SwapBuffers(PresentationCallback callback,
                                                  gfx::FrameData data) {
  gfx::SwapResult swap_result = gfx::SwapResult::SWAP_FAILED;
  gfx::PresentationFeedback feedback;

  if (!pixels_.empty()) {
    ReadPixels(pixels_.as_span());
    if (HandlePixels(pixels_.as_span().data())) {
      // Swap is successful, so return SWAP_ACK and provide the current time
      // with presentation feedback.
      swap_result = gfx::SwapResult::SWAP_ACK;
      feedback.timestamp = base::TimeTicks::Now();
    }
  } else {
    swap_result = gfx::SwapResult::SWAP_ACK;
    feedback.timestamp = base::TimeTicks::Now();
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), feedback));
  return swap_result;
}

gfx::SurfaceOrigin GLSurfaceEglReadback::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

GLSurfaceEglReadback::~GLSurfaceEglReadback() {
  Destroy();
}

bool GLSurfaceEglReadback::HandlePixels(uint8_t* pixels) {
  return true;
}

void GLSurfaceEglReadback::ReadPixels(base::span<uint8_t> buffer) {
  const gfx::Size size = GetSize();
  GLint read_fbo = 0;
  GLint pixel_pack_buffer = 0;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pixel_pack_buffer);

  // Make sure pixels are read from fbo 0.
  if (read_fbo)
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);

  // Make sure pixels are stored into |pixels_| instead of buffer binding to
  // GL_PIXEL_PACK_BUFFER.
  if (pixel_pack_buffer)
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

  CHECK_GE(buffer.size() / base::checked_cast<size_t>(size.width()),
           base::checked_cast<size_t>(size.height()));
  glReadPixels(0, 0, size.width(), size.height(), GL_BGRA, GL_UNSIGNED_BYTE,
               buffer.data());

  if (read_fbo)
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, read_fbo);
  if (pixel_pack_buffer)
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pixel_pack_buffer);
}

}  // namespace ui
