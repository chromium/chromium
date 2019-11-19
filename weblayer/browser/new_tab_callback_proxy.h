// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NEW_TAB_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_NEW_TAB_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "weblayer/public/new_tab_delegate.h"

namespace weblayer {

class TabImpl;

// NewTabCallbackProxy forwards all NewTabDelegate functions to the Java
// side. There is one NewTabCallbackProxy per Tab.
class NewTabCallbackProxy : public NewTabDelegate {
 public:
  NewTabCallbackProxy(JNIEnv* env, jobject obj, TabImpl* tab);
  ~NewTabCallbackProxy() override;

  // NewTabDelegate:
  void OnNewTab(std::unique_ptr<Tab> tab, NewTabType type) override;
  void CloseTab() override;

 private:
  TabImpl* tab_;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;

  DISALLOW_COPY_AND_ASSIGN(NewTabCallbackProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NEW_TAB_CALLBACK_PROXY_H_
