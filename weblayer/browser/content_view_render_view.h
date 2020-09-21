// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_VIEW_CONTENT_VIEW_RENDER_VIEW_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_VIEW_CONTENT_VIEW_RENDER_VIEW_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/android/compositor_client.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class Layer;
}

namespace content {
class Compositor;
}  // namespace content

namespace weblayer {

class ContentViewRenderView : public content::CompositorClient {
 public:
  ContentViewRenderView(JNIEnv* env,
                        jobject obj,
                        gfx::NativeWindow root_window);

  content::Compositor* compositor() { return compositor_.get(); }

  scoped_refptr<cc::Layer> root_container_layer() {
    return root_container_layer_;
  }

  // Height, in pixels.
  int height() const { return height_; }
  void SetHeightChangedListener(base::RepeatingClosure callback);

  // Methods called from Java via JNI -----------------------------------------
  void Destroy(JNIEnv* env);
  void SetCurrentWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents);
  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height,
      jboolean for_config_change);
  void SurfaceCreated(JNIEnv* env);
  void SurfaceDestroyed(JNIEnv* env, jboolean cache_back_buffer);
  void SurfaceChanged(JNIEnv* env,
                      jboolean can_be_used_with_surface_control,
                      jint format,
                      jint width,
                      jint height,
                      const base::android::JavaParamRef<jobject>& surface);
  void SetNeedsRedraw(JNIEnv* env);
  void EvictCachedSurface(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetResourceManager(JNIEnv* env);
  void UpdateBackgroundColor(JNIEnv* env);

  // CompositorClient implementation
  void UpdateLayerTreeHost() override;
  void DidSwapFrame(int pending_frames) override;
  void DidSwapBuffers(const gfx::Size& swap_size) override;

 private:
  ~ContentViewRenderView() override;

  void InitCompositor();

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  std::unique_ptr<content::Compositor> compositor_;

  gfx::NativeWindow root_window_;

  // Set as the root-layer of the compositor. Contains |web_contents_layer_|.
  scoped_refptr<cc::Layer> root_container_layer_;
  scoped_refptr<cc::Layer> web_contents_layer_;

  int current_surface_format_ = 0;

  base::RepeatingClosure height_changed_listener_;
  int height_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ContentViewRenderView);
};

}  // namespace weblayer

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_VIEW_CONTENT_VIEW_RENDER_VIEW_H_
