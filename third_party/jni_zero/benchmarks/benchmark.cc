// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/benchmarks/benchmark.h"

#include <stdint.h>

#include <atomic>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"
#include "third_party/jni_zero/benchmarks/benchmark_jni/Benchmark_jni.h"
#include "third_party/jni_zero/benchmarks/system_jni/Integer_jni.h"
#include "third_party/jni_zero/jni_zero.h"

using benchmark::DoNotOptimize;
using jni_zero::JavaParamRef;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

template <>
std::int32_t jni_zero::FromJniType<std::int32_t>(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_integer) {
  return static_cast<std::int32_t>(
      JNI_Integer::Java_Integer_intValue(env, j_integer));
}
template <>
jni_zero::ScopedJavaLocalRef<jobject> jni_zero::ToJniType<std::int32_t>(
    JNIEnv* env,
    const std::int32_t& input) {
  return JNI_Integer::Java_Integer_valueOf__int(env, static_cast<jint>(input));
}

const int kUsToNs = 1000;

namespace jni_zero::benchmark {

jclass LookupClass(JNIEnv* env, const char* class_name) {
  std::atomic<jclass> cached_class;
  return jni_zero::internal::LazyGetClass(env, class_name, &cached_class);
}

jmethodID LookupMethod(JNIEnv* env,
                       jclass clazz,
                       const char* method_name,
                       const char* method_signature) {
  std::atomic<jmethodID> cached_method_id;
  jni_zero::internal::JniJavaCallContext<true> call_context;
  call_context.Init<jni_zero::MethodID::TYPE_STATIC>(
      env, clazz, method_name, method_signature, &cached_method_id);
  return call_context.method_id();
}

static std::string JNI_Benchmark_RunLookupBenchmark(JNIEnv* env) {
  std::stringstream benchmark_log;
  const int kNumTries = 10000;
  base::ElapsedTimer timer;
  for (int i = 0; i < kNumTries; i++) {
    Java_Benchmark_callMe(env);
  }
  double delta = timer.Elapsed().InMicrosecondsF();

  benchmark_log << "Called java method [Native -> Java] (" << kNumTries
                << " times): Elapsed time = " << delta << " us\n";
  double average_us = delta / kNumTries;
  benchmark_log << "Average per method call = " << average_us * kUsToNs
                << " ns\n";
  return benchmark_log.str();
}

static std::string JNI_Benchmark_RunGeneratedClassesBenchmark(JNIEnv* env) {
  std::stringstream benchmark_log;
  static constexpr std::string_view kClassPrefix =
      "org/jni_zero/benchmark/Placeholder";
  static const char kMethodName[] = "callMe";
  static const char kMethodSignature[] = "()V";
  const int kNumClasses = 1000;
  std::string class_names[kNumClasses];
  for (int i = 1; i <= kNumClasses; i++) {
    class_names[i] = base::StrCat({kClassPrefix, base::NumberToString(i)});
  }

  jclass clazzes[kNumClasses];
  base::ElapsedTimer timer;
  for (int i = 1; i <= kNumClasses; i++) {
    clazzes[i] = LookupClass(env, class_names[i].data());
  }
  double elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Found different clazz (" << kNumClasses
                << " times): Elapsed time = " << elapsed_us << " us\n";

  elapsed_us = 0;
  timer = {};
  for (int i = 1; i <= kNumClasses; i++) {
    LookupMethod(env, clazzes[i], kMethodName, kMethodSignature);
  }
  elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Found different method (" << kNumClasses
                << " times): Elapsed time = " << elapsed_us << " us\n";
  return benchmark_log.str();
}

static std::string JNI_Benchmark_RunNativeToJavaParamSizesBenchmark(
    JNIEnv* env) {
  std::stringstream benchmark_log;
  const int kArraySize = 10000;
  const int kNumTries = 1000;
  std::vector<int> array;
  array.resize(kArraySize);
  for (int i = 0; i < kArraySize; i++) {
    array[i] = i;
  }

  base::ElapsedTimer timer;
  for (int tries = 0; tries < kNumTries; tries++) {
    Java_Benchmark_receiveLargeIntArray(env, array);
  }
  double elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Sending " << kArraySize
                << " int vector with conversion [Native -> Java] (" << kNumTries
                << " times): Elapsed time = " << elapsed_us << " us\n";
  double average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per " << kArraySize
                << " int vector = " << average_us * kUsToNs << " ns\n";

  elapsed_us = 0;
  timer = {};
  for (int tries = 0; tries < kNumTries; tries++) {
    for (int i = 0; i < kArraySize; i++) {
      Java_Benchmark_receiveSingleInt(env, i);
    }
  }
  elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Sending " << kArraySize
                << " ints one at a time [Native -> Java] (" << kNumTries
                << " times): Elapsed time = " << elapsed_us << " us\n";
  average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per " << kArraySize
                << " ints = " << average_us * kUsToNs << " ns\n";

  elapsed_us = 0;
  timer = {};
  for (int tries = 0; tries < kNumTries; tries++) {
    for (int i = 0; i < kArraySize; i++) {
      Java_Benchmark_receiveSingleInteger(env, i);
    }
  }
  elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Sending " << kArraySize
                << " Integers with conversion one at a time [Native -> Java] ("
                << kNumTries << " times): Elapsed time = " << elapsed_us
                << " us\n";
  average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per " << kArraySize
                << " Integers = " << average_us * kUsToNs << " ns\n";
  return benchmark_log.str();
}

static std::string JNI_Benchmark_RunAttachCurrentThreadBenchmark(
    JNIEnv* unused) {
  std::stringstream benchmark_log;
  const int kNumTries = 10000;
  base::ElapsedTimer timer;
  for (int tries = 0; tries < kNumTries; tries++) {
    AttachCurrentThread();
  }
  double elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Calling AttachCurrentThread (" << kNumTries
                << " times): Elapsed time = " << elapsed_us << " us\n";
  double average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per call = " << average_us * kUsToNs << " ns\n";
  return benchmark_log.str();
}

static std::string JNI_Benchmark_RunIntegerBoxingBenchmark(JNIEnv* env) {
  std::stringstream benchmark_log;
  const int kNumTries = 10000;
  ScopedJavaLocalRef<jobject> j_integerArray[kNumTries];
  base::ElapsedTimer timer;
  for (int i = 0; i < kNumTries; i++) {
    j_integerArray[i] = JNI_Integer::Java_Integer_valueOf__int(env, i);
  }
  double elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Calling Integer.valueOf (" << kNumTries
                << " times): Elapsed time = " << elapsed_us << " us\n";
  double average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per call = " << average_us * kUsToNs << " ns\n";

  elapsed_us = 0;
  timer = {};
  for (int i = 0; i < kNumTries; i++) {
    JNI_Integer::Java_Integer_intValue(env, j_integerArray[i]);
  }
  elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Calling Integer.intValue (" << kNumTries
                << " times): Elapsed time = " << elapsed_us << " us\n";
  average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per call = " << average_us * kUsToNs << " ns\n";
  return benchmark_log.str();
}

static std::string JNI_Benchmark_RunNativeToJavaMultipleParamsBenchmark(
    JNIEnv* env) {
  std::stringstream benchmark_log;
  const int kNumTries = 10000;
  base::ElapsedTimer timer;
  for (int i = 0; i < kNumTries; i++) {
    Java_Benchmark_receive10Ints(env, i, i, i, i, i, i, i, i, i, i);
  }
  double elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Sending 10 ints [Native -> Java] (" << kNumTries
                << " times): Elapsed time = " << elapsed_us << " us\n";
  double average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per 10 ints = " << average_us * kUsToNs << " ns\n";

  elapsed_us = 0;
  timer = {};
  for (int i = 0; i < kNumTries; i++) {
    Java_Benchmark_receive10IntegersConverted(env, i, i, i, i, i, i, i, i, i,
                                              i);
  }
  elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log
      << "Sending 10 Integers converted with @JniType [Native -> Java] ("
      << kNumTries << " times): Elapsed time = " << elapsed_us << " us\n";
  average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per 10 Integers = " << average_us * kUsToNs
                << " ns\n";
  return benchmark_log.str();
}

static std::string JNI_Benchmark_RunNativeToJavaStringsBenchmark(JNIEnv* env) {
  std::stringstream benchmark_log;
  const int kNumTries = 10000;
  const int kStringSize = 1000;
  std::string u8_ascii_string = "";
  std::string u8_non_ascii_string = "";
  std::u16string u16_ascii_string = u"";
  std::u16string u16_non_ascii_string = u"";
  for (int i = 0; i < kStringSize; i++) {
    u8_ascii_string += "a";
    u8_non_ascii_string += "ق";
    u16_ascii_string += u"a";
    u16_non_ascii_string += u"ق";
  }

  base::ElapsedTimer timer;
  for (int i = 0; i < kNumTries; i++) {
    Java_Benchmark_receiveU8String(env, u8_ascii_string);
  }
  double elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Sending " << kStringSize
                << " chars utf-8 ASCII string [Native -> Java] (" << kNumTries
                << " times): Elapsed time = " << elapsed_us << " us\n";
  double average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per " << kStringSize
                << " char string = " << average_us * kUsToNs << " ns\n";

  elapsed_us = 0;
  timer = {};
  for (int i = 0; i < kNumTries; i++) {
    Java_Benchmark_receiveU16String(env, u16_ascii_string);
  }
  elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Sending " << kStringSize
                << " chars utf-16 ASCII string [Native -> Java] (" << kNumTries
                << " times): Elapsed time = " << elapsed_us << " us\n";
  average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per " << kStringSize
                << " char string = " << average_us * kUsToNs << " ns\n";

  elapsed_us = 0;
  timer = {};
  for (int i = 0; i < kNumTries; i++) {
    Java_Benchmark_receiveU8String(env, u8_non_ascii_string);
  }
  elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Sending " << kStringSize
                << " chars utf-8 non-ASCII string [Native -> Java] ("
                << kNumTries << " times): Elapsed time = " << elapsed_us
                << " us\n";
  average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per " << kStringSize
                << " char string = " << average_us * kUsToNs << " ns\n";

  elapsed_us = 0;
  timer = {};
  for (int i = 0; i < kNumTries; i++) {
    Java_Benchmark_receiveU16String(env, u16_non_ascii_string);
  }
  elapsed_us = timer.Elapsed().InMicrosecondsF();
  benchmark_log << "Sending " << kStringSize
                << " chars utf-16 non-ASCII string [Native -> Java] ("
                << kNumTries << " times): Elapsed time = " << elapsed_us
                << " us\n";
  average_us = elapsed_us / kNumTries;
  benchmark_log << "Average per " << kStringSize
                << " char string = " << average_us * kUsToNs << " ns\n";
  return benchmark_log.str();
}

static void JNI_Benchmark_SendLargeIntArray(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jintArray>& j_array) {
  size_t array_size = static_cast<size_t>(env->GetArrayLength(j_array.obj()));
  jint* array = env->GetIntArrayElements(j_array.obj(), nullptr);
  for (size_t i = 0; i < array_size; i++) {
    DoNotOptimize(array[i]);
  }
  env->ReleaseIntArrayElements(j_array.obj(), array, 0);
}

static void JNI_Benchmark_SendLargeIntArrayConverted(
    JNIEnv* env,
    std::vector<int32_t>& array) {
  for (size_t i = 0; i < array.size(); i++) {
    DoNotOptimize(array[i]);
  }
}

static void JNI_Benchmark_SendByteArrayUseView(JNIEnv* env,
                                               ByteArrayView& array_view) {
  for (size_t i = 0; i < array_view.size(); i++) {
    DoNotOptimize(array_view.data());
  }
}

static void JNI_Benchmark_SendLargeObjectArray(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& j_array) {
  size_t array_size = static_cast<size_t>(env->GetArrayLength(j_array.obj()));
  for (size_t i = 0; i < array_size; i++) {
    DoNotOptimize(JNI_Integer::Java_Integer_intValue(
        env, JavaParamRef(env, env->GetObjectArrayElement(j_array.obj(), i))));
  }
}

static void JNI_Benchmark_SendLargeObjectList(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_list) {
  size_t array_size = static_cast<size_t>(CollectionSize(env, j_list));
  for (size_t i = 0; i < array_size; i++) {
    DoNotOptimize(
        JNI_Integer::Java_Integer_intValue(env, ListGet(env, j_list, i)));
  }
}

static void JNI_Benchmark_SendSingleInt(JNIEnv* env, jint param) {
  DoNotOptimize(param);
}

static void JNI_Benchmark_SendSingleInteger(
    JNIEnv* env,
    const JavaParamRef<jobject>& param) {
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, param));
}

static void JNI_Benchmark_Send10Ints(JNIEnv* env,
                                     jint a,
                                     jint b,
                                     jint c,
                                     jint d,
                                     jint e,
                                     jint f,
                                     jint g,
                                     jint h,
                                     jint i,
                                     jint j) {
  DoNotOptimize(a + b + c + d + e + f + g + h + i + j);
}

static void JNI_Benchmark_Send10Integers(JNIEnv* env,
                                         const JavaParamRef<jobject>& a,
                                         const JavaParamRef<jobject>& b,
                                         const JavaParamRef<jobject>& c,
                                         const JavaParamRef<jobject>& d,
                                         const JavaParamRef<jobject>& e,
                                         const JavaParamRef<jobject>& f,
                                         const JavaParamRef<jobject>& g,
                                         const JavaParamRef<jobject>& h,
                                         const JavaParamRef<jobject>& i,
                                         const JavaParamRef<jobject>& j) {
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, a));
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, b));
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, c));
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, d));
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, e));
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, f));
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, g));
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, h));
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, i));
  DoNotOptimize(JNI_Integer::Java_Integer_intValue(env, j));
}

static void JNI_Benchmark_SendAsciiStringConvertedToU8(JNIEnv* env,
                                                       std::string& param) {}

static void JNI_Benchmark_SendAsciiStringConvertedToU16(JNIEnv* env,
                                                        std::u16string& param) {
}

static void JNI_Benchmark_SendNonAsciiStringConvertedToU8(JNIEnv* env,
                                                          std::string& param) {}

static void JNI_Benchmark_SendNonAsciiStringConvertedToU16(
    JNIEnv* env,
    std::u16string& param) {}

static void JNI_Benchmark_CallMe(JNIEnv* env) {}

static void JNI_Benchmark_SendListConverted(
    JNIEnv* env,
    std::vector<ScopedJavaLocalRef<jobject>>& vec) {
  for (size_t i = 0; i < vec.size(); i++) {
    DoNotOptimize(vec[i].obj());
  }
}

static void JNI_Benchmark_SendListObject(JNIEnv* env,
                                         const JavaParamRef<jobject>& j_list) {
  int size = CollectionSize(env, j_list);
  for (int i = 0; i < size; i++) {
    DoNotOptimize(ListGet(env, j_list, i).obj());
  }
}

}  // namespace jni_zero::benchmark

DEFINE_JNI(Benchmark)
DEFINE_JNI(Integer)
