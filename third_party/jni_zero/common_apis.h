// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_COMMON_APIS_H_
#define JNI_ZERO_COMMON_APIS_H_

#include <jni.h>
#include <stdint.h>

#include "third_party/jni_zero/java_refs.h"
#include "third_party/jni_zero/jni_export.h"
#include "third_party/jni_zero/type_conversions.h"

namespace jni_zero {
// Wraps Collection.toArray().
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobjectArray>
CollectionToArray(JNIEnv* env, const JavaRef<jobject>& collection);
// Wraps Arrays.asList().
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ArrayToList(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array);
// Serializes a Map to an array of key/values.
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobjectArray> MapToArray(
    JNIEnv* env,
    const JavaRef<jobject>& map);
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ArrayToMap(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array);

//
// java.lang.Runnable
//

// Does not accept "env" to allow it to be used with base::Bind(), and do not
// offer such an overload to not make &RunRunnable hard to work with.
JNI_ZERO_COMPONENT_BUILD_EXPORT void RunRunnable(const JavaRef<>& runnable);

//
// java.util.List
//

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject>
ListGet(JNIEnv* env, const JavaRef<jobject>& list, int32_t idx);

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ListSet(
    JNIEnv* env,
    const JavaRef<jobject>& list,
    int32_t idx,
    const JavaRef<jobject>& value);
// Helper that calls ToJniType on the value before calling ListSet.
template <typename V>
  requires(!IsJavaRef<V>)
inline ScopedJavaLocalRef<jobject> ListSet(JNIEnv* env,
                                           const JavaRef<jobject>& list,
                                           int32_t idx,
                                           const V& value) {
  return ListSet(env, list, idx, ToJniType(env, value));
}

//
// java.util.Collection
//
JNI_ZERO_COMPONENT_BUILD_EXPORT bool CollectionAdd(
    JNIEnv* env,
    const JavaRef<jobject>& collection,
    const JavaRef<jobject>& value);
// Helper that calls ToJniType on the value before calling CollectionAdd.
template <typename V>
  requires(!IsJavaRef<V>)
inline ScopedJavaLocalRef<jobject>
CollectionAdd(JNIEnv* env, const JavaRef<jobject>& collection, const V& value) {
  return CollectionAdd(env, collection, ToJniType(env, value));
}

JNI_ZERO_COMPONENT_BUILD_EXPORT bool CollectionRemove(
    JNIEnv* env,
    const JavaRef<jobject>& collection,
    const JavaRef<jobject>& value);

JNI_ZERO_COMPONENT_BUILD_EXPORT void CollectionClear(
    JNIEnv* env,
    const JavaRef<jobject>& collection);

JNI_ZERO_COMPONENT_BUILD_EXPORT bool CollectionContains(
    JNIEnv* env,
    const JavaRef<jobject>& collection,
    const JavaRef<jobject>& value);

JNI_ZERO_COMPONENT_BUILD_EXPORT int32_t
CollectionSize(JNIEnv* env, const JavaRef<jobject>& collection);

//
// java.util.Map
//

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject>
MapGet(JNIEnv* env, const JavaRef<jobject>& map, const JavaRef<jobject>& key);

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> MapPut(
    JNIEnv* env,
    const JavaRef<jobject>& map,
    const JavaRef<jobject>& key,
    const JavaRef<jobject>& value);

// Helper that calls ToJniType on the key and value before calling MapPut.
template <typename K, typename V>
  requires(!IsJavaRef<K> || !IsJavaRef<V>)
inline ScopedJavaLocalRef<jobject> MapPut(JNIEnv* env,
                                          const JavaRef<jobject>& map,
                                          const K& key,
                                          const V& value) {
  return MapPut(env, map, ToJniType(env, key), ToJniType(env, value));
}

JNI_ZERO_COMPONENT_BUILD_EXPORT bool MapContainsKey(
    JNIEnv* env,
    const JavaRef<jobject>& map,
    const JavaRef<jobject>& key);

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> MapRemove(
    JNIEnv* env,
    const JavaRef<jobject>& map,
    const JavaRef<jobject>& key);

JNI_ZERO_COMPONENT_BUILD_EXPORT int32_t MapSize(JNIEnv* env,
                                                const JavaRef<jobject>& map);
//
// Boxed types
//
JNI_ZERO_COMPONENT_BUILD_EXPORT bool FromJavaBoolean(
    JNIEnv* env,
    const JavaRef<jobject>& val);

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ToJavaBoolean(
    JNIEnv* env,
    bool val);

JNI_ZERO_COMPONENT_BUILD_EXPORT int32_t
FromJavaInteger(JNIEnv* env, const JavaRef<jobject>& val);

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ToJavaInteger(
    JNIEnv* env,
    int32_t val);

JNI_ZERO_COMPONENT_BUILD_EXPORT int64_t
FromJavaLong(JNIEnv* env, const JavaRef<jobject>& val);

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ToJavaLong(
    JNIEnv* env,
    int64_t val);

JNI_ZERO_COMPONENT_BUILD_EXPORT float FromJavaFloat(
    JNIEnv* env,
    const JavaRef<jobject>& val);

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ToJavaFloat(
    JNIEnv* env,
    float val);

JNI_ZERO_COMPONENT_BUILD_EXPORT double FromJavaDouble(
    JNIEnv* env,
    const JavaRef<jobject>& val);

JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ToJavaDouble(
    JNIEnv* env,
    double val);

//
// android.os.Process
//

JNI_ZERO_COMPONENT_BUILD_EXPORT bool ProcessIsIsolated(JNIEnv* env);

//
// java.nio.ByteBuffer
//

// This returns nullptr in the case of an exception.
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject>
ByteBufferAllocateDirect(JNIEnv* env, int size);

}  // namespace jni_zero

#endif  // JNI_ZERO_COMMON_APIS_H_
