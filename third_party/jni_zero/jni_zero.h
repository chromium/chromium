// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_JNI_ZERO_H_
#define JNI_ZERO_JNI_ZERO_H_

#include <jni.h>

#include <cstdint>

// IWYU pragma: begin_exports
#include "third_party/jni_zero/common_apis.h"
#include "third_party/jni_zero/java_refs.h"
#include "third_party/jni_zero/jni_export.h"
#include "third_party/jni_zero/jni_methods.h"
#include "third_party/jni_zero/jni_raw_ptr.h"
#include "third_party/jni_zero/jni_unique_ptr.h"
#include "third_party/jni_zero/jni_wrappers.h"
#include "third_party/jni_zero/logging.h"
#include "third_party/jni_zero/type_conversions.h"
// IWYU pragma: end_exports

#define DEFINE_JNI(className) DEFINE_JNI_FOR_##className()

namespace jni_zero {

// Called at the JNI boundary right before a borrowed pointer is handed to
// Java, to take a PartitionAlloc BackupRefPtr quarantine reference on it.
using RawPtrWrapFn = uintptr_t (*)(uintptr_t);
// Called from JNI_CommonApis_ReleaseRawPtr() to drop the reference taken by
// RawPtrWrapFn. Must be balanced 1:1 with it.
using RawPtrReleaseFn = void (*)(uintptr_t);

// Installs the hooks used to take/drop a PartitionAlloc BackupRefPtr reference
// when a JniRawPtr crosses the JNI boundary. Must be called exactly once,
// before any JNI call can hand a JniRawPtr to Java; not thread-safe. Changing
// the hooks while borrowed pointers are outstanding unbalances the refcount.
JNI_ZERO_COMPONENT_BUILD_EXPORT void SetRawPtrHooks(RawPtrWrapFn wrap_fn,
                                                    RawPtrReleaseFn release_fn);

// Commonly needed jclasses:
extern JNI_ZERO_COMPONENT_BUILD_EXPORT jclass g_class_loader_class;
extern JNI_ZERO_COMPONENT_BUILD_EXPORT jclass g_object_class;
extern JNI_ZERO_COMPONENT_BUILD_EXPORT jclass g_string_class;
// Singletons for empty things.
extern JNI_ZERO_COMPONENT_BUILD_EXPORT LeakedJavaGlobalRef<jstring>
    g_empty_string;
extern JNI_ZERO_COMPONENT_BUILD_EXPORT LeakedJavaGlobalRef<jobject>
    g_empty_list;
extern JNI_ZERO_COMPONENT_BUILD_EXPORT LeakedJavaGlobalRef<jobject> g_empty_map;

}  // namespace jni_zero

#endif  // JNI_ZERO_JNI_ZERO_H_
