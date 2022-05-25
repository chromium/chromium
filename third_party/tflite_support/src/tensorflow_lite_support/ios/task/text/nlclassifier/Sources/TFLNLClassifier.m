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
#import "tensorflow_lite_support/ios/task/text/nlclassifier/Sources/TFLNLClassifier.h"
#import "GTMDefines.h"
#include "tensorflow_lite_support/c/task/text/nl_classifier.h"
#include "tensorflow_lite_support/c/task/text/nl_classifier_common.h"

NS_ASSUME_NONNULL_BEGIN

@implementation TFLNLClassifierOptions
@synthesize inputTensorIndex;
@synthesize outputScoreTensorIndex;
@synthesize outputLabelTensorIndex;
@synthesize inputTensorName;
@synthesize outputScoreTensorName;
@synthesize outputLabelTensorName;
@end

@interface TFLNLClassifier ()
/** NLClassifier backed by C API */
@property(nonatomic) TfLiteNLClassifier *nlClassifier;
@end

@implementation TFLNLClassifier

- (void)dealloc {
  TfLiteNLClassifierDelete(_nlClassifier);
}

+ (instancetype)nlClassifierWithModelPath:(NSString *)modelPath
                                  options:(TFLNLClassifierOptions *)options {
  TfLiteNLClassifierOptions cOptions = {
      .input_tensor_index = options.inputTensorIndex,
      .output_score_tensor_index = options.outputScoreTensorIndex,
      .output_label_tensor_index = options.outputLabelTensorIndex,
      .input_tensor_name = options.inputTensorName.UTF8String,
      .output_score_tensor_name = options.outputScoreTensorName.UTF8String,
      .output_label_tensor_name = options.outputLabelTensorName.UTF8String};
  TfLiteNLClassifier *classifier =
      TfLiteNLClassifierCreateFromOptions(modelPath.UTF8String, &cOptions);
  _GTMDevAssert(classifier, @"Failed to create NLClassifier");
  return [[TFLNLClassifier alloc] initWithNLClassifier:classifier];
}

- (instancetype)initWithNLClassifier:(TfLiteNLClassifier *)nlClassifier {
  self = [super init];
  if (self) {
    _nlClassifier = nlClassifier;
  }
  return self;
}

- (NSDictionary<NSString *, NSNumber *> *)classifyWithText:(NSString *)text {
  Categories *cCategories = TfLiteNLClassifierClassify(_nlClassifier, text.UTF8String);
  NSMutableDictionary<NSString *, NSNumber *> *ret = [NSMutableDictionary dictionary];
  for (int i = 0; i < cCategories->size; i++) {
    Category cCategory = cCategories->categories[i];
    [ret setValue:[NSNumber numberWithDouble:cCategory.score]
           forKey:[NSString stringWithUTF8String:cCategory.text]];
  }
  TfLiteNLClassifierCategoriesDelete(cCategories);
  return ret;
}
@end
NS_ASSUME_NONNULL_END
