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
#import "tensorflow_lite_support/ios/task/processor/sources/TFLCategory+Helpers.h"
#import "tensorflow_lite_support/ios/task/processor/sources/TFLDetectionResult+Helpers.h"

@implementation TFLDetectionResult (Helpers)

+ (TFLDetectionResult *)detectionResultWithCResult:(TfLiteDetectionResult *)cDetectionResult {
  if (!cDetectionResult) return nil;

  NSMutableArray *detections = [[NSMutableArray alloc] init];
  for (int i = 0; i < cDetectionResult->size; i++) {
    TfLiteDetection cDetection = cDetectionResult->detections[i];
    NSMutableArray *categories = [[NSMutableArray alloc] init];
    for (int j = 0; j < cDetection.size; j++) {
      TfLiteCategory cCategory = cDetection.categories[j];

      TFLCategory *resultCategory = [TFLCategory categoryWithCCategory:&cCategory];
      [categories addObject:resultCategory];
    }
    TFLDetection *detection = [[TFLDetection alloc]
        initWithBoundingBox:CGRectMake(
                                cDetection.bounding_box.origin_x, cDetection.bounding_box.origin_y,
                                cDetection.bounding_box.width, cDetection.bounding_box.height)
                 categories:categories];
    [detections addObject:detection];
  }

  return [[TFLDetectionResult alloc] initWithDetections:detections];
}
@end
