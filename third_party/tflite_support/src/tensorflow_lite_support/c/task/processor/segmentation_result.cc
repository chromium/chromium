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

#include "tensorflow_lite_support/c/task/processor/segmentation_result.h"

#include <memory>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void TfLiteSegmentationResultDelete(
    TfLiteSegmentationResult* segmentation_result) {
  for (int i = 0; i < segmentation_result->size; ++i) {
    TfLiteSegmentation segmentation = segmentation_result->segmentations[i];
    for (int j = 0; j < segmentation.colored_labels_size; ++j) {
      free(segmentation.colored_labels[j].display_name);
      free(segmentation.colored_labels[j].label);
    }

    if (segmentation.confidence_masks != nullptr) {
      for (int j = 0; j < segmentation.colored_labels_size; ++j) {
        delete[] segmentation.confidence_masks[j];
      }
      delete[] segmentation.confidence_masks;
    }

    delete[] segmentation.category_mask;

    delete[] segmentation.colored_labels;
  }

  delete[] segmentation_result->segmentations;
  delete segmentation_result;
}

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
