// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_format.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gl/dc_layer_overlay_params.h"
#else
namespace gl {
struct DCLayerOverlayParams {};
}  // namespace gl
#endif

namespace gl {

namespace {

ABSL_CONST_INIT thread_local GLSurface* current_surface = nullptr;

}  // namespace

// static
GpuPreference GLSurface::forced_gpu_preference_ = GpuPreference::kDefault;

GLSurface::GLSurface() = default;

bool GLSurface::Initialize() {
  return Initialize(GLSurfaceFormat());
}

bool GLSurface::Initialize(GLSurfaceFormat format) {
  return true;
}

bool GLSurface::Resize(const gfx::Size& size,
                       float scale_factor,
                       const gfx::ColorSpace& color_space,
                       bool has_alpha) {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::Recreate() {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::DeferDraws() {
  return false;
}

bool GLSurface::SupportsSwapBuffersWithBounds() {
  return false;
}

bool GLSurface::SupportsPostSubBuffer() {
  return false;
}

bool GLSurface::SupportsAsyncSwap() {
  return false;
}

unsigned int GLSurface::GetBackingFramebufferObject() {
  return 0;
}

void GLSurface::SwapBuffersAsync(SwapCompletionCallback completion_callback,
                                 PresentationCallback presentation_callback,
                                 gfx::FrameData data) {
  NOTREACHED();
}

gfx::SwapResult GLSurface::SwapBuffersWithBounds(
    const std::vector<gfx::Rect>& rects,
    PresentationCallback callback,
    gfx::FrameData data) {
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::SwapResult GLSurface::PostSubBuffer(int x,
                                         int y,
                                         int width,
                                         int height,
                                         PresentationCallback callback,
                                         gfx::FrameData data) {
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurface::PostSubBufferAsync(int x,
                                   int y,
                                   int width,
                                   int height,
                                   SwapCompletionCallback completion_callback,
                                   PresentationCallback presentation_callback,
                                   gfx::FrameData data) {
  NOTREACHED();
}

bool GLSurface::OnMakeCurrent(GLContext* context) {
  return true;
}

bool GLSurface::SetBackbufferAllocation(bool allocated) {
  return true;
}

void GLSurface::SetFrontbufferAllocation(bool allocated) {
}

void* GLSurface::GetShareHandle() {
  NOTIMPLEMENTED();
  return NULL;
}

GLDisplay* GLSurface::GetGLDisplay() {
  NOTIMPLEMENTED();
  return NULL;
}

void* GLSurface::GetConfig() {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::VSyncProvider* GLSurface::GetVSyncProvider() {
  return NULL;
}

void GLSurface::SetVSyncEnabled(bool enabled) {}

bool GLSurface::ScheduleDCLayer(std::unique_ptr<DCLayerOverlayParams> params) {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::SetEnableDCLayers(bool enable) {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::IsSurfaceless() const {
  return false;
}

gfx::SurfaceOrigin GLSurface::GetOrigin() const {
  return gfx::SurfaceOrigin::kBottomLeft;
}

bool GLSurface::BuffersFlipped() const {
  return false;
}

bool GLSurface::SupportsDCLayers() const {
  return false;
}

bool GLSurface::SupportsProtectedVideo() const {
  return false;
}

bool GLSurface::SupportsOverridePlatformSize() const {
  return false;
}

bool GLSurface::SetDrawRectangle(const gfx::Rect& rect) {
  return false;
}

gfx::Vector2d GLSurface::GetDrawOffset() const {
  return gfx::Vector2d();
}

bool GLSurface::SupportsSwapTimestamps() const {
  return false;
}

void GLSurface::SetEnableSwapTimestamps() {
  NOTREACHED();
}

int GLSurface::GetBufferCount() const {
  return 2;
}

bool GLSurface::SupportsPlaneGpuFences() const {
  return false;
}

EGLTimestampClient* GLSurface::GetEGLTimestampClient() {
  return nullptr;
}

bool GLSurface::SupportsGpuVSync() const {
  return false;
}

bool GLSurface::SupportsDelegatedInk() {
  return false;
}

void GLSurface::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  NOTREACHED();
}

void GLSurface::SetGpuVSyncEnabled(bool enabled) {}

GLSurface* GLSurface::GetCurrent() {
  return current_surface;
}

bool GLSurface::IsCurrent() {
  return GetCurrent() == this;
}

// static
void GLSurface::SetForcedGpuPreference(GpuPreference gpu_preference) {
  DCHECK_EQ(GpuPreference::kDefault, forced_gpu_preference_);
  forced_gpu_preference_ = gpu_preference;
}

// static
GpuPreference GLSurface::AdjustGpuPreference(GpuPreference gpu_preference) {
  switch (forced_gpu_preference_) {
    case GpuPreference::kDefault:
      return gpu_preference;
    case GpuPreference::kLowPower:
    case GpuPreference::kHighPerformance:
      return forced_gpu_preference_;
    default:
      NOTREACHED();
      return GpuPreference::kDefault;
  }
}

GLSurface::~GLSurface() {
  if (GetCurrent() == this) {
    ClearCurrent();
  }
}

void GLSurface::ClearCurrent() {
  current_surface = nullptr;
}

void GLSurface::SetCurrent() {
  current_surface = this;
}

bool GLSurface::ExtensionsContain(const char* c_extensions, const char* name) {
  DCHECK(name);
  if (!c_extensions)
    return false;
  std::string extensions(c_extensions);
  extensions += " ";

  std::string delimited_name(name);
  delimited_name += " ";

  return extensions.find(delimited_name) != std::string::npos;
}

GLSurfaceAdapter::GLSurfaceAdapter(GLSurface* surface) : surface_(surface) {}

bool GLSurfaceAdapter::Initialize(GLSurfaceFormat format) {
  return surface_->Initialize(format);
}

void GLSurfaceAdapter::Destroy() {
  surface_->Destroy();
}

bool GLSurfaceAdapter::Resize(const gfx::Size& size,
                              float scale_factor,
                              const gfx::ColorSpace& color_space,
                              bool has_alpha) {
  return surface_->Resize(size, scale_factor, color_space, has_alpha);
}

bool GLSurfaceAdapter::Recreate() {
  return surface_->Recreate();
}

bool GLSurfaceAdapter::DeferDraws() {
  return surface_->DeferDraws();
}

bool GLSurfaceAdapter::IsOffscreen() {
  return surface_->IsOffscreen();
}

gfx::SwapResult GLSurfaceAdapter::SwapBuffers(PresentationCallback callback,
                                              gfx::FrameData data) {
  return surface_->SwapBuffers(std::move(callback), data);
}

void GLSurfaceAdapter::SwapBuffersAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  surface_->SwapBuffersAsync(std::move(completion_callback),
                             std::move(presentation_callback), data);
}

gfx::SwapResult GLSurfaceAdapter::SwapBuffersWithBounds(
    const std::vector<gfx::Rect>& rects,
    PresentationCallback callback,
    gfx::FrameData data) {
  return surface_->SwapBuffersWithBounds(rects, std::move(callback), data);
}

gfx::SwapResult GLSurfaceAdapter::PostSubBuffer(int x,
                                                int y,
                                                int width,
                                                int height,
                                                PresentationCallback callback,
                                                gfx::FrameData data) {
  return surface_->PostSubBuffer(x, y, width, height, std::move(callback),
                                 data);
}

void GLSurfaceAdapter::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  surface_->PostSubBufferAsync(x, y, width, height,
                               std::move(completion_callback),
                               std::move(presentation_callback), data);
}

bool GLSurfaceAdapter::SupportsSwapBuffersWithBounds() {
  return surface_->SupportsSwapBuffersWithBounds();
}
bool GLSurfaceAdapter::SupportsPostSubBuffer() {
  return surface_->SupportsPostSubBuffer();
}

bool GLSurfaceAdapter::SupportsAsyncSwap() {
  return surface_->SupportsAsyncSwap();
}

gfx::Size GLSurfaceAdapter::GetSize() {
  return surface_->GetSize();
}

void* GLSurfaceAdapter::GetHandle() {
  return surface_->GetHandle();
}

unsigned int GLSurfaceAdapter::GetBackingFramebufferObject() {
  return surface_->GetBackingFramebufferObject();
}

bool GLSurfaceAdapter::OnMakeCurrent(GLContext* context) {
  return surface_->OnMakeCurrent(context);
}

bool GLSurfaceAdapter::SetBackbufferAllocation(bool allocated) {
  return surface_->SetBackbufferAllocation(allocated);
}

void GLSurfaceAdapter::SetFrontbufferAllocation(bool allocated) {
  surface_->SetFrontbufferAllocation(allocated);
}

void* GLSurfaceAdapter::GetShareHandle() {
  return surface_->GetShareHandle();
}

GLDisplay* GLSurfaceAdapter::GetGLDisplay() {
  return surface_->GetGLDisplay();
}

void* GLSurfaceAdapter::GetConfig() {
  return surface_->GetConfig();
}

GLSurfaceFormat GLSurfaceAdapter::GetFormat() {
  return surface_->GetFormat();
}

gfx::VSyncProvider* GLSurfaceAdapter::GetVSyncProvider() {
  return surface_->GetVSyncProvider();
}

void GLSurfaceAdapter::SetVSyncEnabled(bool enabled) {
  surface_->SetVSyncEnabled(enabled);
}

bool GLSurfaceAdapter::ScheduleDCLayer(
    std::unique_ptr<DCLayerOverlayParams> params) {
  return surface_->ScheduleDCLayer(std::move(params));
}

bool GLSurfaceAdapter::SetEnableDCLayers(bool enable) {
  return surface_->SetEnableDCLayers(enable);
}

bool GLSurfaceAdapter::IsSurfaceless() const {
  return surface_->IsSurfaceless();
}

gfx::SurfaceOrigin GLSurfaceAdapter::GetOrigin() const {
  return surface_->GetOrigin();
}

bool GLSurfaceAdapter::BuffersFlipped() const {
  return surface_->BuffersFlipped();
}

bool GLSurfaceAdapter::SupportsDCLayers() const {
  return surface_->SupportsDCLayers();
}

bool GLSurfaceAdapter::SupportsProtectedVideo() const {
  return surface_->SupportsProtectedVideo();
}

bool GLSurfaceAdapter::SupportsOverridePlatformSize() const {
  return surface_->SupportsOverridePlatformSize();
}

bool GLSurfaceAdapter::SetDrawRectangle(const gfx::Rect& rect) {
  return surface_->SetDrawRectangle(rect);
}

gfx::Vector2d GLSurfaceAdapter::GetDrawOffset() const {
  return surface_->GetDrawOffset();
}

bool GLSurfaceAdapter::SupportsSwapTimestamps() const {
  return surface_->SupportsSwapTimestamps();
}

void GLSurfaceAdapter::SetEnableSwapTimestamps() {
  return surface_->SetEnableSwapTimestamps();
}

int GLSurfaceAdapter::GetBufferCount() const {
  return surface_->GetBufferCount();
}

bool GLSurfaceAdapter::SupportsPlaneGpuFences() const {
  return surface_->SupportsPlaneGpuFences();
}

bool GLSurfaceAdapter::SupportsGpuVSync() const {
  return surface_->SupportsGpuVSync();
}

void GLSurfaceAdapter::SetGpuVSyncEnabled(bool enabled) {
  surface_->SetGpuVSyncEnabled(enabled);
}

void GLSurfaceAdapter::SetFrameRate(float frame_rate) {
  surface_->SetFrameRate(frame_rate);
}

void GLSurfaceAdapter::SetCurrent() {
  surface_->SetCurrent();
}

bool GLSurfaceAdapter::IsCurrent() {
  return surface_->IsCurrent();
}

bool GLSurfaceAdapter::SupportsDelegatedInk() {
  return surface_->SupportsDelegatedInk();
}

void GLSurfaceAdapter::SetDelegatedInkTrailStartPoint(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  surface_->SetDelegatedInkTrailStartPoint(std::move(metadata));
}

void GLSurfaceAdapter::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  surface_->InitDelegatedInkPointRendererReceiver(std::move(pending_receiver));
}

GLSurfaceAdapter::~GLSurfaceAdapter() = default;

scoped_refptr<GLSurface> InitializeGLSurfaceWithFormat(
    scoped_refptr<GLSurface> surface, GLSurfaceFormat format) {
  if (!surface->Initialize(format))
    return nullptr;
  return surface;
}

scoped_refptr<GLSurface> InitializeGLSurface(scoped_refptr<GLSurface> surface) {
  return InitializeGLSurfaceWithFormat(surface, GLSurfaceFormat());
}

}  // namespace gl
