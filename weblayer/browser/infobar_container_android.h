// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_INFOBAR_CONTAINER_ANDROID_H_
#define WEBLAYER_BROWSER_INFOBAR_CONTAINER_ANDROID_H_

#include <stddef.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "components/infobars/core/infobar_container.h"

namespace weblayer {

class InfoBarContainerAndroid : public infobars::InfoBarContainer {
 public:
  InfoBarContainerAndroid(JNIEnv* env, jobject infobar_container);

  InfoBarContainerAndroid(const InfoBarContainerAndroid&) = delete;
  InfoBarContainerAndroid& operator=(const InfoBarContainerAndroid&) = delete;

  void SetWebContents(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jobject>& web_contents);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  JavaObjectWeakGlobalRef java_container() const {
    return weak_java_infobar_container_;
  }

 private:
  ~InfoBarContainerAndroid() override;

  // InfobarContainer:
  void PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                  size_t position) override;
  void PlatformSpecificRemoveInfoBar(infobars::InfoBar* infobar) override;
  void PlatformSpecificReplaceInfoBar(infobars::InfoBar* old_infobar,
                                      infobars::InfoBar* new_infobar) override;

  // We're owned by the java infobar, need to use a weak ref so it can destroy
  // us.
  JavaObjectWeakGlobalRef weak_java_infobar_container_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_INFOBAR_CONTAINER_ANDROID_H_
