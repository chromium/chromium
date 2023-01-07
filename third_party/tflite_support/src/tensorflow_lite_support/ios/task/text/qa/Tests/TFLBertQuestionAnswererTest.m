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

#import <XCTest/XCTest.h>

static NSString *const kContext =
    @"The role of teacher is often formal and ongoing, carried out at a school "
     "or other place of formal education. In many countries, a person who "
     "wishes to become a teacher must first obtain specified professional "
     "qualifications or credentials from a university or college. These "
     "professional qualifications may include the study of pedagogy, the "
     "science of teaching. Teachers, like other professionals, may have to "
     "continue their education after they qualify, a process known as "
     "continuing professional development. Teachers may use a lesson plan to "
     "facilitate student learning, providing a course of study which is called "
     "the curriculum.";
static NSString *const kQuestion = @"What is a course of study called?";
static NSString *const kAnswer = @"the curriculum.";

@interface TFLBertQuestionAnswererTest : XCTestCase
@property(nonatomic, nullable) NSString *mobileBertModelPath;
@property(nonatomic, nullable) NSString *albertModelPath;
@end

@implementation TFLBertQuestionAnswererTest
#pragma mark - Tests

- (void)setUp {
  [super setUp];
  NSBundle *bundle = [NSBundle bundleForClass:[self class]];
  self.mobileBertModelPath = [bundle pathForResource:@"mobilebert_with_metadata" ofType:@"tflite"];
  self.albertModelPath = [bundle pathForResource:@"albert_with_metadata" ofType:@"tflite"];
}

- (void)testInitMobileBert {
  TFLBertQuestionAnswerer* mobileBertAnswerer =
    [TFLBertQuestionAnswerer questionAnswererWithModelPath:self.mobileBertModelPath];

  XCTAssertNotNil(mobileBertAnswerer);

  NSArray<TFLQAAnswer*>* answers =
    [mobileBertAnswerer answerWithContext:kContext question:kQuestion];

  XCTAssertEqualObjects([answers[0] text], kAnswer);
}

- (void)testInitAlbert {
  TFLBertQuestionAnswerer* albertAnswerer =
    [TFLBertQuestionAnswerer questionAnswererWithModelPath:self.albertModelPath];

  XCTAssertNotNil(albertAnswerer);

  NSArray<TFLQAAnswer*>* answers =
    [albertAnswerer answerWithContext:kContext question:kQuestion];


  XCTAssertEqualObjects([answers[0] text], kAnswer);
}
@end
