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
#import "tensorflow_lite_support/ios/task/text/nlclassifier/Sources/TFLBertNLClassifier.h"
#import "GTMDefines.h"
#include "tensorflow_lite_support/c/task/text/bert_nl_classifier.h"
#include "tensorflow_lite_support/c/task/text/nl_classifier_common.h"

NS_ASSUME_NONNULL_BEGIN

@implementation TFLBertNLClassifierOptions
@synthesize maxSeqLen;
@end

@interface TFLBertNLClassifier ()
/** BertNLClassifier backed by C API */
@property(nonatomic) TfLiteBertNLClassifier *bertNLClassifier;
@end

@implementation TFLBertNLClassifier

- (void)dealloc {
  TfLiteBertNLClassifierDelete(_bertNLClassifier);
}

+ (instancetype)bertNLClassifierWithModelPath:(NSString *)modelPath {
  TfLiteBertNLClassifier *classifier = TfLiteBertNLClassifierCreate(modelPath.UTF8String);

  _GTMDevAssert(classifier, @"Failed to create BertNLClassifier");
  return [[TFLBertNLClassifier alloc] initWithBertNLClassifier:classifier];
}

+ (instancetype)bertNLClassifierWithModelPath:(NSString *)modelPath
                                  options:(TFLBertNLClassifierOptions *)options {
  // Note that maxSeqLen has been deprecated. Passing it to the C API is a no-op.
  TfLiteBertNLClassifierOptions cOptions = {.max_seq_len = options.maxSeqLen};
  TfLiteBertNLClassifier *classifier =
      TfLiteBertNLClassifierCreateFromOptions(modelPath.UTF8String, &cOptions);
  _GTMDevAssert(classifier, @"Failed to create BertNLClassifier");
  return [[TFLBertNLClassifier alloc] initWithBertNLClassifier:classifier];
}

- (instancetype)initWithBertNLClassifier:(TfLiteBertNLClassifier *)bertNLClassifier {
  self = [super init];
  if (self) {
    _bertNLClassifier = bertNLClassifier;
  }
  return self;
}

- (NSDictionary<NSString *, NSNumber *> *)classifyWithText:(NSString *)text {
  Categories *cCategories = TfLiteBertNLClassifierClassify(_bertNLClassifier, text.UTF8String);
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
