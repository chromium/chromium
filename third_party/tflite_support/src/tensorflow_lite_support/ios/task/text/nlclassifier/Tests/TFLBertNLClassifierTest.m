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

#import <XCTest/XCTest.h>

NS_ASSUME_NONNULL_BEGIN

@interface TFLBertNLClassifierTest : XCTestCase
@property(nonatomic, nullable) NSString *bertModelPath;
@property(nonatomic, nullable) TFLBertNLClassifierOptions *modelOptions;
@end

@implementation TFLBertNLClassifierTest
#pragma mark - Tests

- (void)setUp {
  [super setUp];
  NSBundle *bundle = [NSBundle bundleForClass:[self class]];
  self.bertModelPath = [bundle pathForResource:@"bert_nl_classifier" ofType:@"tflite"];
}

- (void)testClassifyPositiveResult {
  TFLBertNLClassifier *bertNLClassifier =
      [TFLBertNLClassifier bertNLClassifierWithModelPath:self.bertModelPath];

  XCTAssertNotNil(bertNLClassifier);

  NSDictionary<NSString *, NSNumber *> *categories =
      [bertNLClassifier classifyWithText:@"it's a charming and often affecting journey"];

  XCTAssertGreaterThan([categories[@"positive"] doubleValue],
                       [categories[@"negative"] doubleValue]);
}

- (void)testClassifyNegativeResult {
  TFLBertNLClassifier *bertNLClassifier =
      [TFLBertNLClassifier bertNLClassifierWithModelPath:self.bertModelPath];

  XCTAssertNotNil(bertNLClassifier);

  NSDictionary<NSString *, NSNumber *> *categories =
      [bertNLClassifier classifyWithText:@"unflinchingly bleak and desperate"];

  XCTAssertGreaterThan([categories[@"negative"] doubleValue],
                       [categories[@"positive"] doubleValue]);
}

- (void)testCreateFromOptionsClassifyPositiveResult {
  self.modelOptions = [[TFLBertNLClassifierOptions alloc] init];
  [self.modelOptions setMaxSeqLen:128];

  TFLBertNLClassifier *bertNLClassifier =
      [TFLBertNLClassifier bertNLClassifierWithModelPath:self.bertModelPath
                                                 options:self.modelOptions];

  XCTAssertNotNil(bertNLClassifier);

  NSDictionary<NSString *, NSNumber *> *categories =
      [bertNLClassifier classifyWithText:@"it's a charming and often affecting journey"];

  XCTAssertGreaterThan([categories[@"positive"] doubleValue],
                       [categories[@"negative"] doubleValue]);
}

- (void)testCreateFromOptionsClassifyNegativeResult {
  self.modelOptions = [[TFLBertNLClassifierOptions alloc] init];
  [self.modelOptions setMaxSeqLen:128];

  TFLBertNLClassifier *bertNLClassifier =
      [TFLBertNLClassifier bertNLClassifierWithModelPath:self.bertModelPath
                                                 options:self.modelOptions];

  XCTAssertNotNil(bertNLClassifier);

  NSDictionary<NSString *, NSNumber *> *categories =
      [bertNLClassifier classifyWithText:@"unflinchingly bleak and desperate"];

  XCTAssertGreaterThan([categories[@"negative"] doubleValue],
                       [categories[@"positive"] doubleValue]);
}
@end
NS_ASSUME_NONNULL_END
