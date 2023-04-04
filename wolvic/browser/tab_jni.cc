// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/jni_headers/Tab_jni.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "wolvic/wolvic_browser_context.h"
#include "wolvic/wolvic_content_main_delegate.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> JNI_Tab_CreateWebContents(
    JNIEnv* env) {
  // TODO(wolvic-chromium#6): Consider handling browser profiles.
  auto* delegate = content::WolvicContentMainDelegate::Get();
  CHECK(delegate->browser_context() != nullptr);

  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(content::WebContents::CreateParams(
          static_cast<BrowserContext*>(delegate->browser_context())));

  // TODO: This is just for the proof-of-concept and URL should be loaded
  // explicitly by `NavigationController.loadUrl` after creating WebContents.
  NavigationController::LoadURLParams params(GURL("https://google.com"));
  params.transition_type = static_cast<ui::PageTransition>(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents->GetController().LoadURLWithParams(params);

  return web_contents.release()->GetJavaWebContents();
}

}  // namespace content
