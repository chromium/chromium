// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/dialogs/color_chooser_manager.h"

#include <stddef.h>

#include "content/public/browser/web_contents.h"
#include "wolvic/jni_headers/ColorChooserManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

namespace wolvic {

ColorChooserManager::ColorChooserManager(
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
    : web_contents_(web_contents) {
  JNIEnv* env = AttachCurrentThread();
  if (web_contents->IsBeingDestroyed()) {
    OnColorChosen(env, j_color_chooser_, initial_color);
    return;
  }

  // Not implemeted for |suggestions| yet.

  j_color_chooser_.Reset(Java_ColorChooserManager_createColorChooser(
        env, reinterpret_cast<intptr_t>(this), initial_color));
}

ColorChooserManager::~ColorChooserManager() {}

void ColorChooserManager::End() {
  if (!j_color_chooser_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    Java_ColorChooserManager_closeColorChooser(env, j_color_chooser_);
  }
}

void ColorChooserManager::SetSelectedColor(SkColor color) {
  // Not implemented since the color is set on the wolvic side only, in theory
  // it can be set from JS which would override the user selection but
  // we don't support that for now.
}

void ColorChooserManager::OnColorChosen(JNIEnv* env,
                                        const JavaRef<jobject>& obj,
                                        jint color) {
  web_contents_->DidChooseColorInColorChooser(static_cast<SkColor>(color));
  web_contents_->DidEndColorChooser();
}

}  // namespace wolvic
