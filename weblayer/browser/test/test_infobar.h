// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TEST_TEST_INFOBAR_H_
#define WEBLAYER_BROWSER_TEST_TEST_INFOBAR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/infobar_android.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents.h"

namespace weblayer {

class TestInfoBarDelegate;

// A test infobar.
class TestInfoBar : public infobars::InfoBarAndroid {
 public:
  explicit TestInfoBar(std::unique_ptr<TestInfoBarDelegate> delegate);
  ~TestInfoBar() override;

  TestInfoBar(const TestInfoBar&) = delete;
  TestInfoBar& operator=(const TestInfoBar&) = delete;

  static void Show(content::WebContents* web_contents);

 protected:
  infobars::InfoBarDelegate* GetDelegate();

  // infobars::InfoBarAndroid overrides.
  void ProcessButton(int action) override;
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TEST_TEST_INFOBAR_H_
