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
#ifndef TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_NLCLASSIFIER_NL_CLASSIFIER_C_API_H_
#define TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_NLCLASSIFIER_NL_CLASSIFIER_C_API_H_

#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier_c_api_common.h"
// --------------------------------------------------------------------------
/// C API for NLClassifier.
///
/// The API leans towards simplicity and uniformity instead of convenience, as
/// most usage will be by language-specific wrappers. It provides largely the
/// same set of functionality as that of the C++ TensorFlow Lite `NLClassifier`
/// API, but is useful for shared libraries where having a stable ABI boundary
/// is important.
///
/// Usage:
/// <pre><code>
/// // Create the model and interpreter options.
/// NLClassifier* classifier = NLClassifierFromFileAndOptions(
///   "/path/to/model.tflite");
///
/// // classification.
/// Categories* categories = Classify(classifier, context, question);
///
/// // Dispose of the API object.
/// NLClassifierDelete(classifier);

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct NLClassifier NLClassifier;

struct NLClassifierOptions {
  int input_tensor_index;
  int output_score_tensor_index;
  int output_label_tensor_index;
  const char* input_tensor_name;
  const char* output_score_tensor_name;
  const char* output_label_tensor_name;
};

// Creates NLClassifier from model path and options, returns nullptr if the file
// doesn't exist or is not a well formatted TFLite model path.
extern NLClassifier* NLClassifierFromFileAndOptions(
    const char* model_path,
    const struct NLClassifierOptions* options);

// Invokes the encapsulated TFLite model and classifies the input text.
extern struct Categories* NLClassifierClassify(const NLClassifier* classifier,
                                               const char* text);

extern void NLClassifierDelete(NLClassifier* classifier);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_NLCLASSIFIER_NL_CLASSIFIER_C_API_H_
