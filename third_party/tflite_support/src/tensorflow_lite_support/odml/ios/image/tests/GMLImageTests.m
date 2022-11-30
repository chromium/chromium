// Copyright 2021 Google LLC. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "tensorflow_lite_support/odml/ios/image/apis/GMLImage.h"

#import <Accelerate/Accelerate.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <XCTest/XCTest.h>

NS_ASSUME_NONNULL_BEGIN

static NSString *const kTestImageName = @"grace_hopper";
static NSString *const kTestImageType = @"jpg";
static CGFloat kTestImageWidthInPixels = 517.0f;
static CGFloat kTestImageHeightInPixels = 606.0f;

/** Unit tests for `GMLImage`. */
@interface GMLImageTests : XCTestCase

/** Test image. */
@property(nonatomic, nullable) UIImage *image;

@end

@implementation GMLImageTests

#pragma mark - Tests

- (void)setUp {
  [super setUp];
  NSString *imageName = [[NSBundle bundleForClass:[self class]] pathForResource:kTestImageName
                                                                         ofType:kTestImageType];
  self.image = [[UIImage alloc] initWithContentsOfFile:imageName];
}

- (void)tearDown {
  self.image = nil;
  [super tearDown];
}

- (void)testInitWithImage {
  GMLImage *mlImage = [[GMLImage alloc] initWithImage:self.image];
  XCTAssertNotNil(mlImage);
  XCTAssertEqual(mlImage.imageSourceType, GMLImageSourceTypeImage);
  XCTAssertEqual(mlImage.orientation, self.image.imageOrientation);
  mlImage.orientation = UIImageOrientationDown;
  XCTAssertEqual(mlImage.orientation, UIImageOrientationDown);
  XCTAssertEqualWithAccuracy(mlImage.width, kTestImageWidthInPixels, FLT_EPSILON);
  XCTAssertEqualWithAccuracy(mlImage.height, kTestImageHeightInPixels, FLT_EPSILON);
}

- (void)testInitWithImage_nilImage {
  GMLImage *mlImage = [[GMLImage alloc] initWithImage:nil];
  XCTAssertNil(mlImage);
}

- (void)testInitWithSampleBuffer {
  CMSampleBufferRef sampleBuffer = [self sampleBuffer];
  GMLImage *mlImage = [[GMLImage alloc] initWithSampleBuffer:sampleBuffer];
  XCTAssertNotNil(mlImage);
  XCTAssertEqual(mlImage.imageSourceType, GMLImageSourceTypeSampleBuffer);
  XCTAssertEqual(mlImage.orientation, UIImageOrientationUp);
  mlImage.orientation = UIImageOrientationDown;
  XCTAssertEqual(mlImage.orientation, UIImageOrientationDown);
  XCTAssertEqualWithAccuracy(mlImage.width, kTestImageWidthInPixels, FLT_EPSILON);
  XCTAssertEqualWithAccuracy(mlImage.height, kTestImageHeightInPixels, FLT_EPSILON);
}

- (void)testInitWithSampleBuffer_nilImage {
  GMLImage *mlImage = [[GMLImage alloc] initWithSampleBuffer:nil];
  XCTAssertNil(mlImage);
}

- (void)testInitWithPixelBuffer {
  CMSampleBufferRef sampleBuffer = [self sampleBuffer];
  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  GMLImage *mlImage = [[GMLImage alloc] initWithPixelBuffer:pixelBuffer];
  XCTAssertNotNil(mlImage);
  XCTAssertEqual(mlImage.imageSourceType, GMLImageSourceTypePixelBuffer);
  XCTAssertEqual(mlImage.orientation, UIImageOrientationUp);
  mlImage.orientation = UIImageOrientationDown;
  XCTAssertEqual(mlImage.orientation, UIImageOrientationDown);
  XCTAssertEqualWithAccuracy(mlImage.width, kTestImageWidthInPixels, FLT_EPSILON);
  XCTAssertEqualWithAccuracy(mlImage.height, kTestImageHeightInPixels, FLT_EPSILON);
}

- (void)testInitWithPixelBuffer_nilImage {
  GMLImage *mlImage = [[GMLImage alloc] initWithPixelBuffer:nil];
  XCTAssertNil(mlImage);
}

#pragma mark - Private

/**
 * Converts the input image in RGBA space into a `CMSampleBuffer`.
 *
 * @return `CMSampleBuffer` converted from the given `UIImage`.
 */
- (CMSampleBufferRef)sampleBuffer {
  // Rotate the image and convert from RGBA to BGRA.
  CGImageRef CGImage = self.image.CGImage;
  size_t width = CGImageGetWidth(CGImage);
  size_t height = CGImageGetHeight(CGImage);
  size_t bpr = CGImageGetBytesPerRow(CGImage);

  CGDataProviderRef provider = CGImageGetDataProvider(CGImage);
  NSData *imageRGBAData = (id)CFBridgingRelease(CGDataProviderCopyData(provider));
  const uint8_t order[4] = {2, 1, 0, 3};

  NSData *imageBGRAData = nil;
  unsigned char *bgraPixel = (unsigned char *)malloc([imageRGBAData length]);
  if (bgraPixel) {
    vImage_Buffer src;
    src.height = height;
    src.width = width;
    src.rowBytes = bpr;
    src.data = (void *)[imageRGBAData bytes];

    vImage_Buffer dest;
    dest.height = height;
    dest.width = width;
    dest.rowBytes = bpr;
    dest.data = bgraPixel;

    // Specify ordering changes in map.
    vImage_Error error = vImagePermuteChannels_ARGB8888(&src, &dest, order, kvImageNoFlags);

    // Package the result.
    if (error == kvImageNoError) {
      imageBGRAData = [NSData dataWithBytes:bgraPixel length:[imageRGBAData length]];
    }

    // Memory cleanup.
    free(bgraPixel);
  }

  if (imageBGRAData == nil) {
    XCTFail(@"Failed to convert input image.");
  }

  // Write data to `CMSampleBuffer`.
  NSDictionary *options = @{
    (__bridge NSString *)kCVPixelBufferCGImageCompatibilityKey : @(YES),
    (__bridge NSString *)kCVPixelBufferCGBitmapContextCompatibilityKey : @(YES)
  };
  CVPixelBufferRef pixelBuffer;
  CVReturn status = CVPixelBufferCreateWithBytes(
      kCFAllocatorDefault, width, height, kCVPixelFormatType_32BGRA, (void *)[imageBGRAData bytes],
      bpr, NULL, nil, (__bridge CFDictionaryRef)options, &pixelBuffer);

  if (status != kCVReturnSuccess) {
    XCTFail(@"Failed to create pixel buffer.");
  }

  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  CMVideoFormatDescriptionRef videoInfo = NULL;
  CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer, &videoInfo);

  CMSampleBufferRef buffer;
  CMSampleBufferCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer, true, NULL, NULL, videoInfo,
                                     &kCMTimingInfoInvalid, &buffer);

  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

  return buffer;
}

@end

NS_ASSUME_NONNULL_END
