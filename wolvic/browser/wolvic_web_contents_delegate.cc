// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/wolvic_web_contents_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/web_contents.h"
#include "wolvic/browser/wolvic_contents.h"
#include "wolvic/jni_headers/WolvicWebContentsDelegate_jni.h"
#include "url/android/gurl_android.h"
#include "wolvic/browser/dialogs/color_chooser_manager.h"
#include "wolvic/browser/dialogs/wolvic_javascript_dialog_manager.h"

namespace wolvic {

WolvicWebContentsDelegate::WolvicWebContentsDelegate(JNIEnv* env, jobject obj)
    : WebContentsDelegateAndroid(env, obj),
      javascript_dialog_manager_(
          std::make_unique<WolvicJavascriptDialogManager>()) {}

WolvicWebContentsDelegate::~WolvicWebContentsDelegate() = default;

using base::android::ScopedJavaLocalRef;

// Called by web_contents_impl.cc whenever a navigation
// requires the creation of a new window (for example
// a link with target=_blank
void WolvicWebContentsDelegate::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  DCHECK(java_delegate.obj());

  // We let new_contents die on purpouse (by not assigning the
  // unique_ptr to any local variable/attribute because that way
  // we are telling the web engine that we do not want a new
  // window to be created. We'll in any case use the target_urlformatter
  // to notify the client (Wolvic) so that it can create the window
  // for the new URL on its own.
  ScopedJavaLocalRef<jobject> java_gurl =
      url::GURLAndroid::FromNativeGURL(env, target_url);
  Java_WolvicWebContentsDelegate_onCreateNewWindow(env, java_delegate, java_gurl);
}

// This adds an extra null check to the parent's behaviour. Seems to be needed
// in some architectures, otherwise it crashes. This started to be observed in
// Meta's Quest3 after the M118 update
void WolvicWebContentsDelegate::LoadingStateChanged(
    content::WebContents* source,
    bool should_show_loading_ui) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  WebContentsDelegateAndroid::LoadingStateChanged(source, should_show_loading_ui);
}

content::JavaScriptDialogManager* WolvicWebContentsDelegate::GetJavaScriptDialogManager(
    content::WebContents* source) {
  return javascript_dialog_manager_.get();
}

std::unique_ptr<content::ColorChooser> WolvicWebContentsDelegate::OpenColorChooser(
    content::WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return std::make_unique<ColorChooserManager>(web_contents, color, suggestions);
}

} // namespace wolvic
