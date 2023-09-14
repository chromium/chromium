/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include <jni.h>

#include "tensorflow_lite_support/cc/task/core/proto/base_options_proto_inc.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace {

using ::tflite::proto::Delegate;
using ::tflite::support::StatusOr;
using ::tflite::support::utils::ConvertToProtoDelegate;
using ::tflite::support::utils::kIllegalStateException;
using ::tflite::support::utils::kInvalidPointer;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::core::BaseOptions;

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_core_TaskJniUtils_createProtoBaseOptions(
    JNIEnv* env, jclass thiz, jint delegate, jint num_threads) {
  StatusOr<Delegate> delegate_proto_or = ConvertToProtoDelegate(delegate);
  if (!delegate_proto_or.ok()) {
    ThrowException(env, kIllegalStateException,
                   "Error occurred when converting to the proto delegate: %s",
                   delegate_proto_or.status().message().data());
    return kInvalidPointer;
  }

  // base_options will be owned by the task proto options, such as
  // ImageClassifierOptions.
  BaseOptions* base_options = new BaseOptions();
  auto tflite_settings =
      base_options->mutable_compute_settings()->mutable_tflite_settings();
  tflite_settings->set_delegate(delegate_proto_or.value());
  tflite_settings->mutable_cpu_settings()->set_num_threads(num_threads);
  return reinterpret_cast<jlong>(base_options);
}

}  // namespace
