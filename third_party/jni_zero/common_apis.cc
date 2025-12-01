// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/common_apis.h"

#include "third_party/jni_zero/generate_jni/JniUtil_jni.h"
#include "third_party/jni_zero/system_jni/Arrays_jni.h"
#include "third_party/jni_zero/system_jni/Boolean_jni.h"
#include "third_party/jni_zero/system_jni/Collection_jni.h"
#include "third_party/jni_zero/system_jni/Integer_jni.h"
#include "third_party/jni_zero/system_jni/List_jni.h"
#include "third_party/jni_zero/system_jni/Long_jni.h"
#include "third_party/jni_zero/system_jni/Map_jni.h"

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

ScopedJavaLocalRef<jobject> ListGet(JNIEnv* env,
                                    const JavaRef<jobject>& list,
                                    jint idx) {
  return JNI_List::Java_List_get(env, list, idx);
}

ScopedJavaLocalRef<jobject> ListSet(JNIEnv* env,
                                    const JavaRef<jobject>& list,
                                    jint idx,
                                    const JavaRef<jobject>& value) {
  return JNI_List::Java_List_set(env, list, idx, value);
}

void ListAdd(JNIEnv* env,
             const JavaRef<jobject>& list,
             const JavaRef<jobject>& value) {
  JNI_List::Java_List_add(env, list, value);
}

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

jint CollectionSize(JNIEnv* env, const JavaRef<jobject>& collection) {
  return JNI_Collection::Java_Collection_size(env, collection);
}

jint MapSize(JNIEnv* env, const JavaRef<jobject>& map) {
  return JNI_Map::Java_Map_size(env, map);
}

bool FromJavaBoolean(JNIEnv* env, const JavaRef<jobject>& j_bool) {
  return static_cast<bool>(JNI_Boolean::Java_Boolean_booleanValue(env, j_bool));
}

ScopedJavaLocalRef<jobject> ToJavaBoolean(JNIEnv* env, bool val) {
  return JNI_Boolean::Java_Boolean_valueOf__boolean(env, val);
}

int32_t FromJavaInteger(JNIEnv* env, const JavaRef<jobject>& j_int) {
  return static_cast<int32_t>(JNI_Integer::Java_Integer_intValue(env, j_int));
}

ScopedJavaLocalRef<jobject> ToJavaInteger(JNIEnv* env, int32_t val) {
  return JNI_Integer::Java_Integer_valueOf__int(env, val);
}

int64_t FromJavaLong(JNIEnv* env, const JavaRef<jobject>& j_long) {
  return static_cast<int64_t>(JNI_Long::Java_Long_longValue(env, j_long));
}

ScopedJavaLocalRef<jobject> ToJavaLong(JNIEnv* env, int64_t val) {
  return JNI_Long::Java_Long_valueOf__long(env, val);
}

}  // namespace jni_zero

DEFINE_JNI(JniUtil)
