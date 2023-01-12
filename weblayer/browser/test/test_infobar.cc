// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/test/test_infobar.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar_delegate.h"
#include "weblayer/browser/java/test_jni/TestInfoBar_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

class TestInfoBarDelegate : public infobars::InfoBarDelegate {
 public:
  TestInfoBarDelegate() = default;

  ~TestInfoBarDelegate() override = default;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return InfoBarDelegate::InfoBarIdentifier::TEST_INFOBAR;
  }
};

TestInfoBar::TestInfoBar(std::unique_ptr<TestInfoBarDelegate> delegate)
    : infobars::InfoBarAndroid(std::move(delegate)) {}

TestInfoBar::~TestInfoBar() = default;

void TestInfoBar::ProcessButton(int action) {}

ScopedJavaLocalRef<jobject> TestInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  return Java_TestInfoBar_create(env);
}

// static
void TestInfoBar::Show(content::WebContents* web_contents) {
  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  manager->AddInfoBar(
      std::make_unique<TestInfoBar>(std::make_unique<TestInfoBarDelegate>()));
}

static void JNI_TestInfoBar_Show(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  auto* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  TestInfoBar::Show(web_contents);
}

}  // namespace weblayer
