// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/jni_headers/Tab_jni.h"

#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "url/gurl.h"
#include "wolvic/browser/autocomplete/wolvic_autofill_client.h"
#include "wolvic/browser/autocomplete/wolvic_password_manager_client.h"
#include "wolvic/browser/wolvic_contents.h"
#include "wolvic/browser/wolvic_web_contents_delegate.h"
#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_browser_client.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

namespace wolvic {

namespace {

void doPageZoom(const JavaParamRef<jobject>& jweb_contents,
                content::PageZoom zoom) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  zoom::PageZoom::Zoom(web_contents, zoom);
}

}  // namespace

ScopedJavaLocalRef<jobject> JNI_Tab_CreateWebContents(
    JNIEnv* env,
    jboolean is_off_the_record) {
  // TODO(wolvic-chromium#6): Consider handling browser profiles.
  auto* browser_client = WolvicContentBrowserClient::Get();
  CHECK(browser_client->browser_context() != nullptr);

  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(content::WebContents::CreateParams(
          is_off_the_record ? browser_client->off_the_record_browser_context()
                            : browser_client->browser_context()));

  auto* web_contents_impl = web_contents.get();
  auto wolvic_contents =
      std::make_unique<WolvicContents>(std::move(web_contents));
  wolvic_contents.release()->Init();

  zoom::ZoomController::CreateForWebContents(web_contents_impl);

  return web_contents_impl->GetJavaWebContents();
}

void JNI_Tab_AttachWebContents(JNIEnv* env,
                            const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  auto wolvic_contents = std::make_unique<WolvicContents>(
      std::unique_ptr<WebContents>(web_contents));
  wolvic_contents.release()->Init();
  web_contents->ResumeLoadingCreatedWebContents();
}

void JNI_Tab_ReleaseWebContents(JNIEnv* env,
                                const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  auto* wolvic_contents = WolvicContents::FromWebContents(web_contents);
  DCHECK(wolvic_contents);
  delete wolvic_contents;
}

void JNI_Tab_SetWebContentsDelegate(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jweb_contents_delegate) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  auto* wolvic_contents = WolvicContents::FromWebContents(web_contents);
  auto web_contents_delegate =
      std::make_unique<WolvicWebContentsDelegate>(env, jweb_contents_delegate);
  wolvic_contents->SetDelegate(std::move(web_contents_delegate));

  WolvicAutofillClient::CreateForWebContents(web_contents);
  WolvicPasswordManagerClient::CreateForWebContents(web_contents);
}

void JNI_Tab_PageZoomIn(JNIEnv* env,
                        const JavaParamRef<jobject>& jweb_contents) {
  doPageZoom(jweb_contents, content::PAGE_ZOOM_IN);
}

void JNI_Tab_PageZoomOut(JNIEnv* env,
                         const JavaParamRef<jobject>& jweb_contents) {
  doPageZoom(jweb_contents, content::PAGE_ZOOM_OUT);
}

void JNI_Tab_PageZoomReset(JNIEnv* env,
                           const JavaParamRef<jobject>& jweb_contents) {
  doPageZoom(jweb_contents, content::PAGE_ZOOM_RESET);
}

jint JNI_Tab_GetCurrentZoomLevel(JNIEnv* env,
                                 const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  return zoom_controller->GetZoomPercent();
}

}  // namespace wolvic
