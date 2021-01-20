// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TAB_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_TAB_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "weblayer/public/tab_observer.h"

namespace weblayer {

class Tab;

// TabCallbackProxy forwards all TabObserver functions to the Java side. There
// is one TabCallbackProxy per Tab.
class TabCallbackProxy : public TabObserver {
 public:
  TabCallbackProxy(JNIEnv* env, jobject obj, Tab* tab);
  ~TabCallbackProxy() override;

  // TabObserver:
  void DisplayedUrlChanged(const GURL& url) override;
  void OnRenderProcessGone() override;
  void OnTitleUpdated(const base::string16& title) override;

 private:
  Tab* tab_;
  base::android::ScopedJavaGlobalRef<jobject> java_observer_;

  DISALLOW_COPY_AND_ASSIGN(TabCallbackProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TAB_CALLBACK_PROXY_H_
