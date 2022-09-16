// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_list_proxy.h"

#include "base/android/jni_android.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/browser_list.h"
#include "weblayer/browser/java/jni/BrowserList_jni.h"

namespace weblayer {

BrowserListProxy::BrowserListProxy()
    : java_browser_list_(Java_BrowserList_createBrowserList(
          base::android::AttachCurrentThread())) {}

BrowserListProxy::~BrowserListProxy() = default;

void BrowserListProxy::OnBrowserCreated(Browser* browser) {
  Java_BrowserList_onBrowserCreated(
      base::android::AttachCurrentThread(), java_browser_list_,
      static_cast<BrowserImpl*>(browser)->java_browser());
}

void BrowserListProxy::OnBrowserDestroyed(Browser* browser) {
  Java_BrowserList_onBrowserDestroyed(
      base::android::AttachCurrentThread(), java_browser_list_,
      static_cast<BrowserImpl*>(browser)->java_browser());
}

static void JNI_BrowserList_CreateBrowserList(JNIEnv* env) {
  BrowserList::GetInstance();
}

}  // namespace weblayer
