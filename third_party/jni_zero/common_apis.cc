// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/common_apis.h"

#include "third_party/jni_zero/generate_jni/JniUtil_jni.h"
#include "third_party/jni_zero/system_jni/Arrays_jni.h"
#include "third_party/jni_zero/system_jni/Boolean_jni.h"
#include "third_party/jni_zero/system_jni/Collection_jni.h"
#include "third_party/jni_zero/system_jni/Double_jni.h"
#include "third_party/jni_zero/system_jni/Float_jni.h"
#include "third_party/jni_zero/system_jni/Integer_jni.h"
#include "third_party/jni_zero/system_jni/List_jni.h"
#include "third_party/jni_zero/system_jni/Long_jni.h"
#include "third_party/jni_zero/system_jni/Map_jni.h"
#include "third_party/jni_zero/system_jni/Process_jni.h"
#include "third_party/jni_zero/system_jni/Runnable_jni.h"
#include "third_party/jni_zero/system_jni_unchecked_exceptions/ByteBuffer_jni.h"

namespace jni_zero {

ScopedJavaLocalRef<jobjectArray> CollectionToArray(
    JNIEnv* env,
    const JavaRef<jobject>& collection) {
  return JNI_Collection::Java_Collection_toArray(env, collection);
}

ScopedJavaLocalRef<jobject> ArrayToList(JNIEnv* env,
                                        const JavaRef<jobjectArray>& array) {
  return JNI_Arrays::Java_Arrays_asList(env, array);
}

ScopedJavaLocalRef<jobjectArray> MapToArray(JNIEnv* env,
                                            const JavaRef<jobject>& map) {
  return Java_JniUtil_mapToArray(env, map);
}

ScopedJavaLocalRef<jobject> ArrayToMap(JNIEnv* env,
                                       const JavaRef<jobjectArray>& array) {
  return Java_JniUtil_arrayToMap(env, array);
}

//
// java.lang.Runnable
//

void RunRunnable(const JavaRef<>& runnable) {
  JNI_Runnable::Java_Runnable_run(AttachCurrentThread(), runnable);
}

//
// java.util.List
//

ScopedJavaLocalRef<jobject> ListGet(JNIEnv* env,
                                    const JavaRef<jobject>& list,
                                    int32_t idx) {
  return JNI_List::Java_List_get(env, list, idx);
}

ScopedJavaLocalRef<jobject> ListSet(JNIEnv* env,
                                    const JavaRef<jobject>& list,
                                    int32_t idx,
                                    const JavaRef<jobject>& value) {
  return JNI_List::Java_List_set(env, list, idx, value);
}

//
// java.util.Collection
//

bool CollectionAdd(JNIEnv* env,
                   const JavaRef<jobject>& collection,
                   const JavaRef<jobject>& value) {
  return JNI_Collection::Java_Collection_add(env, collection, value);
}

bool CollectionRemove(JNIEnv* env,
                      const JavaRef<jobject>& collection,
                      const JavaRef<jobject>& value) {
  return JNI_Collection::Java_Collection_remove(env, collection, value);
}

void CollectionClear(JNIEnv* env, const JavaRef<jobject>& collection) {
  JNI_Collection::Java_Collection_clear(env, collection);
}

bool CollectionContains(JNIEnv* env,
                        const JavaRef<jobject>& collection,
                        const JavaRef<jobject>& value) {
  return JNI_Collection::Java_Collection_contains(env, collection, value);
}

int32_t CollectionSize(JNIEnv* env, const JavaRef<jobject>& collection) {
  return JNI_Collection::Java_Collection_size(env, collection);
}

//
// java.util.Map
//

ScopedJavaLocalRef<jobject> MapGet(JNIEnv* env,
                                   const JavaRef<jobject>& map,
                                   const JavaRef<jobject>& key) {
  return JNI_Map::Java_Map_get(env, map, key);
}

ScopedJavaLocalRef<jobject> MapPut(JNIEnv* env,
                                   const JavaRef<jobject>& map,
                                   const JavaRef<jobject>& key,
                                   const JavaRef<jobject>& value) {
  return JNI_Map::Java_Map_put(env, map, key, value);
}

bool MapContainsKey(JNIEnv* env,
                    const JavaRef<jobject>& map,
                    const JavaRef<jobject>& key) {
  return JNI_Map::Java_Map_containsKey(env, map, key);
}

ScopedJavaLocalRef<jobject> MapRemove(JNIEnv* env,
                                      const JavaRef<jobject>& map,
                                      const JavaRef<jobject>& key) {
  return JNI_Map::Java_Map_remove(env, map, key);
}

int32_t MapSize(JNIEnv* env, const JavaRef<jobject>& map) {
  return JNI_Map::Java_Map_size(env, map);
}

//
// Boxed types
//

bool FromJavaBoolean(JNIEnv* env, const JavaRef<jobject>& val) {
  return JNI_Boolean::Java_Boolean_booleanValue(env, val);
}

ScopedJavaLocalRef<jobject> ToJavaBoolean(JNIEnv* env, bool val) {
  return JNI_Boolean::Java_Boolean_valueOf__boolean(env, val);
}

int32_t FromJavaInteger(JNIEnv* env, const JavaRef<jobject>& val) {
  return JNI_Integer::Java_Integer_intValue(env, val);
}

ScopedJavaLocalRef<jobject> ToJavaInteger(JNIEnv* env, int32_t val) {
  return JNI_Integer::Java_Integer_valueOf__int(env, val);
}

int64_t FromJavaLong(JNIEnv* env, const JavaRef<jobject>& val) {
  return JNI_Long::Java_Long_longValue(env, val);
}

ScopedJavaLocalRef<jobject> ToJavaLong(JNIEnv* env, int64_t val) {
  return JNI_Long::Java_Long_valueOf__long(env, val);
}

float FromJavaFloat(JNIEnv* env, const JavaRef<jobject>& val) {
  return JNI_Float::Java_Float_floatValue(env, val);
}

ScopedJavaLocalRef<jobject> ToJavaFloat(JNIEnv* env, float val) {
  return JNI_Float::Java_Float_valueOf__float(env, val);
}

double FromJavaDouble(JNIEnv* env, const JavaRef<jobject>& val) {
  return JNI_Double::Java_Double_doubleValue(env, val);
}

ScopedJavaLocalRef<jobject> ToJavaDouble(JNIEnv* env, double val) {
  return JNI_Double::Java_Double_valueOf__double(env, val);
}

//
// android.os.Process
//

bool ProcessIsIsolated(JNIEnv* env) {
  return JNI_Process::Java_Process_isIsolated(env);
}

//
// java.nio.ByteBuffer
//

ScopedJavaLocalRef<jobject> ByteBufferAllocateDirect(JNIEnv* env, int size) {
  ScopedJavaLocalRef<jobject> ret =
      JNI_ByteBuffer::Java_ByteBuffer_allocateDirect(env, size);
  ClearException(env);
  return ret;
}

}  // namespace jni_zero

DEFINE_JNI(JniUtil)
