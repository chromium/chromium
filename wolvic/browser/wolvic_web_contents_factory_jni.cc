// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/jni_headers/WolvicWebContentsFactory_jni.h"

#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "wolvic/browser/wolvic_contents.h"
#include "wolvic/browser/wolvic_web_contents_delegate.h"
#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_browser_client.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

namespace wolvic {

ScopedJavaLocalRef<jobject> JNI_WolvicWebContentsFactory_CreateWebContents(
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

}  // namespace wolvic
