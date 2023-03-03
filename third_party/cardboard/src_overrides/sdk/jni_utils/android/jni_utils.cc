/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "third_party/cardboard/src/sdk/jni_utils/android/jni_utils.h"

#include "base/android/jni_android.h"
#include "third_party/cardboard/src/sdk/util/logging.h"

namespace cardboard::jni {
namespace {

jclass runtime_excepton_class_;

void LoadJNIResources(JNIEnv* env) {
  runtime_excepton_class_ =
      cardboard::jni::LoadJClass(env, "java/lang/RuntimeException");
}

}  // anonymous namespace

void initializeAndroid(JavaVM* vm, jobject /*context*/) {
  JNIEnv* env;
  LoadJNIEnv(vm, &env);
  LoadJNIResources(env);
}

bool CheckExceptionInJava(JNIEnv* env) {
  const bool exception_occurred = env->ExceptionOccurred();
  if (exception_occurred) {
    env->ExceptionDescribe();
    env->ExceptionClear();
  }
  return exception_occurred;
}

void LoadJNIEnv(JavaVM* vm, JNIEnv** env) {
  switch (vm->GetEnv(reinterpret_cast<void**>(env), JNI_VERSION_1_6)) {
    case JNI_OK:
      break;
    case JNI_EDETACHED:
      if (vm->AttachCurrentThread(env, nullptr) != 0) {
        *env = nullptr;
      }
      break;
    default:
      *env = nullptr;
      break;
  }
}

jclass LoadJClass(JNIEnv* env, const char* class_name) {
  auto local_ref = base::android::GetClass(env, class_name);
  CheckExceptionInJava(env);
  return static_cast<jclass>(env->NewGlobalRef(local_ref.obj()));
}

void ThrowJavaRuntimeException(JNIEnv* env, const char* msg) {
  CARDBOARD_LOGE("Throw Java RuntimeException: %s", msg);
  env->ThrowNew(runtime_excepton_class_, msg);
}

}  // namespace cardboard::jni
