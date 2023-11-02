// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_LIST_PROXY_H_
#define WEBLAYER_BROWSER_BROWSER_LIST_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "weblayer/browser/browser_list_observer.h"

namespace weblayer {

// Owns the Java BrowserList implementation and funnels all BrowserListObserver
// calls to the java side.
class BrowserListProxy : public BrowserListObserver {
 public:
  BrowserListProxy();
  BrowserListProxy(const BrowserListProxy&) = delete;
  BrowserListProxy& operator=(const BrowserListProxy&) = delete;
  ~BrowserListProxy() override;

  // BrowserListObserver:
  void OnBrowserCreated(Browser* browser) override;
  void OnBrowserDestroyed(Browser* browser) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_browser_list_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_LIST_PROXY_H_
