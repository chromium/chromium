// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_fragment_impl.h"

#include "weblayer/browser/browser_fragment_list.h"
#include "weblayer/browser/java/jni/BrowserFragmentImpl_jni.h"

namespace weblayer {

BrowserFragmentImpl::BrowserFragmentImpl() {
  BrowserFragmentList::GetInstance()->AddBrowserFragment(this);
}

BrowserFragmentImpl::~BrowserFragmentImpl() {
  BrowserFragmentList::GetInstance()->RemoveBrowserFragment(this);
}

void BrowserFragmentImpl::OnFragmentResume(JNIEnv* env) {
  UpdateFragmentResumedState(true);
}

void BrowserFragmentImpl::OnFragmentPause(JNIEnv* env) {
  UpdateFragmentResumedState(false);
}

void BrowserFragmentImpl::DeleteBrowserFragment(JNIEnv* env) {
  delete this;
}

void BrowserFragmentImpl::UpdateFragmentResumedState(bool state) {
  const bool old_has_at_least_one_active_browser =
      BrowserFragmentList::GetInstance()->HasAtLeastOneResumedBrowser();
  fragment_resumed_ = state;
  if (old_has_at_least_one_active_browser !=
      BrowserFragmentList::GetInstance()->HasAtLeastOneResumedBrowser()) {
    BrowserFragmentList::GetInstance()
        ->NotifyHasAtLeastOneResumedBrowserFragmentChanged();
  }
}

static jlong JNI_BrowserFragmentImpl_CreateBrowserFragment(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new BrowserFragmentImpl());
}

}  // namespace weblayer
