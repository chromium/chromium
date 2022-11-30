// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FULLSCREEN_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_FULLSCREEN_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "weblayer/public/fullscreen_delegate.h"

namespace weblayer {

class Tab;

// FullscreenCallbackProxy forwards all FullscreenDelegate functions to the
// Java side. There is at most one FullscreenCallbackProxy per
// Tab.
class FullscreenCallbackProxy : public FullscreenDelegate {
 public:
  FullscreenCallbackProxy(JNIEnv* env, jobject obj, Tab* tab);

  FullscreenCallbackProxy(const FullscreenCallbackProxy&) = delete;
  FullscreenCallbackProxy& operator=(const FullscreenCallbackProxy&) = delete;

  ~FullscreenCallbackProxy() override;

  // FullscreenDelegate:
  void EnterFullscreen(base::OnceClosure exit_closure) override;
  void ExitFullscreen() override;

  // Called from the Java side to exit fullscreen.
  void DoExitFullscreen(JNIEnv* env);

 private:
  raw_ptr<Tab> tab_;
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  base::OnceClosure exit_fullscreen_closure_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FULLSCREEN_CALLBACK_PROXY_H_
