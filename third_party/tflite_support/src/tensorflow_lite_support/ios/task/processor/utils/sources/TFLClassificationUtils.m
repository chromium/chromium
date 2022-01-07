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
#import "tensorflow_lite_support/ios/task/processor/utils/sources/TFLClassificationUtils.h"

@implementation TFLClassificationUtils

+ (TFLClassificationResult*)classificationResultFromCClassificationResults:
    (TfLiteClassificationResult*)cClassificationResult {
  if (cClassificationResult == nil)
    return nil;

  NSMutableArray* classificationHeads = [[NSMutableArray alloc] init];
  for (int i = 0; i < cClassificationResult->size; i++) {
    TfLiteClassifications cClassifications =
        cClassificationResult->classifications[i];
    NSMutableArray* classes = [[NSMutableArray alloc] init];
    for (int j = 0; j < cClassifications.size; j++) {
      TfLiteCategory cCategory = cClassifications.categories[j];
      TFLCategory* resultCategory = [[TFLCategory alloc] init];

      if (cCategory.display_name != nil) {
        resultCategory.displayName =
            [NSString stringWithCString:cCategory.display_name
                               encoding:NSUTF8StringEncoding];
      }

      if (cCategory.label != nil) {
        resultCategory.label =
            [NSString stringWithCString:cCategory.label
                               encoding:NSUTF8StringEncoding];
      }

      resultCategory.score = cCategory.score;
      resultCategory.classIndex = (NSInteger)cCategory.index;
      [classes addObject:resultCategory];
    }
    TFLClassifications* classificationHead = [[TFLClassifications alloc] init];
    classificationHead.categories = classes;
    classificationHead.headIndex = i;
    [classificationHeads addObject:classificationHead];
  }

  TFLClassificationResult* classificationResult =
      [[TFLClassificationResult alloc] init];
  classificationResult.classifications = classificationHeads;
  return classificationResult;
}

@end
