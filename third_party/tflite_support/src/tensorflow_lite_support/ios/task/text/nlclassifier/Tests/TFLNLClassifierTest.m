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

#import <XCTest/XCTest.h>

NS_ASSUME_NONNULL_BEGIN

@interface TFLNLClassifierTest : XCTestCase
@property(nonatomic, nullable) NSString *modelPath;
@property(nonatomic, nullable) TFLNLClassifierOptions *modelOptions;
@end

@implementation TFLNLClassifierTest
#pragma mark - Tests

- (void)setUp {
  [super setUp];
  NSBundle *bundle = [NSBundle bundleForClass:[self class]];
  self.modelPath = [bundle pathForResource:@"test_model_nl_classifier_with_regex_tokenizer"
                                    ofType:@"tflite"];
  self.modelOptions = [[TFLNLClassifierOptions alloc] init];
  [self.modelOptions setInputTensorName:@"input_text"];
  [self.modelOptions setOutputScoreTensorName:@"probability"];
}

- (void)testClassifyPositiveResult {
  TFLNLClassifier *nlClassifier = [TFLNLClassifier nlClassifierWithModelPath:self.modelPath
                                                                     options:self.modelOptions];

  XCTAssertNotNil(nlClassifier);

  NSDictionary<NSString *, NSNumber *> *categories = [nlClassifier
      classifyWithText:@"This is the best movie I’ve seen in recent years. Strongly recommend it!"];

  XCTAssertGreaterThan([categories[@"Positive"] doubleValue],
                       [categories[@"Negative"] doubleValue]);
}

- (void)testClassifyNegativeResult {
  TFLNLClassifier *nlClassifier = [TFLNLClassifier nlClassifierWithModelPath:self.modelPath
                                                                     options:self.modelOptions];

  XCTAssertNotNil(nlClassifier);

  NSDictionary<NSString *, NSNumber *> *categories =
      [nlClassifier classifyWithText:@"What a waste of my time."];

  XCTAssertGreaterThan([categories[@"Negative"] doubleValue],
                       [categories[@"Positive"] doubleValue]);
}
@end
NS_ASSUME_NONNULL_END
