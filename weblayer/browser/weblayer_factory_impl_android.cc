// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/weblayer_factory_impl_android.h"

#include "weblayer/browser/java/jni/WebLayerFactoryImpl_jni.h"

namespace weblayer {

int WebLayerFactoryImplAndroid::GetClientMajorVersion() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebLayerFactoryImpl_getClientMajorVersion(env);
}

}  // namespace weblayer
