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
  if (!cClassificationResult) return nil;

  NSMutableArray *classificationHeads = [[NSMutableArray alloc] init];
  for (int i = 0; i < cClassificationResult->size; i++) {
    TfLiteClassifications cClassifications = cClassificationResult->classifications[i];
    NSMutableArray *categories = [[NSMutableArray alloc] init];
    for (int j = 0; j < cClassifications.size; j++) {
      TfLiteCategory cCategory = cClassifications.categories[j];
      [categories addObject:[TFLCategory categoryWithCCategory:&cCategory]];
    }

    NSString *headName = nil;

    if (cClassifications.head_name) {
      headName = [NSString stringWithCString:cClassifications.head_name encoding:NSUTF8StringEncoding];
    }
     
    TFLClassifications *classifications = [[TFLClassifications alloc] initWithHeadIndex:cClassifications.head_index
                                                                               headName:headName
                                                                             categories:categories];

    [classificationHeads addObject:classifications];
  }

  return [[TFLClassificationResult alloc] initWithClassifications:classificationHeads];
}
@end
