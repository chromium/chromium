/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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
#import "tensorflow_lite_support/ios/task/processor/sources/TFLSegmentationResult+Helpers.h"

@implementation TFLSegmentationResult (Helpers)

+ (TFLSegmentationResult *)segmentationResultWithCResult:
    (TfLiteSegmentationResult *)cSegmentationResult {
  if (!cSegmentationResult) return nil;

  NSMutableArray *segmentations = [[NSMutableArray alloc] init];
  for (int i = 0; i < cSegmentationResult->size; i++) {
    TfLiteSegmentation cSegmentation = cSegmentationResult->segmentations[i];
    NSMutableArray *coloredLabels = [[NSMutableArray alloc] init];
    for (int j = 0; j < cSegmentation.colored_labels_size; j++) {
      TfLiteColoredLabel cColoredLabel = cSegmentation.colored_labels[j];

      NSString *displayName;
      if (cColoredLabel.display_name) {
        displayName = [NSString stringWithCString:cColoredLabel.display_name
                                         encoding:NSUTF8StringEncoding];
      }

      NSString *label;
      if (cColoredLabel.label) {
        label = [NSString stringWithCString:cColoredLabel.label encoding:NSUTF8StringEncoding];
      }

      TFLColoredLabel *coloredLabel =
          [[TFLColoredLabel alloc] initWithRed:(NSUInteger)cColoredLabel.r
                                         green:(NSUInteger)cColoredLabel.g
                                          blue:(NSUInteger)cColoredLabel.b
                                         label:label
                                   displayName:displayName];
      [coloredLabels addObject:coloredLabel];
    }

    TFLSegmentation *segmentation;

    if (cSegmentation.confidence_masks) {
      NSMutableArray *confidenceMasks = [[NSMutableArray alloc] init];
      for (int i = 0; i < cSegmentation.colored_labels_size; i++) {
        TFLConfidenceMask *confidenceMask =
            [[TFLConfidenceMask alloc] initWithWidth:(NSInteger)cSegmentation.width
                                              height:(NSInteger)cSegmentation.height
                                                mask:cSegmentation.confidence_masks[i]];
        [confidenceMasks addObject:confidenceMask];
      }
      segmentation = [[TFLSegmentation alloc] initWithConfidenceMasks:confidenceMasks
                                                        coloredLabels:coloredLabels];

    } else if (cSegmentation.category_mask) {
      TFLCategoryMask *categoryMask =
          [[TFLCategoryMask alloc] initWithWidth:(NSInteger)cSegmentation.width
                                          height:(NSInteger)cSegmentation.height
                                            mask:cSegmentation.category_mask];
      segmentation = [[TFLSegmentation alloc] initWithCategoryMask:categoryMask
                                                     coloredLabels:coloredLabels];
    }

    [segmentations addObject:segmentation];
  }

  return [[TFLSegmentationResult alloc] initWithSegmentations:segmentations];
}

@end
