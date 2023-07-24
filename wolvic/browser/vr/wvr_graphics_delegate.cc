// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/vr/wvr_graphics_delegate.h"

#include "device/vr/android/web_xr_presentation_state.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/init/gl_factory.h"
#include "wolvic/jni_headers/WVRSurfaceTexture_jni.h"

namespace wolvic {

namespace {

int32_t GetNextTextureHandleId() {
  static int32_t s_next_texture_handle_id = 0;
  if (s_next_texture_handle_id == std::numeric_limits<int32_t>::max())
    s_next_texture_handle_id = 0;
  return ++s_next_texture_handle_id;
}

}  // namespace

WvrGraphicsDelegate::WvrGraphicsDelegate()
    : texture_handle_id_(GetNextTextureHandleId()) {}

WvrGraphicsDelegate::~WvrGraphicsDelegate() {
  if (j_surface_texture_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_WVRSurfaceTexture_release(env, j_surface_texture_);
  }
}

base::WeakPtr<WvrGraphicsDelegate> WvrGraphicsDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WvrGraphicsDelegate::InitializeGl(const gfx::Size& frame_size,
                                       base::OnceClosure callback) {
  screen_size_ = frame_size;

  gl::init::DisableANGLE();

  gl::GLDisplay* display = nullptr;
  if (gl::GetGLImplementation() == gl::kGLImplementationNone) {
    display = gl::init::InitializeGLOneOff(gl::GpuPreference::kDefault);
    if (!display) {
      LOG(ERROR) << "gl::init::InitializeGLOneOff failed";
      return;
    }
  } else {
    display = gl::GetDefaultDisplayEGL();
  }

  surface_ = gl::init::CreateOffscreenGLSurface(display, gfx::Size());

  if (!surface_.get()) {
    LOG(ERROR) << "gl::init::CreateOffscreenGLSurface failed";
    return;
  }

  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  if (!context_.get()) {
    LOG(ERROR) << "gl::init::CreateGLContext failed";
    return;
  }
  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return;
  }

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  unsigned int texture[1];
  glGenTextures(1, texture);
  webvr_texture_id_ = texture[0];

  std::move(callback).Run();
}

bool WvrGraphicsDelegate::CreateOrResizeWebXrSurface(
    const gfx::Size& size,
    base::RepeatingClosure on_webxr_frame_available) {
  DVLOG(2) << __func__ << ": size=" << size.width() << "x" << size.height();
  if (!webxr_surface_texture_) {
    DCHECK(on_webxr_frame_available)
        << "A callback must be provided to create the surface texture";
    webxr_surface_texture_ = gl::SurfaceTexture::Create(webvr_texture_id_);
    webxr_surface_texture_->SetFrameAvailableCallback(
        std::move(on_webxr_frame_available));
  }

  if (!j_surface_texture_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    j_surface_texture_ = Java_WVRSurfaceTexture_create(
        env,
        texture_handle_id_,
        webxr_surface_texture_.get()->j_surface_texture());
  }

  webxr_surface_texture_->SetDefaultBufferSize(size.width(), size.height());
  webxr_surface_size_ = size;
  return true;
}

}  // namespace wolvic
