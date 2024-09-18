// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include "base/check.h"
#include "base/notreached.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_format.h"

namespace gl {

namespace {

constinit thread_local GLSurface* current_surface = nullptr;

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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
}

bool GLSurface::OnMakeCurrent(GLContext* context) {
  return true;
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

bool GLSurface::IsSurfaceless() const {
  return false;
}

gfx::SurfaceOrigin GLSurface::GetOrigin() const {
  return gfx::SurfaceOrigin::kBottomLeft;
}

bool GLSurface::BuffersFlipped() const {
  return false;
}

bool GLSurface::SupportsOverridePlatformSize() const {
  return false;
}

bool GLSurface::SupportsSwapTimestamps() const {
  return false;
}

void GLSurface::SetEnableSwapTimestamps() {
  NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
      return GpuPreference::kDefault;
  }
}

GLSurface::~GLSurface() {
  // InvalidateWeakPtrs should be called from the most concrete class.
  CHECK(!weak_ptr_factory_.HasWeakPtrs());
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

base::WeakPtr<GLSurface> GLSurface::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GLSurface::InvalidateWeakPtrs() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool GLSurface::HasWeakPtrs() {
  return weak_ptr_factory_.HasWeakPtrs();
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

scoped_refptr<GLSurface> InitializeGLSurface(scoped_refptr<GLSurface> surface) {
  if (!surface->Initialize(GLSurfaceFormat())) {
    return nullptr;
  }
  return surface;
}

}  // namespace gl
