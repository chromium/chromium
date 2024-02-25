// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_GL_SURFACE_EGL_READBACK_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_GL_SURFACE_EGL_READBACK_WAYLAND_H_

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "ui/ozone/common/gl_surface_egl_readback.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"

namespace ui {

class WaylandBufferManagerGpu;

// This is a GLSurface implementation that uses glReadPixels to populate a
// shared memory region with the contents of the surface, and then passes the
// shared memory region to Wayland for presentation.
//
// Basic control flow:
//   1. Resize() creates kMaxBuffers shared memory regions. These are added to
//   available_buffers_ and registered with Wayland via CreateShmBasedBuffer().
//   2. SwapBuffersAsync() calls glReadPixels() to read the contents of the
//   active GL context into the next available shared memory region. The shared
//   memory region is immediately sent to Wayland via CommitBuffer().
//   3. The buffer is not available for reuse until OnSubmission() is called.
//
// Note: This class relies on the assumption that kMaxBuffers is necessary and
// sufficient. The behavior is undefined if SwapBuffersAsync() is called and no
// buffers are available.
class GLSurfaceEglReadbackWayland : public GLSurfaceEglReadback,
                                    public WaylandSurfaceGpu {
 public:
  GLSurfaceEglReadbackWayland(gl::GLDisplayEGL* display,
                              gfx::AcceleratedWidget widget,
                              WaylandBufferManagerGpu* buffer_manager);
  GLSurfaceEglReadbackWayland(const GLSurfaceEglReadbackWayland&) = delete;
  GLSurfaceEglReadbackWayland& operator=(const GLSurfaceEglReadbackWayland&) =
      delete;

  // gl::GLSurface:
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback,
                              gfx::FrameData data) override;
  bool SupportsAsyncSwap() override;
  void SwapBuffersAsync(SwapCompletionCallback completion_callback,
                        PresentationCallback presentation_callback,
                        gfx::FrameData data) override;
  gfx::SurfaceOrigin GetOrigin() const override;

 private:
  struct PixelBuffer {
    PixelBuffer(base::WritableSharedMemoryMapping shm_mapping,
                uint32_t buffer_id);
    ~PixelBuffer();
    PixelBuffer(const PixelBuffer&) = delete;
    PixelBuffer& operator=(const PixelBuffer&) = delete;

    // Shared memory mapping that readback pixels are written to so that Wayland
    // is able to turn them in light.
    base::WritableSharedMemoryMapping shm_mapping_;

    // The buffer id that corresponds to the |wl_buffer| created on the browser
    // process side.
    uint32_t buffer_id_ = 0;
  };

  ~GLSurfaceEglReadbackWayland() override;

  // WaylandSurfaceGpu:
  void OnSubmission(uint32_t frame_id,
                    const gfx::SwapResult& swap_result,
                    gfx::GpuFenceHandle release_fence) override;
  void OnPresentation(uint32_t frame_id,
                      const gfx::PresentationFeedback& feedback) override;

  void DestroyBuffers();

  // Widget of the window that this readback writes pixels to.
  const gfx::AcceleratedWidget widget_;

  const raw_ptr<WaylandBufferManagerGpu> buffer_manager_;

  // Size of the buffer.
  gfx::Size size_;
  float surface_scale_factor_ = 1.f;

  // Available pixel buffers based on shared memory.
  std::vector<std::unique_ptr<PixelBuffer>> available_buffers_;

  // Displayed buffer that will become available after another buffer is
  // submitted.
  std::unique_ptr<PixelBuffer> displayed_buffer_;

  // Submitted buffers waiting to be displayed.
  base::circular_deque<std::unique_ptr<PixelBuffer>> in_flight_pixel_buffers_;

  std::vector<SwapCompletionCallback> completion_callbacks_;
  std::vector<PresentationCallback> presentation_callbacks_;

  size_t pending_frames_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_GL_SURFACE_EGL_READBACK_WAYLAND_H_
