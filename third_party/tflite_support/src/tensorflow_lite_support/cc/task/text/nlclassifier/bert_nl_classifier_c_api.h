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
#ifndef TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_NLCLASSIFIER_BERT_NL_CLASSIFIER_C_API_H_
#define TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_NLCLASSIFIER_BERT_NL_CLASSIFIER_C_API_H_

#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier_c_api_common.h"
// --------------------------------------------------------------------------
/// C API for BertNLClassifier.
///
/// The API leans towards simplicity and uniformity instead of convenience, as
/// most usage will be by language-specific wrappers. It provides largely the
/// same set of functionality as that of the C++ TensorFlow Lite
/// `BertNLClassifier` API, but is useful for shared libraries where having
/// a stable ABI boundary is important.
///
/// Usage:
/// <pre><code>
/// // Create the model and interpreter options.
/// BertNLClassifier* classifier =
///   BertNLClassifierFromFile("/path/to/model.tflite");
///
/// // classification.
/// Categories* categories = Classify(classifier, context, question);
///
/// // Dispose of the API object.
/// BertNLClassifierrDelete(classifier);

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct BertNLClassifier BertNLClassifier;

// Creates BertNLClassifier from model path, returns nullptr if the file
// doesn't exist or is not a well formatted TFLite model path.
extern BertNLClassifier* BertNLClassifierFromFile(const char* model_path);

// Invokes the encapsulated TFLite model and classifies the input text.
extern struct Categories* BertNLClassifierClassify(
    const BertNLClassifier* classifier,
    const char* text);

extern void BertNLClassifierDelete(BertNLClassifier* classifier);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // TENSORFLOW_LITE_SUPPORT_CC_TASK_TEXT_NLCLASSIFIER_BERT_NL_CLASSIFIER_C_API_H_
