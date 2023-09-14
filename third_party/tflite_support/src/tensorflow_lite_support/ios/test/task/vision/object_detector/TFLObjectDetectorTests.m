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
#import <CoreGraphics/CoreGraphics.h>
#import <XCTest/XCTest.h>

#import "tensorflow_lite_support/ios/task/vision/sources/TFLObjectDetector.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+Utils.h"

const int kMaxPixelOffset = 5;

#define VerifyDetection(detection, expectedBoundingBox, expectedFirstScore, expectedFirstLabel,  \
                        precision)                                                               \
  XCTAssertGreaterThan(detection.categories.count, 0);                                           \
  NSLog(@"Detected %f", detection.categories[0].score);                                          \
  NSLog(@"Expected %f", expectedFirstScore);                                                     \
  XCTAssertEqualWithAccuracy(detection.boundingBox.origin.x, expectedBoundingBox.origin.x,       \
                             kMaxPixelOffset);                                                   \
  XCTAssertEqualWithAccuracy(detection.boundingBox.origin.y, expectedBoundingBox.origin.y,       \
                             kMaxPixelOffset);                                                   \
  XCTAssertEqualWithAccuracy(detection.boundingBox.size.width, expectedBoundingBox.size.width,   \
                             kMaxPixelOffset);                                                   \
  XCTAssertEqualWithAccuracy(detection.boundingBox.size.height, expectedBoundingBox.size.height, \
                             kMaxPixelOffset);                                                   \
  XCTAssertEqualObjects(detection.categories[0].label, expectedFirstLabel);                      \
  XCTAssertEqualWithAccuracy(detection.categories[0].score, expectedFirstScore, precision)

@interface TFLObjectDetectorTests : XCTestCase
@property(nonatomic, nullable) NSString *modelPath;
@end

@implementation TFLObjectDetectorTests

- (void)setUp {
  // Put setup code here. This method is called before the invocation of each test method in the
  // class.
  [super setUp];
  self.modelPath = [[NSBundle bundleForClass:self.class]
      pathForResource:@"coco_ssd_mobilenet_v1_1.0_quant_2018_06_29"
               ofType:@"tflite"];
  XCTAssertNotNil(self.modelPath);
}

- (void)verifyResults:(TFLDetectionResult *)detectionResult {
  XCTAssertGreaterThan(detectionResult.detections.count, 0);
  VerifyDetection(detectionResult.detections[0],
                  CGRectMake(54, 396, 393, 199),  // expectedBoundingBox
                  0.63,                           // expectedFirstScore
                  @"cat",                         // expectedFirstLabel
                  0.05                            // precision
  );
  VerifyDetection(detectionResult.detections[1],
                  CGRectMake(602, 157, 391, 447),  // expectedBoundingBox
                  0.60,                            // expectedFirstScore
                  @"cat",                          // expectedFirstLabel
                  0.05                             // precision
  );
  VerifyDetection(detectionResult.detections[2],
                  CGRectMake(260, 394, 179, 209),  // expectedBoundingBox
                  0.56,                            // expectedFirstScore
                  @"cat",                          // expectedFirstLabel
                  0.05                             // precision
  );
  VerifyDetection(detectionResult.detections[3],
                  CGRectMake(387, 197, 281, 409),  // expectedBoundingBox
                  0.5,                             // expectedFirstScore
                  @"dog",                          // expectedFirstLabel
                  0.05                             // precision
  );
}

- (void)testSuccessfulObjectDetectionOnMLImageWithUIImage {
  TFLObjectDetectorOptions *objectDetectorOptions =
      [[TFLObjectDetectorOptions alloc] initWithModelPath:self.modelPath];

  TFLObjectDetector *objectDetector =
      [TFLObjectDetector objectDetectorWithOptions:objectDetectorOptions error:nil];
  XCTAssertNotNil(objectDetector);

  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"cats_and_dogs" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  TFLDetectionResult *detectionResults = [objectDetector detectWithGMLImage:gmlImage error:nil];
  [self verifyResults:detectionResults];
}

- (void)testModelOptionsWithMaxResults {
  TFLObjectDetectorOptions *objectDetectorOptions =
      [[TFLObjectDetectorOptions alloc] initWithModelPath:self.modelPath];
  int maxResults = 3;
  objectDetectorOptions.classificationOptions.maxResults = maxResults;

  TFLObjectDetector *objectDetector =
      [TFLObjectDetector objectDetectorWithOptions:objectDetectorOptions error:nil];
  XCTAssertNotNil(objectDetector);

  GMLImage *gmlImage =
      [GMLImage imageFromBundleWithClass:self.class fileName:@"cats_and_dogs" ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  TFLDetectionResult *detectionResult = [objectDetector detectWithGMLImage:gmlImage error:nil];

  XCTAssertLessThanOrEqual(detectionResult.detections.count, maxResults);
  VerifyDetection(detectionResult.detections[0],
                  CGRectMake(54, 396, 393, 199),  // expectedBoundingBox
                  0.63,                           // expectedFirstScore
                  @"cat",                         // expectedFirstLabel
                  0.05                            // precision
  );
  VerifyDetection(detectionResult.detections[1],
                  CGRectMake(602, 157, 391, 447),  // expectedBoundingBox
                  0.6,                             // expectedFirstScore
                  @"cat",                          // expectedFirstLabel
                  0.05                             // precision
  );
  VerifyDetection(detectionResult.detections[2],
                  CGRectMake(260, 394, 179, 209),  // expectedBoundingBox
                  0.56,                            // expectedFirstScore
                  @"cat",                          // expectedFirstLabel
                  0.05                             // precision
  );
}

@end
