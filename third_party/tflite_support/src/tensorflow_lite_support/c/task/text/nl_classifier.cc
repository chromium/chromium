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

#include "tensorflow_lite_support/c/task/text/nl_classifier.h"

#include <memory>

#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/task/core/category.h"
#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"

namespace {
using CategoryCpp = ::tflite::task::core::Category;
using NLClassifierCpp = ::tflite::task::text::nlclassifier::NLClassifier;
using NLClassifierOptionsCpp =
    ::tflite::task::text::nlclassifier::NLClassifierOptions;
}  // namespace

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct TfLiteNLClassifier {
  std::unique_ptr<NLClassifierCpp> impl;
};

TfLiteNLClassifier* TfLiteNLClassifierCreateFromOptions(
    const char* model_path, const TfLiteNLClassifierOptions* options) {
  auto classifier_status = NLClassifierCpp::CreateFromFileAndOptions(
      std::string(model_path),
      {
          .input_tensor_index = options->input_tensor_index,
          .output_score_tensor_index = options->output_score_tensor_index,
          .output_label_tensor_index = options->output_label_tensor_index,
          .input_tensor_name = !options->input_tensor_name
                                   ? ""
                                   : std::string(options->input_tensor_name),
          .output_score_tensor_name =
              !options->output_score_tensor_name
                  ? ""
                  : std::string(options->output_score_tensor_name),
          .output_label_tensor_name =
              !options->output_label_tensor_name
                  ? ""
                  : std::string(options->output_label_tensor_name),
      });

  if (classifier_status.ok()) {
    return new TfLiteNLClassifier{
        .impl = std::unique_ptr<NLClassifierCpp>(dynamic_cast<NLClassifierCpp*>(
            classifier_status.value().release()))};
  } else {
    return nullptr;
  }
}

Categories* TfLiteNLClassifierClassify(const TfLiteNLClassifier* classifier,
                                       const char* text) {
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

void TfLiteNLClassifierDelete(TfLiteNLClassifier* classifier) {
  delete classifier;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
