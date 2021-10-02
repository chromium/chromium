// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/page_impl.h"

#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "weblayer/browser/navigation_controller_impl.h"
#include "weblayer/browser/tab_impl.h"

#if defined(OS_ANDROID)
#include "weblayer/browser/java/jni/PageImpl_jni.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
#endif

namespace weblayer {
PAGE_USER_DATA_KEY_IMPL(PageImpl);

PageImpl::PageImpl(content::Page& page)
    : content::PageUserData<PageImpl>(page) {}

PageImpl::~PageImpl() {
  auto* rfh = &(page().GetMainDocument());
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  auto* tab = TabImpl::FromWebContents(web_contents);
  if (tab) {
    auto* navigation_controller =
        static_cast<NavigationControllerImpl*>(tab->GetNavigationController());
    navigation_controller->OnPageDestroyed(this);
  }

#if defined(OS_ANDROID)
  if (java_page_) {
    Java_PageImpl_onNativeDestroyed(AttachCurrentThread(), java_page_);
  }
#endif
}

#if defined(OS_ANDROID)
void PageImpl::SetJavaPage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_page) {
  java_page_ = java_page;
}
#endif

}  // namespace weblayer
