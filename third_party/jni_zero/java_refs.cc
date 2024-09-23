// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/java_refs.h"

#include "third_party/jni_zero/jni_methods.h"

namespace jni_zero {
namespace {
const int kDefaultLocalFrameCapacity = 16;
}

ScopedJavaLocalFrame::ScopedJavaLocalFrame(JNIEnv* env) : env_(env) {
  int failed = env_->PushLocalFrame(kDefaultLocalFrameCapacity);
  JNI_ZERO_DCHECK(!failed);
}

ScopedJavaLocalFrame::ScopedJavaLocalFrame(JNIEnv* env, int capacity)
    : env_(env) {
  int failed = env_->PushLocalFrame(capacity);
  JNI_ZERO_DCHECK(!failed);
}

ScopedJavaLocalFrame::~ScopedJavaLocalFrame() {
  env_->PopLocalFrame(nullptr);
}

#if JNI_ZERO_DCHECK_IS_ON()
// This constructor is inlined when DCHECKs are disabled; don't add anything
// else here.
JavaRef<jobject>::JavaRef(JNIEnv* env, jobject obj) : obj_(obj) {
  if (obj) {
    JNI_ZERO_DCHECK(env && env->GetObjectRefType(obj) == JNILocalRefType);
  }
}
#endif

JNIEnv* JavaRef<jobject>::SetNewLocalRef(JNIEnv* env, jobject obj) {
  if (!env) {
    env = AttachCurrentThread();
  } else {
    JNI_ZERO_DCHECK(env ==
                    AttachCurrentThread());  // Is |env| on correct thread.
  }
  if (obj) {
    obj = env->NewLocalRef(obj);
  }
  if (obj_) {
    env->DeleteLocalRef(obj_);
  }
  obj_ = obj;
  return env;
}

void JavaRef<jobject>::SetNewGlobalRef(JNIEnv* env, jobject obj) {
  if (!env) {
    env = AttachCurrentThread();
  } else {
    JNI_ZERO_DCHECK(env ==
                    AttachCurrentThread());  // Is |env| on correct thread.
  }
  if (obj) {
    obj = env->NewGlobalRef(obj);
  }
  if (obj_) {
    env->DeleteGlobalRef(obj_);
  }
  obj_ = obj;
}

void JavaRef<jobject>::ResetLocalRef(JNIEnv* env) {
  if (obj_) {
    JNI_ZERO_DCHECK(env ==
                    AttachCurrentThread());  // Is |env| on correct thread.
    env->DeleteLocalRef(obj_);
    obj_ = nullptr;
  }
}

void JavaRef<jobject>::ResetGlobalRef() {
  if (obj_) {
    AttachCurrentThread()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
}

ScopedJavaGlobalWeakRef::ScopedJavaGlobalWeakRef(
    const ScopedJavaGlobalWeakRef& orig) {
  Assign(orig);
}

ScopedJavaGlobalWeakRef::ScopedJavaGlobalWeakRef(JNIEnv* env,
                                                 const JavaRef<jobject>& obj)
    : obj_(env->NewWeakGlobalRef(obj.obj())) {}

void ScopedJavaGlobalWeakRef::reset() {
  if (obj_) {
    AttachCurrentThread()->DeleteWeakGlobalRef(obj_);
    obj_ = nullptr;
  }
}

ScopedJavaLocalRef<jobject> ScopedJavaGlobalWeakRef::get(JNIEnv* env) const {
  jobject real = obj_ ? real = env->NewLocalRef(obj_) : nullptr;
  return ScopedJavaLocalRef<jobject>(env, real);
}

void ScopedJavaGlobalWeakRef::Assign(const ScopedJavaGlobalWeakRef& other) {
  if (&other == this) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  if (obj_) {
    env->DeleteWeakGlobalRef(obj_);
  }

  obj_ = other.obj_ ? env->NewWeakGlobalRef(other.obj_) : nullptr;
}

}  // namespace jni_zero
