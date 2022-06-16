// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_DROP_DATA_ANDROID_H_
#define UI_EVENTS_ANDROID_DROP_DATA_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "ui/events/events_export.h"
#include "url/gurl.h"

namespace ui {

// Generate a Java equivalent DropData object from |drop_data| and used to
// communicate drop data with Java UI. This object can either created from
// native, or from Android UI. In either case, this class will always associate
// to a java object.
class EVENTS_EXPORT DropDataAndroid {
 public:
  DropDataAndroid(JNIEnv* env,
                  const base::android::JavaRef<jobject>& drop_data_android);

  ~DropDataAndroid();

  // Create a new instance of |DropDataAndroid| and its java equivalent.
  static DropDataAndroid Create(const std::u16string& text,
                                const GURL& url,
                                const std::string& file_content,
                                const std::string& image_extension,
                                const std::u16string& image_filename);

  // Get the java equivalent instance of this |DropDataAndroid|.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;

  std::u16string text() const;

 private:
  JNIEnv* env_;
  JavaObjectWeakGlobalRef java_ref_;
};

}  // namespace ui
#endif  // UI_EVENTS_ANDROID_DROP_DATA_ANDROID_H_
