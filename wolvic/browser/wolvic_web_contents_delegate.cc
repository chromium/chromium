// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/wolvic_web_contents_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "wolvic/browser/wolvic_contents.h"
#include "wolvic/jni_headers/WolvicWebContentsDelegate_jni.h"
#include "url/android/gurl_android.h"
#include "wolvic/browser/dialogs/color_chooser_manager.h"
#include "wolvic/browser/dialogs/file_select_manager.h"
#include "wolvic/browser/dialogs/wolvic_javascript_dialog_manager.h"
#include "wolvic/wolvic_permission_manager.h"

namespace wolvic {

WolvicWebContentsDelegate::WolvicWebContentsDelegate(JNIEnv* env, jobject obj)
    : WebContentsDelegateAndroid(env, obj),
      javascript_dialog_manager_(
          std::make_unique<WolvicJavascriptDialogManager>()) {}

WolvicWebContentsDelegate::~WolvicWebContentsDelegate() = default;

using base::android::ScopedJavaLocalRef;

// Called by web_contents_impl.cc whenever a navigation requires the creation
// of a new window (for example a link with target=_blank and window.open)
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
  if (java_delegate.is_null())
    return;

  Java_WolvicWebContentsDelegate_onCreateNewWindow(
      env, java_delegate, new_contents->GetJavaWebContents());

  // |new_contents| ownership has been passed to java, and will retake it
  // in WolvicWebContents when the new tab is created asynchronously.
  new_contents.release();
}

bool WolvicWebContentsDelegate::ShouldResumeRequestsForCreatedWindow() {
  // Always return false here since we need to defer loading the created window
  // until after we have attached a new delegate to the new webcontents (which
  // happens asynchronously).
  return false;
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

void WolvicWebContentsDelegate::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectManager::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

void WolvicWebContentsDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  auto* permission_manager = WolvicPermissionManager::GetInstance(
      web_contents->GetBrowserContext()->IsOffTheRecord());
  permission_manager->RequestMediaAccessPermission(web_contents, request,
                                                   std::move(callback));
}

bool WolvicWebContentsDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  auto* permission_manager = WolvicPermissionManager::GetInstance(
      render_frame_host->GetBrowserContext()->IsOffTheRecord());
  return permission_manager->CheckMediaAccessPermission(render_frame_host,
                                                        security_origin.GetURL(), type);
}

} // namespace wolvic
