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
#import "tensorflow_lite_support/ios/task/text/qa/Sources/TFLBertQuestionAnswerer.h"
#import "GTMDefines.h"
#include "tensorflow_lite_support/c/task/text/bert_question_answerer.h"

NS_ASSUME_NONNULL_BEGIN

@implementation TFLQAAnswer
@synthesize pos;
@synthesize text;
@end

@interface TFLBertQuestionAnswerer ()
/** BertQuestionAnswerer backed by C API */
@property(nonatomic) TfLiteBertQuestionAnswerer *bertQuestionAnswerer;
@end

@implementation TFLBertQuestionAnswerer

- (void)dealloc {
  TfLiteBertQuestionAnswererDelete(_bertQuestionAnswerer);
}

+ (instancetype)questionAnswererWithModelPath:(NSString *)modelPath {
  TfLiteBertQuestionAnswerer *bert_qa = TfLiteBertQuestionAnswererCreate(modelPath.UTF8String);

  _GTMDevAssert(bert_qa, @"Failed to create BertQuestionAnswerer");
  return [[TFLBertQuestionAnswerer alloc] initWithBertQuestionAnswerer:bert_qa];
}

- (instancetype)initWithBertQuestionAnswerer:(TfLiteBertQuestionAnswerer *)bertQuestionAnswerer {
  self = [super init];
  if (self) {
    _bertQuestionAnswerer = bertQuestionAnswerer;
  }
  return self;
}

- (NSArray<TFLQAAnswer *> *)answerWithContext:(NSString *)context question:(NSString *)question {
  TfLiteQaAnswers *cAnswers = TfLiteBertQuestionAnswererAnswer(
      _bertQuestionAnswerer, context.UTF8String, question.UTF8String);
  NSMutableArray<TFLQAAnswer *> *ret = [NSMutableArray arrayWithCapacity:cAnswers->size];
  for (int i = 0; i < cAnswers->size; i++) {
    TfLiteQaAnswer cAnswer = cAnswers->answers[i];
    TFLQAAnswer *answer = [[TFLQAAnswer alloc] init];
    struct TFLPos pos = {.start = cAnswer.start, .end = cAnswer.end, .logit = cAnswer.logit};
    [answer setPos:pos];
    [answer setText:[NSString stringWithUTF8String:cAnswer.text]];
    [ret addObject:answer];
  }
  TfLiteQaAnswersDelete(cAnswers);
  return ret;
}
@end
NS_ASSUME_NONNULL_END
