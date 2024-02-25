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
#import <XCTest/XCTest.h>

#import "tensorflow_lite_support/ios/task/vision/sources/TFLImageClassifier.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+Utils.h"

NS_ASSUME_NONNULL_BEGIN

@interface TFLImageClassifierTests : XCTestCase
@property(nonatomic, nullable) NSString *modelPath;
@end

@implementation TFLImageClassifierTests

- (void)setUp {
  // Put setup code here. This method is called before the invocation of each test method in the
  // class.
  [super setUp];
  self.modelPath = [[NSBundle bundleForClass:self.class] pathForResource:@"mobilenet_v2_1.0_224"
                                                                  ofType:@"tflite"];
  XCTAssertNotNil(self.modelPath);
}

- (void)testSuccessfullImageInferenceOnMLImageWithUIImage {
  TFLImageClassifierOptions *imageClassifierOptions =
      [[TFLImageClassifierOptions alloc] initWithModelPath:self.modelPath];

  TFLImageClassifier *imageClassifier =
      [TFLImageClassifier imageClassifierWithOptions:imageClassifierOptions error:nil];
  XCTAssertNotNil(imageClassifier);

  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"burger" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  TFLClassificationResult *classificationResults = [imageClassifier classifyWithGMLImage:gmlImage
                                                                                   error:nil];
  XCTAssertTrue(classificationResults.classifications.count > 0);
  XCTAssertTrue(classificationResults.classifications[0].categories.count > 0);

  TFLCategory *category = classificationResults.classifications[0].categories[0];
  XCTAssertTrue([category.label isEqual:@"cheeseburger"]);
  // TODO: match the score as image_classifier_test.cc
  XCTAssertEqualWithAccuracy(category.score, 0.748976, 0.001);
}

- (void)testModelOptionsWithMaxResults {
  TFLImageClassifierOptions *imageClassifierOptions =
      [[TFLImageClassifierOptions alloc] initWithModelPath:self.modelPath];
  int maxResults = 3;
  imageClassifierOptions.classificationOptions.maxResults = maxResults;

  TFLImageClassifier *imageClassifier =
      [TFLImageClassifier imageClassifierWithOptions:imageClassifierOptions error:nil];
  XCTAssertNotNil(imageClassifier);

  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"burger" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  TFLClassificationResult *classificationResults = [imageClassifier classifyWithGMLImage:gmlImage
                                                                                   error:nil];
  XCTAssertTrue(classificationResults.classifications.count > 0);
  XCTAssertLessThanOrEqual(classificationResults.classifications[0].categories.count, maxResults);

  TFLCategory *category = classificationResults.classifications[0].categories[0];
  XCTAssertTrue([category.label isEqual:@"cheeseburger"]);
  // TODO: match the score as image_classifier_test.cc
  XCTAssertEqualWithAccuracy(category.score, 0.748976, 0.001);
}

- (void)testInferenceWithBoundingBox {
  TFLImageClassifierOptions *imageClassifierOptions =
      [[TFLImageClassifierOptions alloc] initWithModelPath:self.modelPath];
  int maxResults = 3;
  imageClassifierOptions.classificationOptions.maxResults = maxResults;

  TFLImageClassifier *imageClassifier =
      [TFLImageClassifier imageClassifierWithOptions:imageClassifierOptions error:nil];
  XCTAssertNotNil(imageClassifier);

  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"multi_objects" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  CGRect roi = CGRectMake(406, 110, 148, 153);
  TFLClassificationResult *classificationResults = [imageClassifier classifyWithGMLImage:gmlImage
                                                                        regionOfInterest:roi
                                                                                   error:nil];
  XCTAssertTrue(classificationResults.classifications.count > 0);
  XCTAssertTrue(classificationResults.classifications[0].categories.count > 0);

  // TODO: match the label and score as image_classifier_test.cc
  // TFLCategory *__unused category = classificationResults.classifications[0].categories[0];
  // XCTAssertTrue([category.label isEqual:@"soccer ball"]);
  // XCTAssertEqualWithAccuracy(category.score, 0.256512, 0.001);
}

- (void)testInferenceWithRGBAImage {
  TFLImageClassifierOptions *imageClassifierOptions =
      [[TFLImageClassifierOptions alloc] initWithModelPath:self.modelPath];

  TFLImageClassifier *imageClassifier =
      [TFLImageClassifier imageClassifierWithOptions:imageClassifierOptions error:nil];
  XCTAssertNotNil(imageClassifier);

  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"sparrow" ofType:@"png"];
  XCTAssertNotNil(gmlImage);

  TFLClassificationResult *classificationResults = [imageClassifier classifyWithGMLImage:gmlImage
                                                                                   error:nil];
  XCTAssertTrue(classificationResults.classifications.count > 0);
  XCTAssertTrue(classificationResults.classifications[0].categories.count > 0);

  TFLCategory *category = classificationResults.classifications[0].categories[0];
  XCTAssertTrue([category.label isEqual:@"junco"]);
  // TODO: inspect if score is correct. Better to test againest "burger", because we know the
  // expected result for "burger.jpg".
  XCTAssertEqualWithAccuracy(category.score, 0.253016, 0.001);
}

@end

NS_ASSUME_NONNULL_END
