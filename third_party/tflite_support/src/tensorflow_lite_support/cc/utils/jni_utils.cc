/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/cc/utils/jni_utils.h"

#include <dlfcn.h>
#include <string.h>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow/lite/acceleration/configuration/c/delegate_plugin.h"
#include "tensorflow/lite/acceleration/configuration/delegate_plugin_converter.h"
#include "tensorflow/lite/acceleration/configuration/delegate_registry.h"
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"

namespace tflite {
namespace support {
namespace utils {
namespace {

using ::absl::StatusCode;
using ::tflite::proto::Delegate;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::delegates::DelegatePluginRegistry;

// delegate_name should be one of the following:
// gpu / hexagon
absl::Status loadDelegatePluginLibrary(const std::string& delegate_name) {
  // Load "lib<delegate_name>_plugin.so".
  std::string lib_name =
      absl::StrFormat("lib%s_delegate_plugin.so", delegate_name);

  // Choosing RTLD_NOW over RTLD_LAZY: RTLD_NOW loads symbols now and
  // makes sure there's no unresolved symbols. Using RTLD_LAZY will not
  // discover unresolved symbols issues right away, and may lead to crash later
  // during inference, which should be avoided.
  // Choosing RTLD_LOCAL over RTLD_GLOBAL: the symbols should not be available
  // for subsequently loaded libraries.
  // Not choosing RTLD_DEEPBIND due to portability concerns; also we're using a
  // linker script to hide internal symbols, so we don't really need it.
  // Not choosing RTLD_NODELETE to avoid a (bounded) memory leak:
  // if we used RTLD_NODELETE, dlclose() would not free the memory for the
  // library.
  void* handle = dlopen(lib_name.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Error loading %s. %s", lib_name, dlerror()));
  }

  // Load the method "TfLite<camel_name>DelegatePluginCApi".
  std::string camel_name(delegate_name);
  camel_name[0] = toupper(camel_name[0]);
  std::string new_method_name =
      absl::StrFormat("TfLite%sDelegatePluginCApi", camel_name);
  TfLiteOpaqueDelegatePlugin* (*new_delegate_c)();
  new_delegate_c = reinterpret_cast<decltype(new_delegate_c)>(
      dlsym(handle, new_method_name.c_str()));
  if (!new_delegate_c) {
    // Ignore the return value of dlclose as we deliberately hide it from users.
    dlclose(handle);
    handle = nullptr;
    return CreateStatusWithPayload(
        StatusCode::kInternal,
        absl::StrFormat("Error loading method, %s from %s", new_method_name,
                        lib_name));
  }

  // Register the delegate.
  new DelegatePluginRegistry::Register(
      absl::StrFormat("%sPlugin", camel_name),
      tflite::delegates::DelegatePluginConverter(*new_delegate_c()));

  return absl::OkStatus();
}

}  // namespace

tflite::support::StatusOr<Delegate> ConvertToProtoDelegate(jint delegate) {
  // The supported delegate types should match
  // org.tensorflow.lite.task.core.ComputeSettings.Delegate.
  switch (delegate) {
    case 0:
      return Delegate::NONE;
    case 1:
      return Delegate::NNAPI;
    case 2:
      TFLITE_RETURN_IF_ERROR(loadDelegatePluginLibrary("gpu"));
      return Delegate::GPU;
    default:
      break;
  }
  // Should never happen.
  return CreateStatusWithPayload(
      StatusCode::kInternal,
      absl::StrFormat("The delegate type is unsupported: %d", delegate));
}

std::string JStringToString(JNIEnv* env, jstring jstr) {
  if (jstr == nullptr) {
    return std::string();
  }
  const char* cstring = env->GetStringUTFChars(jstr, nullptr);
  std::string result(cstring);
  env->ReleaseStringUTFChars(jstr, cstring);
  return result;
}

std::vector<std::string> StringListToVector(JNIEnv* env, jobject list_object) {
  jobject j_iterator = env->CallObjectMethod(
      list_object, env->GetMethodID(env->GetObjectClass(list_object),
                                    "iterator", "()Ljava/util/Iterator;"));
  std::vector<std::string> result;
  jmethodID has_next =
      env->GetMethodID(env->GetObjectClass(j_iterator), "hasNext", "()Z");
  jmethodID get_next = env->GetMethodID(env->GetObjectClass(j_iterator), "next",
                                        "()Ljava/lang/Object;");
  while (env->CallBooleanMethod(j_iterator, has_next)) {
    jstring jstr =
        static_cast<jstring>(env->CallObjectMethod(j_iterator, get_next));
    const char* raw_str = env->GetStringUTFChars(jstr, JNI_FALSE);
    result.emplace_back(std::string(raw_str));
    env->ReleaseStringUTFChars(jstr, raw_str);
  }
  return result;
}

absl::string_view GetMappedFileBuffer(JNIEnv* env, const jobject& file_buffer) {
  return absl::string_view(
      static_cast<char*>(env->GetDirectBufferAddress(file_buffer)),
      static_cast<size_t>(env->GetDirectBufferCapacity(file_buffer)));
}

jbyteArray CreateByteArray(JNIEnv* env, const jbyte* data, int num_bytes) {
  jbyteArray ret = env->NewByteArray(num_bytes);
  env->SetByteArrayRegion(ret, 0, num_bytes, data);

  return ret;
}

void ThrowException(JNIEnv* env, const char* clazz, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const size_t max_msg_len = 512;
  auto* message = static_cast<char*>(malloc(max_msg_len));
  if (message && (vsnprintf(message, max_msg_len, fmt, args) >= 0)) {
    ThrowExceptionWithMessage(env, clazz, message);
  } else {
    ThrowExceptionWithMessage(env, clazz, "");
  }
  if (message) {
    free(message);
  }
  va_end(args);
}

void ThrowExceptionWithMessage(JNIEnv* env, const char* clazz,
                               const char* message) {
  jclass e_class = env->FindClass(clazz);
  if (strcmp(clazz, kAssertionError) == 0) {
    // AssertionError cannot use ThrowNew in Java 7
    jmethodID constructor =
        env->GetMethodID(e_class, "<init>", "(Ljava/lang/Object;)V");
    jstring jstr_message = env->NewStringUTF(message);
    jobject e_object = env->NewObject(e_class, constructor,
                                      static_cast<jobject>(jstr_message));
    env->Throw(static_cast<jthrowable>(e_object));
    return;
  }
  env->ThrowNew(e_class, message);
}

const char* GetExceptionClassNameForStatusCode(StatusCode status_code) {
  switch (status_code) {
    case StatusCode::kOk:
      return nullptr;
    case StatusCode::kInvalidArgument:
      return kIllegalArgumentException;
    // TODO(b/197650198): Uncomment this before the next major version bump
    //  and update the signature, as IOException is a checked exception.
    // case StatusCode::kNotFound:
    //   return kIOException;
    case StatusCode::kInternal:
      return kIllegalStateException;
    // kUnknown and all other status codes are mapped to a generic
    // RuntimeException.
    case StatusCode::kUnknown:
    default:
      return kRuntimeException;
  }
}

}  // namespace utils
}  // namespace support
}  // namespace tflite
