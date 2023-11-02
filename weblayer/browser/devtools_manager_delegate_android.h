// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
#define WEBLAYER_BROWSER_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_

#include "content/public/browser/devtools_manager_delegate.h"

namespace weblayer {

class DevToolsManagerDelegateAndroid : public content::DevToolsManagerDelegate {
 public:
  DevToolsManagerDelegateAndroid();

  DevToolsManagerDelegateAndroid(const DevToolsManagerDelegateAndroid&) =
      delete;
  DevToolsManagerDelegateAndroid& operator=(
      const DevToolsManagerDelegateAndroid&) = delete;

  ~DevToolsManagerDelegateAndroid() override;

  // content::DevToolsManagerDelegate implementation.
  content::BrowserContext* GetDefaultBrowserContext() override;
  std::string GetDiscoveryPageHTML() override;
  bool IsBrowserTargetDiscoverable() override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
