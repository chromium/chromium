// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_URL_BAR_URL_BAR_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_URL_BAR_URL_BAR_CONTROLLER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"
#include "weblayer/public/url_bar_controller.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class LocationBarModelImpl;

namespace content {
class WebContents;
}

namespace weblayer {
class BrowserImpl;

class UrlBarControllerImpl : public UrlBarController,
                             public LocationBarModelDelegate {
 public:
  explicit UrlBarControllerImpl(BrowserImpl* native_browser);
  ~UrlBarControllerImpl() override;
  UrlBarControllerImpl(const UrlBarControllerImpl&) = delete;
  UrlBarControllerImpl& operator=(const UrlBarControllerImpl&) = delete;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jstring> GetUrlForDisplay(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetPublisherUrl(JNIEnv* env);
  jint GetConnectionSecurityLevel(JNIEnv* env);
#endif

  // UrlBarController:
  std::u16string GetUrlForDisplay() override;
  security_state::SecurityLevel GetConnectionSecurityLevel() override;

  // LocationBarModelDelegate:
  bool GetURL(GURL* url) const override;
  bool ShouldTrimDisplayUrlAfterHostName() const override;
  std::u16string FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const std::u16string& formatted_url) const override;

 private:
  content::WebContents* GetActiveWebContents() const;
  const raw_ptr<BrowserImpl> browser_;
  std::unique_ptr<LocationBarModelImpl> location_bar_model_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_URL_BAR_URL_BAR_CONTROLLER_IMPL_H_
