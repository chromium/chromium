// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
#define WEBLAYER_BROWSER_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_

#include "base/macros.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace weblayer {

class DevToolsManagerDelegateAndroid : public content::DevToolsManagerDelegate {
 public:
  DevToolsManagerDelegateAndroid();
  ~DevToolsManagerDelegateAndroid() override;

  // content::DevToolsManagerDelegate implementation.
  std::string GetDiscoveryPageHTML() override;
  bool IsBrowserTargetDiscoverable() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsManagerDelegateAndroid);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
