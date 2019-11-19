// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/skia/skia_surfaceless_gl_renderer.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDeferredDisplayListRecorder.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

namespace {

const char kPartialPrimaryPlane[] = "partial-primary-plane";
const char kEnableOverlay[] = "enable-overlay";
const char kDisablePrimaryPlane[] = "disable-primary-plane";

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
  overlay_candidate.is_clipped = false;

  return overlay_candidate;
}

}  // namespace

class SurfacelessSkiaGlRenderer::BufferWrapper {
 public:
  BufferWrapper();
  ~BufferWrapper();

  gl::GLImage* image() const { return image_.get(); }
  SkSurface* sk_surface() const { return sk_surface_.get(); }

  bool Initialize(GrContext* gr_context,
                  gfx::AcceleratedWidget widget,
                  const gfx::Size& size);
  void BindFramebuffer();

  const gfx::Size size() const { return size_; }

 private:
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  gfx::Size size_;

  scoped_refptr<gl::GLImage> image_;
  unsigned int gl_tex_ = 0;
  sk_sp<SkSurface> sk_surface_;
};

SurfacelessSkiaGlRenderer::BufferWrapper::BufferWrapper() = default;

SurfacelessSkiaGlRenderer::BufferWrapper::~BufferWrapper() {
  if (gl_tex_) {
    image_->ReleaseTexImage(GL_TEXTURE_2D);
    glDeleteTextures(1, &gl_tex_);
  }
}

bool SurfacelessSkiaGlRenderer::BufferWrapper::Initialize(
    GrContext* gr_context,
    gfx::AcceleratedWidget widget,
    const gfx::Size& size) {
  glGenTextures(1, &gl_tex_);

  gfx::BufferFormat format = display::DisplaySnapshot::PrimaryFormat();
  scoped_refptr<gfx::NativePixmap> pixmap =
      OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(widget, nullptr, size, format,
                               gfx::BufferUsage::SCANOUT);
  auto image = base::MakeRefCounted<gl::GLImageNativePixmap>(size, format);
  if (!image->Initialize(std::move(pixmap))) {
    LOG(ERROR) << "Failed to create GLImage";
    return false;
  }
  image_ = image;

  glBindTexture(GL_TEXTURE_2D, gl_tex_);
  image_->BindTexImage(GL_TEXTURE_2D);

  widget_ = widget;
  size_ = size;

  GrGLTextureInfo texture_info;
  texture_info.fTarget = GL_TEXTURE_2D;
  texture_info.fID = gl_tex_;
  texture_info.fFormat = GL_BGRA8_EXT;
  GrBackendTexture backend_texture(size_.width(), size_.height(),
                                   GrMipMapped::kNo, texture_info);
  sk_surface_ = SkSurface::MakeFromBackendTexture(
      gr_context, backend_texture, kTopLeft_GrSurfaceOrigin, 0,
      kBGRA_8888_SkColorType, nullptr, nullptr);
  if (!sk_surface_) {
    LOG(ERROR) << "Failed to create skia surface";
    return false;
  }

  return true;
}

SurfacelessSkiaGlRenderer::SurfacelessSkiaGlRenderer(
    gfx::AcceleratedWidget widget,
    std::unique_ptr<PlatformWindowSurface> window_surface,
    const scoped_refptr<gl::GLSurface>& gl_surface,
    const gfx::Size& size)
    : SkiaGlRenderer(widget,
                     std::move(window_surface),
                     std::move(gl_surface),
                     size),
      overlay_checker_(ui::OzonePlatform::GetInstance()
                           ->GetOverlayManager()
                           ->CreateOverlayCandidates(widget)) {}

SurfacelessSkiaGlRenderer::~SurfacelessSkiaGlRenderer() {
  // Need to make current when deleting the framebuffer resources allocated in
  // the buffers.
  gl_context_->MakeCurrent(gl_surface_.get());
}

bool SurfacelessSkiaGlRenderer::Initialize() {
  if (!SkiaGlRenderer::Initialize())
    return false;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kPartialPrimaryPlane))
    primary_plane_rect_ = gfx::Rect(200, 200, 800, 800);
  else
    primary_plane_rect_ = gfx::Rect(size_);

  for (size_t i = 0; i < base::size(buffers_); ++i) {
    buffers_[i] = std::make_unique<BufferWrapper>();
    if (!buffers_[i]->Initialize(gr_context_.get(), widget_,
                                 primary_plane_rect_.size()))
      return false;
  }

  if (command_line->HasSwitch(kEnableOverlay)) {
    gfx::Size overlay_size = gfx::Size(size_.width() / 8, size_.height() / 8);
    for (size_t i = 0; i < base::size(overlay_buffer_); ++i) {
      overlay_buffer_[i] = std::make_unique<BufferWrapper>();
      overlay_buffer_[i]->Initialize(gr_context_.get(),
                                     gfx::kNullAcceleratedWidget, overlay_size);
      SkCanvas* sk_canvas = overlay_buffer_[i]->sk_surface()->getCanvas();
      sk_canvas->clear(SkColorSetARGB(96, 255 * i, 255, 0));
    }
  }

  disable_primary_plane_ = command_line->HasSwitch(kDisablePrimaryPlane);
  return true;
}

void SurfacelessSkiaGlRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "SurfacelessSkiaGlRenderer::RenderFrame");

  float fraction = CurrentFraction();
  gfx::Rect overlay_rect;
  const gfx::RectF unity_rect = gfx::RectF(0, 0, 1, 1);

  OverlayCandidatesOzone::OverlaySurfaceCandidateList overlay_list;
  if (!disable_primary_plane_) {
    overlay_list.push_back(
        MakeOverlayCandidate(1, gfx::Rect(size_), unity_rect));
    // We know at least the primary plane can be scanned out.
    overlay_list.back().overlay_handled = true;
  }
  if (overlay_buffer_[0]) {
    overlay_rect = gfx::Rect(overlay_buffer_[0]->size());

    float steps_num = 5.0f;
    float stepped_fraction =
        std::floor((fraction + 0.5f / steps_num) * steps_num) / steps_num;
    gfx::Vector2d offset(
        stepped_fraction * (size_.width() - overlay_rect.width()),
        (size_.height() - overlay_rect.height()) / 2);
    overlay_rect += offset;
    overlay_list.push_back(MakeOverlayCandidate(1, overlay_rect, unity_rect));
  }

  // The actual validation for a specific overlay configuration is done
  // asynchronously and then cached inside overlay_checker_ once a reply
  // is sent back.
  // This means that the first few frames we call this method for a specific
  // overlay_list, all the overlays but the primary plane, that we explicitly
  // marked as handled, will be rejected even if they might be handled at a
  // later time.
  overlay_checker_->CheckOverlaySupport(&overlay_list);

  gl_context_->MakeCurrent(gl_surface_.get());

  SkSurface* sk_surface = buffers_[back_buffer_]->sk_surface();
  if (use_ddl_) {
    StartDDLRenderThreadIfNecessary(sk_surface);
    auto ddl = GetDDL();
    sk_surface->draw(ddl.get());
  } else {
    Draw(sk_surface->getCanvas(), NextFraction());
  }
  gr_context_->flush();
  glFinish();

  if (!disable_primary_plane_) {
    CHECK(overlay_list.front().overlay_handled);
    gl_surface_->ScheduleOverlayPlane(
        0, gfx::OVERLAY_TRANSFORM_NONE, buffers_[back_buffer_]->image(),
        primary_plane_rect_, unity_rect, /* enable_blend */ true,
        /* gpu_fence */ nullptr);
  }

  if (overlay_buffer_[0] && overlay_list.back().overlay_handled) {
    gl_surface_->ScheduleOverlayPlane(
        1, gfx::OVERLAY_TRANSFORM_NONE, overlay_buffer_[back_buffer_]->image(),
        overlay_rect, unity_rect, /* enable_blend */ true,
        /* gpu_fence */ nullptr);
  }

  back_buffer_ ^= 1;
  gl_surface_->SwapBuffersAsync(
      base::BindRepeating(&SurfacelessSkiaGlRenderer::PostRenderFrameTask,
                          weak_ptr_factory_.GetWeakPtr()),
      base::DoNothing());
}

void SurfacelessSkiaGlRenderer::PostRenderFrameTask(
    gfx::SwapResult result,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  switch (result) {
    case gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS:
      for (size_t i = 0; i < base::size(buffers_); ++i) {
        buffers_[i] = std::make_unique<BufferWrapper>();
        if (!buffers_[i]->Initialize(gr_context_.get(), widget_,
                                     primary_plane_rect_.size()))
          LOG(FATAL) << "Failed to recreate buffer";
      }
      FALLTHROUGH;  // We want to render a new frame anyways.
    case gfx::SwapResult::SWAP_ACK:
      SkiaGlRenderer::PostRenderFrameTask(result, std::move(gpu_fence));
      break;
    case gfx::SwapResult::SWAP_FAILED:
      LOG(FATAL) << "Failed to swap buffers";
      break;
  }
}

}  // namespace ui
