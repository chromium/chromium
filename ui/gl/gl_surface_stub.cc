// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_stub.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace gl {

void GLSurfaceStub::Destroy() {
}

bool GLSurfaceStub::Resize(const gfx::Size& size,
                           float scale_factor,
                           ColorSpace color_space,
                           bool has_alpha) {
  return true;
}

bool GLSurfaceStub::IsOffscreen() {
  return false;
}

gfx::SwapResult GLSurfaceStub::SwapBuffers(PresentationCallback callback) {
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::TimeDelta(),
                                     0 /* flags */);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(feedback)));
  return gfx::SwapResult::SWAP_ACK;
}

gfx::Size GLSurfaceStub::GetSize() {
  return size_;
}

void* GLSurfaceStub::GetHandle() {
  return NULL;
}

bool GLSurfaceStub::BuffersFlipped() const {
  return buffers_flipped_;
}

GLSurfaceFormat GLSurfaceStub::GetFormat() {
  return GLSurfaceFormat();
}

bool GLSurfaceStub::SupportsDCLayers() const {
  return supports_draw_rectangle_;
}

gfx::Vector2d GLSurfaceStub::GetDrawOffset() const {
  return supports_draw_rectangle_ ? gfx::Vector2d(100, 200) : gfx::Vector2d();
}

GLSurfaceStub::~GLSurfaceStub() {}

}  // namespace gl
