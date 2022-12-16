// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/presenter.h"

namespace gl {

Presenter::Presenter(GLDisplayEGL* display, const gfx::Size& size)
    : SurfacelessEGL(display, size) {}
Presenter::~Presenter() = default;

bool Presenter::SupportsAsyncSwap() {
  return true;
}

bool Presenter::SupportsPostSubBuffer() {
  return true;
}

bool Presenter::SupportsCommitOverlayPlanes() {
  return false;
}

bool Presenter::IsOffscreen() {
  return false;
}

gfx::SurfaceOrigin Presenter::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

void Presenter::SwapBuffersAsync(SwapCompletionCallback completion_callback,
                                 PresentationCallback presentation_callback,
                                 gfx::FrameData data) {
  Present(std::move(completion_callback), std::move(presentation_callback),
          data);
}

void Presenter::PostSubBufferAsync(int x,
                                   int y,
                                   int width,
                                   int height,
                                   SwapCompletionCallback completion_callback,
                                   PresentationCallback presentation_callback,
                                   gfx::FrameData data) {
  Present(std::move(completion_callback), std::move(presentation_callback),
          data);
}

gfx::SwapResult Presenter::SwapBuffers(PresentationCallback callback,
                                       gfx::FrameData data) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::SwapResult Presenter::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::SwapResult Presenter::CommitOverlayPlanes(PresentationCallback callback,
                                               gfx::FrameData data) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void Presenter::CommitOverlayPlanesAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  Present(std::move(completion_callback), std::move(presentation_callback),
          data);
}

}  // namespace gl