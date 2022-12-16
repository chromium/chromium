// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_PRESENTER_H_
#define UI_GL_PRESENTER_H_

#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

// Class that is used for presentation on the surfaceless platforms. Temporarily
// is in ui/gl and subclasses SurfacelessEGL. Base class will be removed and
// class will be moved to ui/gfx
class GL_EXPORT Presenter : public SurfacelessEGL {
 public:
  Presenter(GLDisplayEGL* display, const gfx::Size& size);

  bool SupportsAsyncSwap() final;
  bool SupportsPostSubBuffer() final;
  bool SupportsCommitOverlayPlanes() override;
  bool IsOffscreen() final;

  gfx::SurfaceOrigin GetOrigin() const final;

  void SwapBuffersAsync(SwapCompletionCallback completion_callback,
                        PresentationCallback presentation_callback,
                        gfx::FrameData data) final;
  void PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          SwapCompletionCallback completion_callback,
                          PresentationCallback presentation_callback,
                          gfx::FrameData data) final;
  void CommitOverlayPlanesAsync(SwapCompletionCallback completion_callback,
                                PresentationCallback presentation_callback,
                                gfx::FrameData data) final;
  gfx::SwapResult SwapBuffers(PresentationCallback callback,
                              gfx::FrameData data) final;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback presentation_callback,
                                gfx::FrameData data) final;
  gfx::SwapResult CommitOverlayPlanes(PresentationCallback callback,
                                      gfx::FrameData data) final;

  // Presents current frame asynchronously. `completion_callback` will be called
  // once all necessary steps were taken to display the frame.
  // `presentation_callback` will be called once frame was displayed and
  // presentation feedback was collected.
  virtual void Present(SwapCompletionCallback completion_callback,
                       PresentationCallback presentation_callback,
                       gfx::FrameData data) = 0;

 protected:
  ~Presenter() override;
};

}  // namespace gl

#endif  // UI_GL_PRESENTER_H_
