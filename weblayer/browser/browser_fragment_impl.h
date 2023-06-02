// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_FRAGMENT_IMPL_H_
#define WEBLAYER_BROWSER_BROWSER_FRAGMENT_IMPL_H_

#include "base/android/jni_android.h"

namespace weblayer {

class BrowserFragmentImpl {
 public:
  BrowserFragmentImpl();
  ~BrowserFragmentImpl();
  BrowserFragmentImpl(const BrowserFragmentImpl&) = delete;
  BrowserFragmentImpl& operator=(const BrowserFragmentImpl&) = delete;

  void OnFragmentResume(JNIEnv* env);
  void OnFragmentPause(JNIEnv* env);

  void DeleteBrowserFragment(JNIEnv* env);

  bool fragment_resumed() { return fragment_resumed_; }

 private:
  void UpdateFragmentResumedState(bool state);

  bool fragment_resumed_ = false;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_FRAGMENT_IMPL_H_
