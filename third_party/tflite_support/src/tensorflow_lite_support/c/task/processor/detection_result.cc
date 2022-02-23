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

#include "tensorflow_lite_support/c/task/processor/detection_result.h"

#include <cstdlib>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void TfLiteDetectionResultDelete(TfLiteDetectionResult* detection_result) {
  for (int i = 0; i < detection_result->size; ++i) {
    TfLiteDetection detections = detection_result->detections[i];
    for (int j = 0; j < detections.size; ++j) {
      TfLiteCategoryDelete(&(detections.categories[j]));
    }

    delete[] detections.categories;
  }

  delete[] detection_result->detections;
  delete detection_result;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
