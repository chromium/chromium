// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TTS_ENVIRONMENT_ANDROID_IMPL_H_
#define WEBLAYER_BROWSER_TTS_ENVIRONMENT_ANDROID_IMPL_H_

#include "content/public/browser/tts_environment_android.h"

namespace weblayer {

// WebLayer implementation of TtsEnvironmentAndroid. This does not allow
// speech from hidden WebContents.
class TtsEnvironmentAndroidImpl : public content::TtsEnvironmentAndroid {
 public:
  TtsEnvironmentAndroidImpl();
  TtsEnvironmentAndroidImpl(const TtsEnvironmentAndroidImpl&) = delete;
  TtsEnvironmentAndroidImpl& operator=(const TtsEnvironmentAndroidImpl&) =
      delete;
  ~TtsEnvironmentAndroidImpl() override;

  // TtsEnvironment:
  bool CanSpeakUtterancesFromHiddenWebContents() override;
  bool CanSpeakNow() override;
  void SetCanSpeakNowChangedCallback(base::RepeatingClosure callback) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TTS_ENVIRONMENT_ANDROID_IMPL_H_
