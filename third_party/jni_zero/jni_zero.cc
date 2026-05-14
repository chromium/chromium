// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/jni_zero.h"

#include <sys/prctl.h>

#include <cassert>
#include <type_traits>

#include "third_party/jni_zero/generate_jni/JniZero_jni.h"
#include "third_party/jni_zero/jni_methods.h"
#include "third_party/jni_zero/jni_zero_internal.h"
#include "third_party/jni_zero/logging.h"
#include "third_party/jni_zero/system_jni_unchecked_exceptions/ClassLoader_jni.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/393091624): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#if defined(JNI_ZERO_MULTIPLEXING_ENABLED)
extern const int64_t kJniZeroHashWhole;
extern const int64_t kJniZeroHashPriority;
#endif

// Ensure the types we're using (bool, int32_t, etc) are compatible with the
// types used by jni (jboolean, jint, etc).
static_assert(sizeof(jboolean) == sizeof(bool));
static_assert(alignof(jboolean) <= alignof(bool));
static_assert(std::is_same<jbyte, int8_t>::value);
static_assert(std::is_same<jchar, uint16_t>::value);
static_assert(std::is_same<jshort, int16_t>::value);
static_assert(std::is_same<jint, int32_t>::value);
static_assert(std::is_same<jlong, int64_t>::value);
static_assert(std::is_same<jfloat, float>::value);
static_assert(std::is_same<jdouble, double>::value);

// Make sure our array type aliasing works.
static_assert(std::is_same<JArray<jobject>, jobjectArray>::value);
static_assert(std::is_same<JArray<bool>, jbooleanArray>::value);

namespace jni_zero {
namespace {

const size_t kMaxClassNameLen = 256;

// Until we fully migrate base's jni_android, we will maintain a copy of this
// global here and will have base set this variable when it sets its own.
JavaVM* g_jvm = nullptr;

jclass (*g_class_resolver)(JNIEnv*, const char*) = nullptr;

LeakedJavaGlobalRef<JClassLoader> g_class_loader = nullptr;

void (*g_exception_handler_callback)(JNIEnv*) = nullptr;

jclass DefaultClassResolver(JNIEnv* env, const char* class_name) {
  JNI_ZERO_DCHECK(g_class_loader);
  auto j_class_name = jni_zero::AdoptRef(env, env->NewStringUTF(class_name));
  return g_class_loader->loadClass(env, j_class_name).Release();
}

jclass GetClassInternal(JNIEnv* env, const char* class_name) {
  if (g_class_resolver != nullptr) {
    return g_class_resolver(env, class_name);
  }

  // This code should be hit only before (or during) InitVM().
  size_t bufsize = strlen(class_name) + 1;
  if (bufsize > kMaxClassNameLen) {
    JNI_ZERO_FLOG("Class name too long: %s", class_name);
  }

  char slash_name[kMaxClassNameLen];
  for (size_t i = 0; i < bufsize; ++i) {
    char c = class_name[i];
    if (c == '.') {
      c = '/';
    }
    slash_name[i] = c;
  }
  return env->FindClass(slash_name);
}

jclass GetClassGlobalRef(JNIEnv* env, jobject obj) {
  return static_cast<jclass>(env->NewGlobalRef(env->GetObjectClass(obj)));
}

}  // namespace

void JNI_JniZero_SetJniClassLoader(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& classLoader) {
  SetClassLoader(env, classLoader);
}

jclass g_class_loader_class = nullptr;
jclass g_object_class = nullptr;
jclass g_string_class = nullptr;
LeakedJavaGlobalRef<jstring> g_empty_string = nullptr;
LeakedJavaGlobalRef<jobject> g_empty_list = nullptr;
LeakedJavaGlobalRef<jobject> g_empty_map = nullptr;

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
  if (g_jvm) {
    JNI_ZERO_CHECK(vm == g_jvm);
    return;
  }
  g_jvm = vm;
  JNIEnv* env = AttachCurrentThread();
#if defined(JNI_ZERO_MULTIPLEXING_ENABLED)
  JniZeroJni::crashIfMultiplexingMisaligned(env, kJniZeroHashWhole,
                                            kJniZeroHashPriority);
#else
  // Mark as used when multiplexing not enabled.
  (void)&Java_JniZero_crashIfMultiplexingMisaligned;
#endif
  ScopedJavaLocalRef<JArray<jobject>> globals = JniZeroJni::init(env);
  jobject empty_list = env->GetObjectArrayElement(globals.obj(), 0);
  jobject empty_map = env->GetObjectArrayElement(globals.obj(), 1);
  jobject jni_class_loader = env->GetObjectArrayElement(globals.obj(), 2);

  // Leak a few local refs since JNI will clean them up for us anyways.
  g_empty_list.Reset(env, CreateLeaky(env, empty_list));
  g_empty_map.Reset(env, CreateLeaky(env, empty_map));
  g_empty_string.Reset(env, CreateLeaky(env, env->NewString(nullptr, 0)));

  g_string_class = GetClassGlobalRef(env, g_empty_string.obj());
  g_class_loader_class = GetClassGlobalRef(env, jni_class_loader);
  g_object_class = static_cast<jclass>(
      env->NewGlobalRef(env->GetSuperclass(g_string_class)));

  if (!g_class_resolver) {
    // Use ClassLoader.loadClass() rather than env->FindClass() because
    // env->FindClass() uses the bootstrap classloader for threads created by
    // native code (which leads to classes not being able to be found).
    if (!g_class_loader) {
      g_class_loader.Reset(env, CreateLeaky(env, jni_class_loader));
    }
    g_class_resolver = &DefaultClassResolver;
  }
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
  env->ExceptionDescribe();
  JNI_ZERO_FLOG("jni_zero crashing due to uncaught Java exception");
}

void SetClassResolver(jclass (*resolver)(JNIEnv*, const char*)) {
  g_class_resolver = resolver;
}

void SetClassLoader(JNIEnv* env, const JavaRef<jobject>& class_loader) {
  JNI_ZERO_DCHECK(class_loader);
  g_class_loader.Reset(env, class_loader);
}

ScopedJavaLocalRef<jclass> GetClass(JNIEnv* env, const char* class_name) {
  return jni_zero::AdoptRef(env, GetClassInternal(env, class_name));
}

template <MethodID::Type type>
jmethodID MethodID::Get(JNIEnv* env,
                        jclass clazz,
                        const char* method_name,
                        const char* jni_signature) {
  auto get_method_ptr = type == MethodID::TYPE_STATIC
                            ? &JNIEnv::GetStaticMethodID
                            : &JNIEnv::GetMethodID;
  jmethodID id = (env->*get_method_ptr)(clazz, method_name, jni_signature);
  if (ClearException(env) || !id) {
    JNI_ZERO_FLOG("Failed to find class %smethod %s %s",
                  (type == TYPE_STATIC ? "static " : ""), method_name,
                  jni_signature);
  }
  return id;
}

// If |atomic_method_id| set, it'll return immediately. Otherwise, it'll call
// into ::Get() above. If there's a race, it's ok since the values are the same
// (and the duplicated effort will happen only once).
template <MethodID::Type type>
jmethodID MethodID::LazyGet(JNIEnv* env,
                            jclass clazz,
                            const char* method_name,
                            const char* jni_signature,
                            std::atomic<jmethodID>* atomic_method_id) {
  const jmethodID value = atomic_method_id->load(std::memory_order_acquire);
  if (value) {
    return value;
  }
  jmethodID id = MethodID::Get<type>(env, clazz, method_name, jni_signature);
  atomic_method_id->store(id, std::memory_order_release);
  return id;
}

// Various template instantiations.
template jmethodID MethodID::Get<MethodID::TYPE_STATIC>(
    JNIEnv* env,
    jclass clazz,
    const char* method_name,
    const char* jni_signature);

template jmethodID MethodID::Get<MethodID::TYPE_INSTANCE>(
    JNIEnv* env,
    jclass clazz,
    const char* method_name,
    const char* jni_signature);

template jmethodID MethodID::LazyGet<MethodID::TYPE_STATIC>(
    JNIEnv* env,
    jclass clazz,
    const char* method_name,
    const char* jni_signature,
    std::atomic<jmethodID>* atomic_method_id);

template jmethodID MethodID::LazyGet<MethodID::TYPE_INSTANCE>(
    JNIEnv* env,
    jclass clazz,
    const char* method_name,
    const char* jni_signature,
    std::atomic<jmethodID>* atomic_method_id);

namespace internal {
template <FieldID::Type type>
jfieldID FieldID::Get(JNIEnv* env,
                      jclass clazz,
                      const char* field_name,
                      const char* jni_signature) {
  auto get_field_ptr = type == FieldID::TYPE_STATIC ? &JNIEnv::GetStaticFieldID
                                                    : &JNIEnv::GetFieldID;
  jfieldID id = (env->*get_field_ptr)(clazz, field_name, jni_signature);
  if (ClearException(env) || !id) {
    JNI_ZERO_FLOG("Failed to find class %sfield %s %s",
                  (type == TYPE_STATIC ? "static " : ""), field_name,
                  jni_signature);
  }
  return id;
}

template <FieldID::Type type>
jfieldID FieldID::LazyGet(JNIEnv* env,
                          jclass clazz,
                          const char* field_name,
                          const char* jni_signature,
                          std::atomic<jfieldID>* atomic_field_id) {
  const jfieldID value = atomic_field_id->load(std::memory_order_acquire);
  if (value) {
    return value;
  }
  jfieldID id = FieldID::Get<type>(env, clazz, field_name, jni_signature);
  atomic_field_id->store(id, std::memory_order_release);
  return id;
}

template jfieldID FieldID::Get<FieldID::TYPE_STATIC>(JNIEnv* env,
                                                     jclass clazz,
                                                     const char* field_name,
                                                     const char* jni_signature);

template jfieldID FieldID::Get<FieldID::TYPE_INSTANCE>(
    JNIEnv* env,
    jclass clazz,
    const char* field_name,
    const char* jni_signature);

template jfieldID FieldID::LazyGet<FieldID::TYPE_STATIC>(
    JNIEnv* env,
    jclass clazz,
    const char* field_name,
    const char* jni_signature,
    std::atomic<jfieldID>* atomic_field_id);

template jfieldID FieldID::LazyGet<FieldID::TYPE_INSTANCE>(
    JNIEnv* env,
    jclass clazz,
    const char* field_name,
    const char* jni_signature,
    std::atomic<jfieldID>* atomic_field_id);

jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    std::atomic<jclass>* atomic_class_id) {
  jclass ret = atomic_class_id->load(std::memory_order_acquire);
  if (ret == nullptr) {
    auto local_ref = jni_zero::AdoptRef(env, GetClassInternal(env, class_name));
    jclass global_ref = static_cast<jclass>(env->NewGlobalRef(local_ref.obj()));
    if (atomic_class_id->compare_exchange_strong(ret, global_ref,
                                                 std::memory_order_acq_rel)) {
      // We intentionally leak the global ref since we are now storing it as a
      // raw pointer in |atomic_class_id|.
      ret = global_ref;
    } else {
      env->DeleteGlobalRef(global_ref);
    }
  }
  return ret;
}

}  // namespace internal
}  // namespace jni_zero

DEFINE_JNI(JniZero)
