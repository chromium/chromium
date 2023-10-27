// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/jni_headers/Tab_jni.h"

#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "wolvic/browser/wolvic_contents.h"
#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_browser_client.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;
using web_contents_delegate_android::WebContentsDelegateAndroid;

namespace wolvic {

ScopedJavaLocalRef<jobject> JNI_Tab_CreateWebContents(
    JNIEnv* env,
    jboolean is_off_the_record) {
  // TODO(wolvic-chromium#6): Consider handling browser profiles.
  auto* browser_client = content::WolvicContentBrowserClient::Get();
  CHECK(browser_client->browser_context() != nullptr);

  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(content::WebContents::CreateParams(
          is_off_the_record ? browser_client->off_the_record_browser_context()
                            : browser_client->browser_context()));

  auto wolvic_contents = std::make_unique<WolvicContents>(web_contents.get());
  wolvic_contents.release()->Init();

  return web_contents.release()->GetJavaWebContents();
}

void JNI_Tab_SetWebContentsDelegate(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jweb_contents_delegate) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  static std::unique_ptr<WebContentsDelegateAndroid> web_contents_delegate;

  DCHECK(web_contents);

  web_contents_delegate =
      std::make_unique<WebContentsDelegateAndroid>(env, jweb_contents_delegate);
  web_contents->SetDelegate(web_contents_delegate.get());
}

}  // namespace wolvic
