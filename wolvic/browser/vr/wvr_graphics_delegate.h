// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_VR_WVR_GRAPHICS_DELEGATE_H_
#define WOLVIC_BROWSER_VR_WVR_GRAPHICS_DELEGATE_H_

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace device {
class WebXrPresentationState;
}

namespace gl {
class GLContext;
class GLSurface;
class SurfaceTexture;
}  // namespace gl

namespace wolvic {

class WvrGraphicsDelegate {
 public:
  WvrGraphicsDelegate();

  WvrGraphicsDelegate(const WvrGraphicsDelegate&) = delete;
  WvrGraphicsDelegate& operator=(const WvrGraphicsDelegate&) = delete;

  ~WvrGraphicsDelegate();

  void set_webxr_presentation_state(device::WebXrPresentationState* webxr) {
    webxr_ = webxr;
  }

  void InitializeGl(const gfx::Size& frame_size,
                    base::OnceClosure callback);

  base::WeakPtr<WvrGraphicsDelegate> GetWeakPtr();

  // WvrManager communicates with this class through these functions.
  bool CreateOrResizeWebXrSurface(
      const gfx::Size& size,
      base::RepeatingClosure on_webxr_frame_available);
  gl::SurfaceTexture* webxr_surface_texture() {
    return webxr_surface_texture_.get();
  }
  gfx::Size get_screen_size() const { return screen_size_; }
  gfx::Size webxr_surface_size() const { return webxr_surface_size_; }
  int32_t webxr_texture_handle() const { return texture_handle_id_; }

 private:
  raw_ptr<device::WebXrPresentationState> webxr_;

   // samplerExternalOES texture data for WebVR content image.
  int webvr_texture_id_ = 0;
  int32_t texture_handle_id_;

  // Java WVRSurfaceTexture instance.
  base::android::ScopedJavaGlobalRef<jobject> j_surface_texture_;

  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::SurfaceTexture> webxr_surface_texture_;

  gfx::Size screen_size_;
  gfx::Size webxr_surface_size_;

  base::WeakPtrFactory<WvrGraphicsDelegate> weak_ptr_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_VR_WVR_GRAPHICS_DELEGATE_H_
