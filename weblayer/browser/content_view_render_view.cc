// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/content_view_render_view.h"

#include <android/bitmap.h>
#include <android/native_window_jni.h>

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/size.h"
#include "weblayer/browser/java/jni/ContentViewRenderView_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

ContentViewRenderView::ContentViewRenderView(JNIEnv* env,
                                             jobject obj,
                                             gfx::NativeWindow root_window)
    : root_window_(root_window) {
  java_obj_.Reset(env, obj);
}

ContentViewRenderView::~ContentViewRenderView() {
  DCHECK(height_changed_listener_.is_null());
}

void ContentViewRenderView::SetHeightChangedListener(
    base::RepeatingClosure callback) {
  DCHECK(height_changed_listener_.is_null() || callback.is_null());
  height_changed_listener_ = std::move(callback);
}

// static
static jlong JNI_ContentViewRenderView_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jroot_window_android) {
  gfx::NativeWindow root_window =
      ui::WindowAndroid::FromJavaWindowAndroid(jroot_window_android);
  ContentViewRenderView* content_view_render_view =
      new ContentViewRenderView(env, obj, root_window);
  return reinterpret_cast<intptr_t>(content_view_render_view);
}

void ContentViewRenderView::Destroy(JNIEnv* env) {
  delete this;
}

void ContentViewRenderView::SetCurrentWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  InitCompositor();
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (web_contents_layer_)
    web_contents_layer_->RemoveFromParent();
  web_contents_layer_ = web_contents ? web_contents->GetNativeView()->GetLayer()
                                     : scoped_refptr<cc::Layer>();

  if (web_contents_layer_)
    root_container_layer_->AddChild(web_contents_layer_);
}

void ContentViewRenderView::OnPhysicalBackingSizeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jint width,
    jint height,
    jboolean for_config_change) {
  bool height_changed = height_ != height;
  height_ = height;
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  gfx::Size size(width, height);

  // The default resize timeout on Android is 1s. It was chosen with browser
  // use case in mind where resize is rare (eg orientation change, fullscreen)
  // and users are generally willing to wait for those cases instead of seeing
  // a frame at the wrong size. Weblayer currently can be resized while user
  // is interacting with the page, in which case the timeout is too long.
  // For now, use the default long timeout only for rotation (ie config change)
  // and just use a zero timeout for all other cases.
  base::Optional<base::TimeDelta> override_deadline;
  if (!for_config_change)
    override_deadline = base::TimeDelta();
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(
      size, override_deadline);

  if (height_changed && !height_changed_listener_.is_null())
    height_changed_listener_.Run();
}

void ContentViewRenderView::SurfaceCreated(JNIEnv* env) {
  InitCompositor();
  current_surface_format_ = 0;
}

void ContentViewRenderView::SurfaceDestroyed(JNIEnv* env,
                                             jboolean cache_back_buffer) {
  if (cache_back_buffer)
    compositor_->CacheBackBufferForCurrentSurface();
  compositor_->SetSurface(nullptr, false);
  current_surface_format_ = 0;
}

void ContentViewRenderView::SurfaceChanged(
    JNIEnv* env,
    jboolean can_be_used_with_surface_control,
    jint format,
    jint width,
    jint height,
    const JavaParamRef<jobject>& surface) {
  current_surface_format_ = format;
  compositor_->SetSurface(surface, can_be_used_with_surface_control);
  compositor_->SetWindowBounds(gfx::Size(width, height));
}

void ContentViewRenderView::SetNeedsRedraw(JNIEnv* env) {
  compositor_->SetNeedsRedraw();
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewRenderView::GetResourceManager(JNIEnv* env) {
  return compositor_->GetResourceManager().GetJavaObject();
}

void ContentViewRenderView::UpdateBackgroundColor(JNIEnv* env) {
  if (!compositor_)
    return;
  compositor_->SetBackgroundColor(
      Java_ContentViewRenderView_getBackgroundColor(env, java_obj_));
}

void ContentViewRenderView::UpdateLayerTreeHost() {
  // TODO(wkorman): Rename Layout to UpdateLayerTreeHost in all Android
  // Compositor related classes.
}

void ContentViewRenderView::DidSwapFrame(int pending_frames) {
  JNIEnv* env = base::android::AttachCurrentThread();
  TRACE_EVENT0("weblayer", "Java_ContentViewRenderView_didSwapFrame");
  if (Java_ContentViewRenderView_didSwapFrame(env, java_obj_)) {
    compositor_->SetNeedsRedraw();
  }
}

void ContentViewRenderView::DidSwapBuffers(const gfx::Size& swap_size) {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool matches_window_bounds = swap_size == compositor_->GetWindowBounds();
  Java_ContentViewRenderView_didSwapBuffers(env, java_obj_,
                                            matches_window_bounds);
}

void ContentViewRenderView::EvictCachedSurface(JNIEnv* env) {
  compositor_->EvictCachedBackBuffer();
}

void ContentViewRenderView::InitCompositor() {
  if (compositor_)
    return;

  compositor_.reset(content::Compositor::Create(this, root_window_));
  root_container_layer_ = cc::Layer::Create();
  root_container_layer_->SetHitTestable(false);
  root_container_layer_->SetElementId(
      cc::ElementId(root_container_layer_->id()));
  root_container_layer_->SetIsDrawable(false);
  compositor_->SetRootLayer(root_container_layer_);
  UpdateBackgroundColor(base::android::AttachCurrentThread());
}

}  // namespace weblayer
