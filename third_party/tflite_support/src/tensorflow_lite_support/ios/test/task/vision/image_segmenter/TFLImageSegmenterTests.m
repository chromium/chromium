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

#import "tensorflow_lite_support/ios/task/vision/sources/TFLImageSegmenter.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+Utils.h"

#define VerifyColoredLabel(coloredLabel, expectedR, expectedG, expectedB, expectedLabel) \
  XCTAssertEqual(coloredLabel.r, expectedR);                                             \
  XCTAssertEqual(coloredLabel.g, expectedG);                                             \
  XCTAssertEqual(coloredLabel.b, expectedB);                                             \
  XCTAssertEqualObjects(coloredLabel.label, expectedLabel)

// The maximum fraction of pixels in the candidate mask that can have a
// different class than the golden mask for the test to pass.
float const kGoldenMaskTolerance = 1e-2;

// Magnification factor used when creating the golden category masks to make
// them more human-friendly. Each pixel in the golden masks has its value
// multiplied by this factor, i.e. a value of 10 means class index 1, a value of
// 20 means class index 2, etc.
NSInteger const kGoldenMaskMagnificationFactor = 10;

NSInteger const deepLabV3SegmentationWidth = 257;

NSInteger const deepLabV3SegmentationHeight = 257;

@interface TFLImageSegmenterTests : XCTestCase

@property(nonatomic, nullable) NSString *modelPath;

@end

@implementation TFLImageSegmenterTests

- (void)setUp {
  // Put setup code here. This method is called before the invocation of each test method in the
  // class.
  [super setUp];
  self.modelPath = [[NSBundle bundleForClass:self.class] pathForResource:@"deeplabv3"
                                                                  ofType:@"tflite"];
  XCTAssertNotNil(self.modelPath);
}

- (void)compareWithDeepLabV3PartialColoredLabels:(NSArray<TFLColoredLabel *> *)coloredLabels {
  VerifyColoredLabel(coloredLabels[0],
                     0,               // expectedR
                     0,               // expectedG
                     0,               // expectedB
                     @"background");  // expectedLabel

  VerifyColoredLabel(coloredLabels[1],
                     128,          // expectedR
                     0,            // expectedG
                     0,            // expectedB
                     @"aeroplane"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[2],
                     0,          // expectedR
                     128,        // expectedG
                     0,          // expectedB
                     @"bicycle"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[3],
                     128,     // expectedR
                     128,     // expectedG
                     0,       // expectedB
                     @"bird"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[4],
                     0,       // expectedR
                     0,       // expectedG
                     128,     // expectedB
                     @"boat"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[5],
                     128,       // expectedR
                     0,         // expectedG
                     128,       // expectedB
                     @"bottle"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[6],
                     0,      // expectedR
                     128,    // expectedG
                     128,    // expectedB
                     @"bus"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[7],
                     128,    // expectedR
                     128,    // expectedG
                     128,    // expectedB
                     @"car"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[8],
                     64,     // expectedR
                     0,      // expectedG
                     0,      // expectedB
                     @"cat"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[9],
                     192,      // expectedR
                     0,        // expectedG
                     0,        // expectedB
                     @"chair"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[10],
                     64,     // expectedR
                     128,    // expectedG
                     0,      // expectedB
                     @"cow"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[11],
                     192,             // expectedR
                     128,             // expectedG
                     0,               // expectedB
                     @"dining table"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[12],
                     64,     // expectedR
                     0,      // expectedG
                     128,    // expectedB
                     @"dog"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[13],
                     192,      // expectedR
                     0,        // expectedG
                     128,      // expectedB
                     @"horse"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[14],
                     64,           // expectedR
                     128,          // expectedG
                     128,          // expectedB
                     @"motorbike"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[15],
                     192,       // expectedR
                     128,       // expectedG
                     128,       // expectedB
                     @"person"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[16],
                     0,               // expectedR
                     64,              // expectedG
                     0,               // expectedB
                     @"potted plant"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[17],
                     128,      // expectedR
                     64,       // expectedG
                     0,        // expectedB
                     @"sheep"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[18],
                     0,       // expectedR
                     192,     // expectedG
                     0,       // expectedB
                     @"sofa"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[19],
                     128,      // expectedR
                     192,      // expectedG
                     0,        // expectedB
                     @"train"  // expectedLabel
  );

  VerifyColoredLabel(coloredLabels[20],
                     0,     // expectedR
                     64,    // expectedG
                     128,   // expectedB
                     @"tv"  // expectedLabel
  );
}

- (void)testSuccessfulImageSegmentationWithCategoryMask {
  TFLImageSegmenterOptions *imageSegmenterOptions =
      [[TFLImageSegmenterOptions alloc] initWithModelPath:self.modelPath];

  TFLImageSegmenter *imageSegmenter =
      [TFLImageSegmenter imageSegmenterWithOptions:imageSegmenterOptions error:nil];
  XCTAssertNotNil(imageSegmenter);

  GMLImage *gmlImage = [GMLImage imageFromBundleWithClass:self.class
                                                 fileName:@"segmentation_input_rotation0"
                                                   ofType:@"jpg"];
  XCTAssertNotNil(gmlImage);

  TFLSegmentationResult *segmentationResult = [imageSegmenter segmentWithGMLImage:gmlImage
                                                                            error:nil];

  XCTAssertNotNil(segmentationResult);
  XCTAssertEqual(segmentationResult.segmentations.count, 1);

  XCTAssertNotNil(segmentationResult.segmentations[0].coloredLabels);
  [self compareWithDeepLabV3PartialColoredLabels:segmentationResult.segmentations[0].coloredLabels];

  XCTAssertNotNil(segmentationResult.segmentations[0].categoryMask);
  XCTAssertTrue(segmentationResult.segmentations[0].categoryMask.mask != nil);

  GMLImage *goldenImage = [GMLImage imageFromBundleWithClass:self.class
                                                    fileName:@"segmentation_golden_rotation0"
                                                      ofType:@"png"];

  XCTAssertNotNil(goldenImage);
  CVPixelBufferRef pixelBuffer = [goldenImage grayScalePixelBuffer];

  CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

  UInt8 *pixelBufferBaseAddress = (UInt8 *)CVPixelBufferGetBaseAddress(pixelBuffer);

  XCTAssertEqual(deepLabV3SegmentationWidth,
                 segmentationResult.segmentations[0].categoryMask.width);
  XCTAssertEqual(deepLabV3SegmentationHeight,
                 segmentationResult.segmentations[0].categoryMask.height);

  NSInteger numPixels = deepLabV3SegmentationWidth * deepLabV3SegmentationHeight;

  float inconsistentPixels = 0;

  for (int i = 0; i < numPixels; i++)
    if (segmentationResult.segmentations[0].categoryMask.mask[i] * kGoldenMaskMagnificationFactor !=
        pixelBufferBaseAddress[i])
      inconsistentPixels += 1;

  CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

  XCTAssertLessThan(inconsistentPixels / (float)numPixels, kGoldenMaskTolerance);
}

@end
