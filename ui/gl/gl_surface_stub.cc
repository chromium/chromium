// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_stub.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace gl {

void GLSurfaceStub::Destroy() {
}

bool GLSurfaceStub::Resize(const gfx::Size& size,
                           float scale_factor,
                           const gfx::ColorSpace& color_space,
                           bool has_alpha) {
  return true;
}

bool GLSurfaceStub::IsOffscreen() {
  return true;
}

gfx::SwapResult GLSurfaceStub::SwapBuffers(PresentationCallback callback,
                                           gfx::FrameData data) {
  gfx::PresentationFeedback feedback(base::TimeTicks::Now(), base::TimeDelta(),
                                     0 /* flags */);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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

GLSurfaceStub::~GLSurfaceStub() {}

}  // namespace gl
