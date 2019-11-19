// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FULLSCREEN_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_FULLSCREEN_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "weblayer/public/fullscreen_delegate.h"

namespace weblayer {

class Tab;

// FullscreenCallbackProxy forwards all FullscreenDelegate functions to the
// Java side. There is at most one FullscreenCallbackProxy per
// Tab.
class FullscreenCallbackProxy : public FullscreenDelegate {
 public:
  FullscreenCallbackProxy(JNIEnv* env, jobject obj, Tab* tab);
  ~FullscreenCallbackProxy() override;

  // FullscreenDelegate:
  void EnterFullscreen(base::OnceClosure exit_closure) override;
  void ExitFullscreen() override;

  // Called from the Java side to exit fullscreen.
  void DoExitFullscreen(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& caller);

 private:
  Tab* tab_;
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  base::OnceClosure exit_fullscreen_closure_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenCallbackProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FULLSCREEN_CALLBACK_PROXY_H_
