// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/wolvic_web_contents_delegate.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "wolvic/browser/dialogs/color_chooser_manager.h"
#include "wolvic/browser/dialogs/file_select_manager.h"
#include "wolvic/browser/dialogs/wolvic_javascript_dialog_manager.h"
#include "wolvic/browser/wolvic_contents.h"
#include "wolvic/jni_headers/WolvicWebContentsDelegate_jni.h"
#include "wolvic/wolvic_permission_manager.h"

namespace wolvic {

WolvicWebContentsDelegate::WolvicWebContentsDelegate(JNIEnv* env, jobject obj)
    : WebContentsDelegateAndroid(env, obj),
      javascript_dialog_manager_(
          std::make_unique<WolvicJavascriptDialogManager>()) {}

WolvicWebContentsDelegate::~WolvicWebContentsDelegate() = default;

using base::android::ScopedJavaLocalRef;

void WolvicWebContentsDelegate::OnDidGetManifest(
    content::WebContents* web_contents,
    const std::string& raw_manifest) {
  DCHECK(web_contents);
  DCHECK(!raw_manifest.empty());

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_delegate = GetJavaDelegate(env);
  if (java_delegate.is_null()) {
    return;
  }

  Java_WolvicWebContentsDelegate_onWebAppManifest(
      env, java_delegate, web_contents->GetJavaWebContents(),
      base::android::ConvertUTF8ToJavaString(env, raw_manifest));
}

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

void WolvicWebContentsDelegate::FindReply(
    content::WebContents* web_contents,
    int request_id,
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);
  if (!find_result_observations_.IsObservingSource(find_tab_helper))
    find_result_observations_.AddObservation(find_tab_helper);

  find_tab_helper->HandleFindReply(request_id,
                                   number_of_matches,
                                   selection_rect,
                                   active_match_ordinal,
                                   final_update);
}

void WolvicWebContentsDelegate::OnFindResultAvailable(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null()) {
    return;
  }

  const find_in_page::FindNotificationDetails& find_result =
      find_in_page::FindTabHelper::FromWebContents(web_contents)->find_result();

  gfx::Rect rect = find_result.selection_rect();
  ScopedJavaLocalRef<jobject> selection_rect =
      Java_WolvicWebContentsDelegate_createRect(
          env, static_cast<int>(rect.x()), static_cast<int>(rect.y()),
          static_cast<int>(rect.right()), static_cast<int>(rect.bottom()));

  // Create the details object.
  ScopedJavaLocalRef<jobject> details_object =
      Java_WolvicWebContentsDelegate_createFindNotificationDetails(
          env, find_result.number_of_matches(), selection_rect,
          find_result.active_match_ordinal(), find_result.final_update());

  Java_WolvicWebContentsDelegate_onFindResultAvailable(env, obj,
                                                       details_object);
}

void WolvicWebContentsDelegate::OnFindTabHelperDestroyed(
    find_in_page::FindTabHelper* helper) {
  find_result_observations_.RemoveObservation(helper);
}

} // namespace wolvic
