// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_DIALOGS_COLOR_CHOOSER_MANAGER_H_
#define WOLVIC_BROWSER_DIALOGS_COLOR_CHOOSER_MANAGER_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/color_chooser.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"

namespace content {
class WebContents;
}

namespace wolvic {

class ColorChooserManager : public content::ColorChooser {
 public:
  ColorChooserManager(
      content::WebContents* tab,
      SkColor initial_color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);

  ColorChooserManager(const ColorChooserManager&) = delete;
  ColorChooserManager& operator=(const ColorChooserManager&) = delete;

  ~ColorChooserManager() override;

  void OnColorChosen(JNIEnv* env,
                     const base::android::JavaRef<jobject>& obj,
                     jint color);

  // ColorChooser interface
  void End() override;
  void SetSelectedColor(SkColor color) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_color_chooser_;

  // The web contents invoking the color chooser.  No ownership. because it will
  // outlive this class.
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_DIALOGS_COLOR_CHOOSER_MANAGER_H_
