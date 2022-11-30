// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/tts_environment_android_impl.h"

#include "base/callback.h"

namespace weblayer {

TtsEnvironmentAndroidImpl::TtsEnvironmentAndroidImpl() = default;

TtsEnvironmentAndroidImpl::~TtsEnvironmentAndroidImpl() = default;

bool TtsEnvironmentAndroidImpl::CanSpeakUtterancesFromHiddenWebContents() {
  // For simplicity's sake, disallow playing utterances in hidden WebContents.
  // Other options are to allow this, and instead cancel any utterances when
  // all browsers are paused.
  return false;
}

bool TtsEnvironmentAndroidImpl::CanSpeakNow() {
  // Always return true, as by the time we get here we know the WebContents
  // is visible (because CanSpeakUtterancesFromHiddenWebContents() returns
  // false). Further, when the fragment is paused/stopped the WebContents is
  // hidden, which triggers the utterance to stop (because
  // CanSpeakUtterancesFromHiddenWebContents() returns false).
  return true;
}

void TtsEnvironmentAndroidImpl::SetCanSpeakNowChangedCallback(
    base::RepeatingClosure callback) {
  // As CanSpeakNow() always returns true, there is nothing to do here.
}

}  // namespace weblayer
