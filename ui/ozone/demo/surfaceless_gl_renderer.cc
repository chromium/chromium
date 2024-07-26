// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/demo/surfaceless_gl_renderer.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/presenter.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

namespace {

OverlaySurfaceCandidate MakeOverlayCandidate(int z_order,
                                             gfx::Rect bounds_rect,
                                             gfx::RectF crop_rect) {
  // The overlay checking interface is designed to satisfy the needs of CC which
  // will be producing RectF target rectangles. But we use the bounds produced
  // in RenderFrame for GLSurface::ScheduleOverlayPlane.
  gfx::RectF display_rect(bounds_rect.x(), bounds_rect.y(), bounds_rect.width(),
                          bounds_rect.height());

  OverlaySurfaceCandidate overlay_candidate;

  // Use default display format since this should be compatible with most
  // devices.
  overlay_candidate.format = display::DisplaySnapshot::PrimaryFormat();

  // The bounds rectangle of the candidate overlay buffer.
  overlay_candidate.buffer_size = bounds_rect.size();
  // The same rectangle in floating point coordinates.
  overlay_candidate.display_rect = display_rect;

  overlay_candidate.crop_rect = crop_rect;

  // The demo overlay instance is always ontop and not clipped. Clipped quads
  // cannot be placed in overlays.

  return overlay_candidate;
}

}  // namespace

SurfacelessGlRenderer::BufferWrapper::BufferWrapper() {}

SurfacelessGlRenderer::BufferWrapper::~BufferWrapper() {
  if (gl_fb_)
    glDeleteFramebuffersEXT(1, &gl_fb_);

  if (gl_tex_) {
    glDeleteTextures(1, &gl_tex_);
  }
}

scoped_refptr<gfx::NativePixmap> SurfacelessGlRenderer::BufferWrapper::image()
    const {
  return pixmap_;
}

bool SurfacelessGlRenderer::BufferWrapper::Initialize(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size) {
  glGenFramebuffersEXT(1, &gl_fb_);
  glGenTextures(1, &gl_tex_);

  gfx::BufferFormat format = display::DisplaySnapshot::PrimaryFormat();
  pixmap_ = OzonePlatform::GetInstance()
                ->GetSurfaceFactoryOzone()
                ->CreateNativePixmap(widget, nullptr, size, format,
                                     gfx::BufferUsage::SCANOUT);

  glBindFramebufferEXT(GL_FRAMEBUFFER, gl_fb_);

  pixmap_gl_binding_ =
      OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->GetCurrentGLOzone()
          ->ImportNativePixmap(pixmap_, format, gfx::BufferPlane::DEFAULT, size,
                               gfx::ColorSpace(), GL_TEXTURE_2D, gl_tex_);

  if (!pixmap_gl_binding_) {
    LOG(ERROR) << "Failed to create NativePixmapEGLBinding";
    return false;
  }

  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            gl_tex_, 0);
  if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR) << "Failed to create framebuffer "
               << glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
    return false;
  }

  widget_ = widget;
  size_ = size;

  return true;
}

void SurfacelessGlRenderer::BufferWrapper::BindFramebuffer() {
  glBindFramebufferEXT(GL_FRAMEBUFFER, gl_fb_);
}

SurfacelessGlRenderer::SurfacelessGlRenderer(
    gfx::AcceleratedWidget widget,
    std::unique_ptr<PlatformWindowSurface> window_surface,
    const scoped_refptr<gl::GLSurface>& offscreen_surface,
    const scoped_refptr<gl::Presenter>& presenter,
    const gfx::Size& size)
    : RendererBase(widget, size),
      overlay_checker_(ui::OzonePlatform::GetInstance()
                           ->GetOverlayManager()
                           ->CreateOverlayCandidates(widget)),
      window_surface_(std::move(window_surface)),
      gl_surface_(offscreen_surface),
      presenter_(presenter) {}

SurfacelessGlRenderer::~SurfacelessGlRenderer() {
  // Need to make current when deleting the framebuffer resources allocated in
  // the buffers.
  context_->MakeCurrent(gl_surface_.get());
  for (size_t i = 0; i < std::size(buffers_); ++i)
    buffers_[i].reset();

  for (size_t i = 0; i < kMaxLayers; ++i) {
    for (size_t j = 0; j < std::size(overlay_buffers_[i]); ++j)
      overlay_buffers_[i][j].reset();
  }
}

bool SurfacelessGlRenderer::Initialize() {
  context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                       gl::GLContextAttribs());
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create GL context";
    return false;
  }

  presenter_->Resize(size_, 1.f, gfx::ColorSpace(), true);

  if (!context_->MakeCurrent(gl_surface_.get())) {
    LOG(ERROR) << "Failed to make GL context current";
    return false;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("partial-primary-plane"))
    primary_plane_rect_ = gfx::Rect(200, 200, 800, 800);
  else
    primary_plane_rect_ = gfx::Rect(size_);

  for (size_t i = 0; i < std::size(buffers_); ++i) {
    buffers_[i] = std::make_unique<BufferWrapper>();
    if (!buffers_[i]->Initialize(widget_, primary_plane_rect_.size()))
      return false;
  }

  if (command_line->HasSwitch("enable-overlay")) {
    int requested_overlay_cnt;
    base::StringToInt(
        command_line->GetSwitchValueASCII("enable-overlay").c_str(),
        &requested_overlay_cnt);
    overlay_cnt_ = std::clamp(requested_overlay_cnt, 1, kMaxLayers);

    const gfx::Size overlay_size =
        gfx::Size(size_.width() / 8, size_.height() / 8);
    for (size_t i = 0; i < overlay_cnt_; ++i) {
      for (size_t j = 0; j < std::size(overlay_buffers_[i]); ++j) {
        overlay_buffers_[i][j] = std::make_unique<BufferWrapper>();
        overlay_buffers_[i][j]->Initialize(gfx::kNullAcceleratedWidget,
                                           overlay_size);

        glViewport(0, 0, overlay_size.width(), overlay_size.height());
        glClearColor(j, 1.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Ensure that the rendering has been committed to the buffer and thus
        // that the buffer is ready for display without additional
        // synchronization. This allows us to avoid using fences for display
        // synchronization of the non-overlay buffers in RenderFrame.
        glFinish();
      }
    }
  }

  disable_primary_plane_ = command_line->HasSwitch("disable-primary-plane");

  use_gpu_fences_ = presenter_->SupportsPlaneGpuFences();

  // Schedule the initial render.
  PostRenderFrameTask(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
  return true;
}

void SurfacelessGlRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "SurfacelessGlRenderer::RenderFrame");

  float fraction = NextFraction();

  gfx::Rect overlay_rect[kMaxLayers];
  const gfx::RectF unity_rect = gfx::RectF(0, 0, 1, 1);

  OverlayCandidatesOzone::OverlaySurfaceCandidateList overlay_list;
  if (!disable_primary_plane_) {
    overlay_list.push_back(
        MakeOverlayCandidate(1, gfx::Rect(size_), unity_rect));
    // We know at least the primary plane can be scanned out.
    overlay_list.back().overlay_handled = true;
  }

  for (size_t i = 0; i < overlay_cnt_; ++i) {
    overlay_rect[i] = gfx::Rect(overlay_buffers_[i][0]->size());

    float steps_num = 5.0f;
    float stepped_fraction =
        std::floor((fraction + 0.5f / steps_num) * steps_num) / steps_num;
    gfx::Vector2d offset(
        stepped_fraction * (size_.width() - overlay_rect[i].width()),
        ((size_.height() / (overlay_cnt_ + 1)) * (i + 1) -
         overlay_rect[i].height() / 2));
    overlay_rect[i] += offset;
    overlay_list.push_back(
        MakeOverlayCandidate(1, overlay_rect[i], unity_rect));
  }

  // The actual validation for a specific overlay configuration is done
  // asynchronously and then cached inside overlay_checker_ once a reply
  // is sent back.
  // This means that the first few frames we call this method for a specific
  // overlay_list, all the overlays but the primary plane, that we explicitly
  // marked as handled, will be rejected even if they might be handled at a
  // later time.
  overlay_checker_->CheckOverlaySupport(&overlay_list);

  context_->MakeCurrent(gl_surface_.get());
  buffers_[back_buffer_]->BindFramebuffer();

  glViewport(0, 0, size_.width(), size_.height());
  glClearColor(1 - fraction, 0.0, fraction, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (!disable_primary_plane_) {
    CHECK(overlay_list.front().overlay_handled);

    // Optionally use a fence to synchronize overlay plane display, if
    // requested when invoking ozone_demo. Note that currently only the primary
    // plane needs to use a fence, since its buffers are dynamically updated
    // every frame. The buffers for non-primary planes are only drawn to during
    // initialization and guaranteed to be ready for display (see Initialize),
    // so no additional fence synchronization is needed for them.
    std::unique_ptr<gl::GLFence> gl_fence =
        use_gpu_fences_ ? gl::GLFence::CreateForGpuFence() : nullptr;

    presenter_->ScheduleOverlayPlane(
        buffers_[back_buffer_]->image(),
        gl_fence ? gl_fence->GetGpuFence() : nullptr,
        gfx::OverlayPlaneData(
            0, gfx::OVERLAY_TRANSFORM_NONE, gfx::RectF(primary_plane_rect_),
            unity_rect, false, gfx::Rect(buffers_[back_buffer_]->size()), 1.0f,
            gfx::OverlayPriorityHint::kNone, gfx::RRectF(),
            gfx::ColorSpace::CreateSRGB(), std::nullopt));
  }

  for (size_t i = 0; i < overlay_cnt_; ++i) {
    if (overlay_list.back().overlay_handled) {
      presenter_->ScheduleOverlayPlane(
          overlay_buffers_[i][back_buffer_]->image(), /* gpu_fence */ nullptr,
          gfx::OverlayPlaneData(
              1, gfx::OVERLAY_TRANSFORM_NONE, gfx::RectF(overlay_rect[i]),
              unity_rect, false,
              gfx::Rect(overlay_buffers_[i][back_buffer_]->size()), 1.0f,
              gfx::OverlayPriorityHint::kNone, gfx::RRectF(),
              gfx::ColorSpace::CreateSRGB(), std::nullopt));
    }
  }

  back_buffer_ ^= 1;
  presenter_->Present(
      base::BindOnce(&SurfacelessGlRenderer::PostRenderFrameTask,
                     weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing(), gfx::FrameData());
}

void SurfacelessGlRenderer::PostRenderFrameTask(
    gfx::SwapCompletionResult result) {
  if (!result.release_fence.is_null())
    gfx::GpuFence(std::move(result.release_fence)).Wait();

  switch (result.swap_result) {
    case gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS:
      for (size_t i = 0; i < std::size(buffers_); ++i) {
        buffers_[i] = std::make_unique<BufferWrapper>();
        if (!buffers_[i]->Initialize(widget_, primary_plane_rect_.size()))
          LOG(FATAL) << "Failed to recreate buffer";
      }
      [[fallthrough]];  // We want to render a new frame anyways.
    case gfx::SwapResult::SWAP_ACK:
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&SurfacelessGlRenderer::RenderFrame,
                                    weak_ptr_factory_.GetWeakPtr()));
      break;
    case gfx::SwapResult::SWAP_SKIPPED:
    case gfx::SwapResult::SWAP_FAILED:
    case gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED:
      LOG(FATAL) << "Failed to swap buffers";
  }
}

void SurfacelessGlRenderer::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  LOG_IF(ERROR, feedback.timestamp.is_null()) << "Last frame is discarded!";
}

}  // namespace ui
