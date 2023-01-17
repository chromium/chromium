// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/content_view_render_view.h"

#include <android/bitmap.h>

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  DCHECK(content_height_changed_listener_.is_null());
}

void ContentViewRenderView::SetContentHeightChangedListener(
    base::RepeatingClosure callback) {
  DCHECK(content_height_changed_listener_.is_null() || callback.is_null());
  content_height_changed_listener_ = std::move(callback);
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
  if (web_contents_)
    web_contents_->SetPageBaseBackgroundColor(absl::nullopt);
  if (web_contents_layer_)
    web_contents_layer_->RemoveFromParent();

  web_contents_ = web_contents;
  web_contents_layer_ = web_contents ? web_contents->GetNativeView()->GetLayer()
                                     : scoped_refptr<cc::Layer>();

  UpdateWebContentsBaseBackgroundColor();
  if (web_contents_layer_)
    root_container_layer_->AddChild(web_contents_layer_);
}

void ContentViewRenderView::OnViewportSizeChanged(JNIEnv* env,
                                                  jint width,
                                                  jint height) {
  bool content_height_changed = content_height_ != height;
  content_height_ = height;
  if (content_height_changed && !content_height_changed_listener_.is_null())
    content_height_changed_listener_.Run();
}

void ContentViewRenderView::OnPhysicalBackingSizeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jint width,
    jint height,
    jboolean for_config_change) {
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
  absl::optional<base::TimeDelta> override_deadline;
  if (!for_config_change)
    override_deadline = base::TimeDelta();
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(
      size, override_deadline);
}

void ContentViewRenderView::SurfaceCreated(JNIEnv* env) {
  InitCompositor();
}

void ContentViewRenderView::SurfaceDestroyed(JNIEnv* env,
                                             jboolean cache_back_buffer) {
  if (cache_back_buffer)
    compositor_->CacheBackBufferForCurrentSurface();

  // When we switch from Chrome to other app we can't detach child surface
  // controls because it leads to a visible hole: b/157439199. To avoid this we
  // don't detach surfaces if the surface is going to be destroyed, they will be
  // detached and freed by OS.
  compositor_->PreserveChildSurfaceControls();

  compositor_->SetSurface(nullptr, false);
}

void ContentViewRenderView::SurfaceChanged(
    JNIEnv* env,
    jboolean can_be_used_with_surface_control,
    jint width,
    jint height,
    jboolean transparent_background,
    const JavaParamRef<jobject>& new_surface) {
  use_transparent_background_ = transparent_background;
  UpdateWebContentsBaseBackgroundColor();
  compositor_->SetRequiresAlphaChannel(use_transparent_background_);
  // Java side will pass a null `new_surface` if the surface did not change.
  if (new_surface) {
    compositor_->SetSurface(new_surface, can_be_used_with_surface_control);
  }
  compositor_->SetWindowBounds(gfx::Size(width, height));
}

void ContentViewRenderView::SetNeedsRedraw(JNIEnv* env) {
  if (compositor_) {
    compositor_->SetNeedsRedraw();
  }
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewRenderView::GetResourceManager(JNIEnv* env) {
  return compositor_->GetResourceManager().GetJavaObject();
}

void ContentViewRenderView::UpdateBackgroundColor(JNIEnv* env) {
  if (!compositor_)
    return;
  compositor_->SetBackgroundColor(
      requires_alpha_channel_
          ? SK_ColorTRANSPARENT
          : Java_ContentViewRenderView_getBackgroundColor(env, java_obj_));
}

void ContentViewRenderView::SetRequiresAlphaChannel(
    JNIEnv* env,
    jboolean requires_alpha_channel) {
  requires_alpha_channel_ = requires_alpha_channel;
  UpdateBackgroundColor(env);
}

void ContentViewRenderView::SetDidSwapBuffersCallbackEnabled(JNIEnv* env,
                                                             jboolean enable) {
  InitCompositor();
  compositor_->SetDidSwapBuffersCallbackEnabled(enable);
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
  root_container_layer_->SetIsDrawable(false);
  compositor_->SetRootLayer(root_container_layer_);
  UpdateBackgroundColor(base::android::AttachCurrentThread());
}

void ContentViewRenderView::UpdateWebContentsBaseBackgroundColor() {
  if (!web_contents_)
    return;
  web_contents_->SetPageBaseBackgroundColor(
      use_transparent_background_ ? absl::make_optional(SK_ColorTRANSPARENT)
                                  : absl::nullopt);
}

}  // namespace weblayer
