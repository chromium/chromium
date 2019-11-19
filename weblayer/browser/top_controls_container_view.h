// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TOP_CONTROLS_CONTAINER_VIEW_H_
#define WEBLAYER_BROWSER_TOP_CONTROLS_CONTAINER_VIEW_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"

namespace cc {
class UIResourceLayer;
}  // namespace cc

namespace content {
class WebContents;
}

namespace weblayer {

class ContentViewRenderView;

// Native side of TopControlsContainerView. Responsible for creating and
// positioning the cc::Layer that contains an image of the contents of the
// top-control.
class TopControlsContainerView : public content::WebContentsObserver {
 public:
  TopControlsContainerView(const base::android::JavaParamRef<jobject>&
                               java_top_controls_container_view,
                           ContentViewRenderView* content_view_render_view);
  ~TopControlsContainerView() override;

  // Height needed to display the top-control.
  int GetTopControlsHeight();

  // Creates |top_controls_layer_|.
  void CreateTopControlsLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      int id);

  // Deletes |this|.
  void DeleteTopControlsContainerView(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  // Deletes |top_controls_layer_|.
  void DeleteTopControlsLayer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  // Sets the offsets of the top-controls and content. See ViewAndroidDelegate
  // for details on this.
  void SetTopControlsOffset(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& caller,
                            int top_controls_offset_y,
                            int top_content_offset_y);

  // Sets the size of |top_controls_layer_|.
  void SetTopControlsSize(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& caller,
                          int width,
                          int height);

  // Triggers updating the resource (bitmap) shown in |top_controls_layer_|.
  void UpdateTopControlsResource(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller);

  void SetWebContents(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& caller,
                      const base::android::JavaParamRef<jobject>& web_contents);

 private:
  // WebContentsObserver:
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;

  base::android::ScopedJavaGlobalRef<jobject> java_top_controls_container_view_;
  ContentViewRenderView* content_view_render_view_;
  int top_controls_resource_id_ = -1;

  // Layer containing showing the image for the top-controls. This is a sibling
  // of the WebContents layer.
  scoped_refptr<cc::UIResourceLayer> top_controls_layer_;

  DISALLOW_COPY_AND_ASSIGN(TopControlsContainerView);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TOP_CONTROLS_CONTAINER_VIEW_H_
