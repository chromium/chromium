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

#include <jni.h>

#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace tflite {
namespace task {
namespace text {
namespace nlclassifier {

using ::tflite::support::utils::ConvertVectorToArrayList;
using ::tflite::support::utils::JStringToString;
using ::tflite::task::core::Category;
using ::tflite::task::text::nlclassifier::NLClassifier;

jobject RunClassifier(JNIEnv* env, jlong native_handle, jstring text) {
  auto* nl_classifier = reinterpret_cast<NLClassifier*>(native_handle);

  auto results = nl_classifier->Classify(JStringToString(env, text));
  jclass category_class =
      env->FindClass("org/tensorflow/lite/support/label/Category");
  jmethodID category_init =
      env->GetMethodID(category_class, "<init>", "(Ljava/lang/String;F)V");

  return ConvertVectorToArrayList(
      env, results.begin(), results.end(),
      [env, category_class, category_init](const Category& category) {
        jstring class_name = env->NewStringUTF(category.class_name.data());
        // Convert double to float as Java interface exposes float as scores.
        jobject jcategory =
            env->NewObject(category_class, category_init, class_name,
                           static_cast<float>(category.score));
        env->DeleteLocalRef(class_name);
        return jcategory;
      });
}

}  // namespace nlclassifier
}  // namespace text
}  // namespace task
}  // namespace tflite
