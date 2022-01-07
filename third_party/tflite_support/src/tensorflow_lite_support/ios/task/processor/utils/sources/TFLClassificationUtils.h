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
#import <Foundation/Foundation.h>
#import "third_party/tensorflow_lite_support/ios/task/processor/sources/TFLClassificationResult.h"

#include "tensorflow_lite_support/c/task/processor/classification_result.h"

NS_ASSUME_NONNULL_BEGIN

/** Helper utility for conversion between TFLite Task C Library Classification
 * Results and iOS Classification Results . */
@interface TFLClassificationUtils : NSObject

/**
 * Creates and retrurns a TFLClassificationResult from a
 * TfLiteClassificationResult returned by TFLite Task C Library Classification
 * tasks.
 *
 * @param cClassificationResult Classification results returned by TFLite Task C
 * Library Classification tasks
 *
 * @return Classification Result of type TFLClassificationResult to be returned
 * by inference methods of the iOS TF Lite Task Classification tasks.
 */
+ (TFLClassificationResult*)classificationResultFromCClassificationResults:
    (TfLiteClassificationResult*)cClassificationResult;

- (instancetype)init NS_UNAVAILABLE;

+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
