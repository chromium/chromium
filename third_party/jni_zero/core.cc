// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/core.h"

#include <sys/prctl.h>

#include "third_party/jni_zero/logging.h"

namespace jni_zero {
namespace {
// Until we fully migrate base's jni_android, we will maintain a copy of this
// global here and will have base set this variable when it sets its own.
JavaVM* g_jvm = nullptr;

void (*g_exception_handler_callback)(JNIEnv*) = nullptr;
}  // namespace

JNIEnv* AttachCurrentThread() {
  JNI_ZERO_DCHECK(g_jvm);
  JNIEnv* env = nullptr;
  jint ret = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_2);
  if (ret == JNI_EDETACHED || !env) {
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_2;
    args.group = nullptr;

    // 16 is the maximum size for thread names on Android.
    char thread_name[16];
    int err = prctl(PR_GET_NAME, thread_name);
    if (err < 0) {
      JNI_ZERO_ELOG("prctl(PR_GET_NAME)");
      args.name = nullptr;
    } else {
      args.name = thread_name;
    }

#if defined(JNI_ZERO_IS_ROBOLECTRIC)
    ret = g_jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), &args);
#else
    ret = g_jvm->AttachCurrentThread(&env, &args);
#endif
    JNI_ZERO_CHECK(ret == JNI_OK);
  }
  return env;
}

JNIEnv* AttachCurrentThreadWithName(const std::string& thread_name) {
  JNI_ZERO_DCHECK(g_jvm);
  JavaVMAttachArgs args;
  args.version = JNI_VERSION_1_2;
  args.name = const_cast<char*>(thread_name.c_str());
  args.group = nullptr;
  JNIEnv* env = nullptr;
#if defined(JNI_ZERO_IS_ROBOLECTRIC)
  jint ret = g_jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), &args);
#else
  jint ret = g_jvm->AttachCurrentThread(&env, &args);
#endif
  JNI_ZERO_CHECK(ret == JNI_OK);
  return env;
}

void DetachFromVM() {
  // Ignore the return value, if the thread is not attached, DetachCurrentThread
  // will fail. But it is ok as the native thread may never be attached.
  if (g_jvm) {
    g_jvm->DetachCurrentThread();
  }
}

void InitVM(JavaVM* vm) {
  g_jvm = vm;
}

void DisableJvmForTesting() {
  g_jvm = nullptr;
}

bool IsVMInitialized() {
  return g_jvm != nullptr;
}

JavaVM* GetVM() {
  return g_jvm;
}

bool HasException(JNIEnv* env) {
  return env->ExceptionCheck() != JNI_FALSE;
}

bool ClearException(JNIEnv* env) {
  if (!HasException(env)) {
    return false;
  }
  env->ExceptionDescribe();
  env->ExceptionClear();
  return true;
}

void SetExceptionHandler(void (*callback)(JNIEnv*)) {
  g_exception_handler_callback = callback;
}

void CheckException(JNIEnv* env) {
  if (!HasException(env)) {
    return;
  }

  if (g_exception_handler_callback) {
    return g_exception_handler_callback(env);
  }
  JNI_ZERO_FLOG("jni_zero crashing due to uncaught Java exception");
}

}  // namespace jni_zero
