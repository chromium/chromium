// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PAGE_IMPL_H_
#define WEBLAYER_BROWSER_PAGE_IMPL_H_

#include "build/build_config.h"
#include "content/public/browser/page_user_data.h"
#include "weblayer/public/page.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace weblayer {

class PageImpl : public Page, public content::PageUserData<PageImpl> {
 public:
  PageImpl(const PageImpl&) = delete;
  PageImpl& operator=(const PageImpl&) = delete;

  ~PageImpl() override;

#if BUILDFLAG(IS_ANDROID)
  void SetJavaPage(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& java_page);

  base::android::ScopedJavaGlobalRef<jobject> java_page() { return java_page_; }
#endif

 private:
  explicit PageImpl(content::Page& page);
  friend class content::PageUserData<PageImpl>;
  PAGE_USER_DATA_KEY_DECL();

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_page_;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PAGE_IMPL_H_
