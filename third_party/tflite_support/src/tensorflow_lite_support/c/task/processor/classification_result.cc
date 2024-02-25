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

#include "tensorflow_lite_support/c/task/processor/classification_result.h"

#include <cstdlib>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void TfLiteClassificationResultDelete(
    TfLiteClassificationResult* classification_result) {
  for (int head = 0; head < classification_result->size; ++head) {
    TfLiteClassifications classifications =
        classification_result->classifications[head];
        free(classifications.head_name);
    for (int rank = 0; rank < classifications.size; ++rank) {
      TfLiteCategoryDelete(&(classifications.categories[rank]));
    }

    delete[] classifications.categories;
  }

  delete[] classification_result->classifications;
  delete classification_result;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
