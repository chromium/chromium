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

#include "tensorflow_lite_support/c/task/text/bert_nl_classifier.h"

#include <memory>

#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/task/core/category.h"
#include "tensorflow_lite_support/cc/task/text/bert_nl_classifier.h"
#include "tensorflow_lite_support/cc/task/text/proto/bert_nl_classifier_options_proto_inc.h"

namespace {
using CategoryCpp = ::tflite::task::core::Category;
using BertNLClassifierCpp = ::tflite::task::text::BertNLClassifier;
using BertNLClassifierOptionsCpp =
    ::tflite::task::text::BertNLClassifierOptions;

const TfLiteBertNLClassifierOptions kBertNLClassifierOptionsDefault = {128};
}  // namespace

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct TfLiteBertNLClassifier {
  std::unique_ptr<BertNLClassifierCpp> impl;
};

TfLiteBertNLClassifier* TfLiteBertNLClassifierCreateFromOptions(
    const char* model_path, const TfLiteBertNLClassifierOptions* options) {
  BertNLClassifierOptionsCpp cc_options;

  cc_options.mutable_base_options()->mutable_model_file()->set_file_name(
      model_path);
  auto classifier_status = BertNLClassifierCpp::CreateFromOptions(cc_options);

  if (classifier_status.ok()) {
    return new TfLiteBertNLClassifier{
        .impl = std::unique_ptr<BertNLClassifierCpp>(
            dynamic_cast<BertNLClassifierCpp*>(
                classifier_status.value().release()))};

  } else {
    return nullptr;
  }
}

TfLiteBertNLClassifier* TfLiteBertNLClassifierCreate(const char* model_path) {
  return TfLiteBertNLClassifierCreateFromOptions(
      model_path, &kBertNLClassifierOptionsDefault);
}

Categories* TfLiteBertNLClassifierClassify(
    const TfLiteBertNLClassifier* classifier, const char* text) {
  std::vector<CategoryCpp> results =

      classifier->impl->Classify(absl::string_view(text).data());
  size_t size = results.size();
  auto* categories = new Category[size];

  for (size_t i = 0; i < size; ++i) {
    categories[i].text = strdup(results[i].class_name.c_str());
    categories[i].score = results[i].score;
  }

  auto* c_categories = new Categories;
  c_categories->size = size;
  c_categories->categories = categories;
  return c_categories;
}

void TfLiteBertNLClassifierDelete(TfLiteBertNLClassifier* classifier) {
  delete classifier;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
