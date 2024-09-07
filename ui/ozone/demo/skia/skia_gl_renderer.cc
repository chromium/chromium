// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/skia/skia_gl_renderer.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/font_utils.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "third_party/skia/include/private/chromium/GrDeferredDisplayList.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace ui {

namespace {

const char kUseDDL[] = "use-ddl";

}  // namespace

SkiaGlRenderer::SkiaGlRenderer(
    gfx::AcceleratedWidget widget,
    std::unique_ptr<PlatformWindowSurface> window_surface,
    const scoped_refptr<gl::GLSurface>& surface,
    const gfx::Size& size)
    : RendererBase(widget, size),
      window_surface_(std::move(window_surface)),
      gl_surface_(surface),
      use_ddl_(base::CommandLine::ForCurrentProcess()->HasSwitch(kUseDDL)),
      condition_variable_(&lock_) {}

SkiaGlRenderer::~SkiaGlRenderer() {
  if (use_ddl_)
    StopDDLRenderThread();
}

bool SkiaGlRenderer::Initialize() {
  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                          gl::GLContextAttribs());
  if (!gl_context_.get()) {
    LOG(FATAL) << "Failed to create GL context";
  }

  gl_surface_->Resize(size_, 1.f, gfx::ColorSpace(), true);

  if (!gl_context_->MakeCurrent(gl_surface_.get())) {
    LOG(FATAL) << "Failed to make GL context current";
  }

  sk_sp<const GrGLInterface> native_interface = GrGLMakeAssembledInterface(
      nullptr,
      [](void* ctx, const char name[]) { return gl::GetGLProcAddress(name); });
  DCHECK(native_interface);
  GrContextOptions options;
  // TODO(csmartdalton): enable internal multisampling after the related Skia
  // rolls are in.
  options.fInternalMultisampleCount = 0;
  gr_context_ = GrDirectContexts::MakeGL(std::move(native_interface), options);
  DCHECK(gr_context_);

  PostRenderFrameTask(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
  return true;
}

void SkiaGlRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "SkiaGlRenderer::RenderFrame");

  SkSurfaceProps surface_props =
      skia::LegacyDisplayGlobals::GetSkSurfaceProps();

  if (!sk_surface_) {
    GrGLFramebufferInfo framebuffer_info;
    framebuffer_info.fFBOID = 0;
    framebuffer_info.fFormat = GL_RGBA8;
    auto render_target = GrBackendRenderTargets::MakeGL(
        size_.width(), size_.height(), 0, 8, framebuffer_info);

    sk_surface_ = SkSurfaces::WrapBackendRenderTarget(
        gr_context_.get(), render_target, kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, nullptr, &surface_props);
  }

  if (use_ddl_) {
    StartDDLRenderThreadIfNecessary(sk_surface_.get());
    skgpu::ganesh::DrawDDL(sk_surface_, GetDDL());
  } else {
    Draw(sk_surface_->getCanvas(), NextFraction());
  }
  gr_context_->flushAndSubmit();
  glFinish();

  if (gl_surface_->SupportsAsyncSwap()) {
    gl_surface_->SwapBuffersAsync(
        base::BindOnce(&SkiaGlRenderer::PostRenderFrameTask,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SkiaGlRenderer::OnPresentation,
                       weak_ptr_factory_.GetWeakPtr()),
        gfx::FrameData());
  } else {
    PostRenderFrameTask(gfx::SwapCompletionResult(
        gl_surface_->SwapBuffers(base::BindOnce(&SkiaGlRenderer::OnPresentation,
                                                weak_ptr_factory_.GetWeakPtr()),
                                 gfx::FrameData())));
  }
}

void SkiaGlRenderer::PostRenderFrameTask(gfx::SwapCompletionResult result) {
  if (!result.release_fence.is_null())
    gfx::GpuFence(std::move(result.release_fence)).Wait();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SkiaGlRenderer::RenderFrame,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SkiaGlRenderer::Draw(SkCanvas* canvas, float fraction) {
  TRACE_EVENT0("ozone", "SkiaGlRenderer::Draw");
  // Clear background
  canvas->clear(SkColorSetARGB(255, 255 * fraction, 255 * (1 - fraction), 0));

  SkPaint paint;
  paint.setColor(SK_ColorRED);

  // Draw a rectangle with red paint
  SkRect rect = SkRect::MakeXYWH(10, 10, 128, 128);
  canvas->drawRect(rect, paint);

  // Set up a linear gradient and draw a circle
  {
    SkPoint linearPoints[] = {{0, 0}, {300, 300}};
    SkColor linearColors[] = {SK_ColorGREEN, SK_ColorBLACK};
    paint.setShader(SkGradientShader::MakeLinear(
        linearPoints, linearColors, nullptr, 2, SkTileMode::kMirror));
    paint.setAntiAlias(true);

    canvas->drawCircle(200, 200, 64, paint);

    // Detach shader
    paint.setShader(nullptr);
  }

  // Draw a message with a nice black paint
  paint.setColor(SK_ColorBLACK);

  SkFont font = skia::DefaultFont();
  font.setSize(32);
  font.setSubpixel(true);

  canvas->save();
  static const char message[] = "Hello Ozone";
  static const char message_ddl[] = "Hello Ozone + DDL";

  // Translate and rotate
  canvas->translate(300, 300);
  rotation_angle_ += 0.2f;
  if (rotation_angle_ > 360) {
    rotation_angle_ -= 360;
  }
  canvas->rotate(rotation_angle_);

  const char* text = use_ddl_ ? message_ddl : message;
  // Draw the text
  canvas->drawString(text, 0, 0, font, paint);
  canvas->restore();
}

void SkiaGlRenderer::StartDDLRenderThreadIfNecessary(SkSurface* sk_surface) {
  DCHECK(use_ddl_);
  if (ddl_render_thread_)
    return;
  DCHECK(!surface_charaterization_.isValid());
  DCHECK(sk_surface);

  if (!sk_surface->characterize(&surface_charaterization_))
    LOG(FATAL) << "Failed to cheracterize the skia surface!";
  ddl_render_thread_ =
      std::make_unique<base::DelegateSimpleThread>(this, "DDLRenderThread");
  ddl_render_thread_->Start();
}

void SkiaGlRenderer::StopDDLRenderThread() {
  if (!ddl_render_thread_)
    return;
  {
    base::AutoLock auto_lock_(lock_);
    surface_charaterization_ = GrSurfaceCharacterization();
    condition_variable_.Signal();
  }
  ddl_render_thread_->Join();
  ddl_render_thread_ = nullptr;
  while (!ddls_.empty())
    ddls_.pop();
}

sk_sp<GrDeferredDisplayList> SkiaGlRenderer::GetDDL() {
  base::AutoLock auto_lock_(lock_);
  DCHECK(surface_charaterization_.isValid());
  // Wait until DDL is generated by DDL render thread.
  while (ddls_.empty())
    condition_variable_.Wait();
  auto ddl = std::move(ddls_.front());
  ddls_.pop();
  condition_variable_.Signal();
  return ddl;
}

void SkiaGlRenderer::Run() {
  base::AutoLock auto_lock(lock_);
  while (true) {
    // Wait until ddls_ is consumed or surface_charaterization_ is reset.
    constexpr size_t kMaxPendingDDLS = 4;
    while (surface_charaterization_.isValid() &&
           ddls_.size() == kMaxPendingDDLS)
      condition_variable_.Wait();
    if (!surface_charaterization_.isValid())
      break;
    DCHECK_LT(ddls_.size(), kMaxPendingDDLS);
    GrDeferredDisplayListRecorder recorder(surface_charaterization_);
    sk_sp<GrDeferredDisplayList> ddl;
    {
      base::AutoUnlock auto_unlock(lock_);
      Draw(recorder.getCanvas(), NextFraction());
      ddl = recorder.detach();
    }
    ddls_.push(std::move(ddl));
    condition_variable_.Signal();
  }
}

void SkiaGlRenderer::OnPresentation(const gfx::PresentationFeedback& feedback) {
  LOG_IF(ERROR, feedback.timestamp.is_null()) << "Last frame is discarded!";
}

}  // namespace ui
