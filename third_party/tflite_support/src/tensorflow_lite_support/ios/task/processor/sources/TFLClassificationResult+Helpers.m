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
#import "tensorflow_lite_support/ios/task/processor/sources/TFLClassificationResult+Helpers.h"

@implementation TFLClassificationResult (Helpers)

+ (TFLClassificationResult *)classificationResultWithCResult:
    (TfLiteClassificationResult *)cClassificationResult {
  if (cClassificationResult == nil) return nil;

  NSMutableArray *classificationHeads = [[NSMutableArray alloc] init];
  for (int i = 0; i < cClassificationResult->size; i++) {
    TfLiteClassifications cClassifications = cClassificationResult->classifications[i];
    NSMutableArray *classes = [[NSMutableArray alloc] init];
    for (int j = 0; j < cClassifications.size; j++) {
      TfLiteCategory cCategory = cClassifications.categories[j];

      TFLCategory *resultCategory = [TFLCategory categoryWithCCategory:&cCategory];
      [classes addObject:resultCategory];
    }
    TFLClassifications *classificationHead = [[TFLClassifications alloc] init];
    classificationHead.categories = classes;
    classificationHead.headIndex = i;
    [classificationHeads addObject:classificationHead];
  }

  TFLClassificationResult *classificationResult = [[TFLClassificationResult alloc] init];
  classificationResult.classifications = classificationHeads;
  return classificationResult;
}
@end
