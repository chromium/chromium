// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "weblayer/app/content_main_delegate_impl.h"

namespace weblayer {

class MainDelegateImpl : public MainDelegate {
 public:
  void PreMainMessageLoopRun() override {}
  void PostMainMessageLoopRun() override {}
  void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) override {}
};

// This is called by the VM when the shared library is first loaded.
bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;

  weblayer::MainParams params;
  params.delegate = new weblayer::MainDelegateImpl;

  content::SetContentMainDelegate(
      new weblayer::ContentMainDelegateImpl(params));
  return true;
}

}  // namespace weblayer
