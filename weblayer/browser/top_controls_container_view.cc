// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/top_controls_container_view.h"

#include "base/android/jni_string.h"
#include "cc/layers/ui_resource_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_controls_state.h"
#include "ui/android/resources/resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/view_android.h"
#include "weblayer/browser/content_view_render_view.h"
#include "weblayer/browser/java/jni/TopControlsContainerView_jni.h"

using base::android::AttachCurrentThread;

namespace weblayer {

TopControlsContainerView::TopControlsContainerView(
    const base::android::JavaParamRef<jobject>&
        java_top_controls_container_view,
    ContentViewRenderView* content_view_render_view)
    : java_top_controls_container_view_(java_top_controls_container_view),
      content_view_render_view_(content_view_render_view) {
  DCHECK(content_view_render_view_);
}

TopControlsContainerView::~TopControlsContainerView() = default;

int TopControlsContainerView::GetTopControlsHeight() {
  return top_controls_layer_ ? top_controls_layer_->bounds().height() : 0;
}

void TopControlsContainerView::CreateTopControlsLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    int id) {
  top_controls_resource_id_ = id;
  top_controls_layer_ = cc::UIResourceLayer::Create();
  // Real size is sent in SetTopControlsSize().
  top_controls_layer_->SetBounds(gfx::Size(1, 1));
  top_controls_layer_->SetPosition(gfx::PointF(0, 0));
  top_controls_layer_->SetElementId(cc::ElementId(top_controls_layer_->id()));
  top_controls_layer_->SetHitTestable(false);
  top_controls_layer_->SetIsDrawable(true);
  content_view_render_view_->root_container_layer()->AddChild(
      top_controls_layer_);
  UpdateTopControlsResource(env, caller);
}

void TopControlsContainerView::DeleteTopControlsContainerView(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  delete this;
}

void TopControlsContainerView::DeleteTopControlsLayer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  top_controls_layer_.reset();
}

void TopControlsContainerView::SetTopControlsOffset(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    int top_controls_offset_y,
    int top_content_offset_y) {
  // |top_controls_layer_| may not be created if the top controls view has 0
  // height.
  if (top_controls_layer_)
    top_controls_layer_->SetPosition(gfx::PointF(0, top_controls_offset_y));
  if (web_contents()) {
    web_contents()->GetNativeView()->GetLayer()->SetPosition(
        gfx::PointF(0, top_content_offset_y));
  }
}

void TopControlsContainerView::SetTopControlsSize(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    int width,
    int height) {
  DCHECK(top_controls_layer_);
  top_controls_layer_->SetBounds(gfx::Size(width, height));
}

void TopControlsContainerView::UpdateTopControlsResource(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  DCHECK(top_controls_layer_);
  ui::ResourceManager& resource_manager =
      content_view_render_view_->compositor()->GetResourceManager();
  ui::Resource* top_controls_resource = resource_manager.GetResource(
      ui::ANDROID_RESOURCE_TYPE_DYNAMIC, top_controls_resource_id_);
  DCHECK(top_controls_resource);
  top_controls_layer_->SetUIResourceId(
      top_controls_resource->ui_resource()->id());
}

void TopControlsContainerView::SetWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaParamRef<jobject>& web_contents) {
  Observe(content::WebContents::FromJavaWebContents(web_contents));
}

void TopControlsContainerView::DidToggleFullscreenModeForTab(
    bool entered_fullscreen,
    bool will_cause_resize) {
  TRACE_EVENT0("weblayer",
               "Java_TopControlsContainerView_didToggleFullscreenModeForTab");
  Java_TopControlsContainerView_didToggleFullscreenModeForTab(
      AttachCurrentThread(), java_top_controls_container_view_,
      entered_fullscreen);
}

static jlong JNI_TopControlsContainerView_CreateTopControlsContainerView(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>&
        java_top_controls_container_view,
    jlong native_content_view_render_view) {
  return reinterpret_cast<jlong>(
      new TopControlsContainerView(java_top_controls_container_view,
                                   reinterpret_cast<ContentViewRenderView*>(
                                       native_content_view_render_view)));
}

}  // namespace weblayer
