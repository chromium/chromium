// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webapps/webapk_install_scheduler_bridge.h"

#include <jni.h>
#include <string>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"
#include "weblayer/browser/java/jni/WebApkInstallSchedulerBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;
using gfx::ConvertToJavaBitmap;

namespace weblayer {

// static
bool WebApkInstallSchedulerBridge::IsInstallServiceAvailable() {
  return Java_WebApkInstallSchedulerBridge_isInstallServiceAvailable(
      base::android::AttachCurrentThread());
}

WebApkInstallSchedulerBridge::WebApkInstallSchedulerBridge(
    FinishCallback finish_callback) {
  finish_callback_ = std::move(finish_callback);

  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(Java_WebApkInstallSchedulerBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

WebApkInstallSchedulerBridge::~WebApkInstallSchedulerBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkInstallSchedulerBridge_destroy(env, java_ref_);
  java_ref_.Reset();
}

// static
void WebApkInstallSchedulerBridge::ScheduleWebApkInstallWithChrome(
    std::unique_ptr<std::string> serialized_proto,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    FinishCallback finish_callback) {
  // WebApkInstallSchedulerBridge owns itself and deletes itself when finished.
  WebApkInstallSchedulerBridge* bridge =
      new WebApkInstallSchedulerBridge(std::move(finish_callback));
  bridge->ScheduleWebApkInstallWithChrome(
      std::move(serialized_proto), primary_icon, is_primary_icon_maskable);
}

void WebApkInstallSchedulerBridge::ScheduleWebApkInstallWithChrome(
    std::unique_ptr<std::string> serialized_proto,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable) {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jbyteArray> java_serialized_proto =
      ToJavaByteArray(env, *serialized_proto);
  ScopedJavaLocalRef<jobject> java_primary_icon =
      ConvertToJavaBitmap(primary_icon);

  Java_WebApkInstallSchedulerBridge_scheduleInstall(
      env, java_ref_, java_serialized_proto, java_primary_icon,
      is_primary_icon_maskable);
}

void WebApkInstallSchedulerBridge::OnInstallFinished(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint result) {
  std::move(finish_callback_)
      .Run(static_cast<webapps::WebApkInstallResult>(result));

  delete this;
}

}  // namespace weblayer