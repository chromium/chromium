// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/jni_zero.h"

// This is called by the VM when the shared library is first loaded.
__attribute__((visibility("default"))) jint JNI_OnLoad(JavaVM* vm,
                                                       void* reserved) {
  jni_zero::InitVM(vm);

  return JNI_VERSION_1_4;
}
