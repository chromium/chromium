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

NS_ASSUME_NONNULL_BEGIN

@implementation GMLImage

#pragma mark - Public

- (nullable instancetype)initWithImage:(UIImage *)image {
  if (image.CGImage == NULL) {
    return nil;
  }

  self = [super init];
  if (self != nil) {
    _imageSourceType = GMLImageSourceTypeImage;
    _orientation = image.imageOrientation;
    _image = image;
    _width = image.size.width * image.scale;
    _height = image.size.height * image.scale;
  }
  return self;
}

- (nullable instancetype)initWithPixelBuffer:(CVPixelBufferRef)pixelBuffer {
  if (pixelBuffer == NULL) {
    return nil;
  }

  self = [super init];
  if (self != nil) {
    _imageSourceType = GMLImageSourceTypePixelBuffer;
    _orientation = UIImageOrientationUp;
    CVPixelBufferRetain(pixelBuffer);
    _pixelBuffer = pixelBuffer;
    _width = CVPixelBufferGetWidth(pixelBuffer);
    _height = CVPixelBufferGetHeight(pixelBuffer);
  }
  return self;
}

- (nullable instancetype)initWithSampleBuffer:(CMSampleBufferRef)sampleBuffer {
  if (!CMSampleBufferIsValid(sampleBuffer)) {
    return nil;
  }

  CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (imageBuffer == NULL) {
    return nil;
  }

  self = [super init];
  if (self != nil) {
    _imageSourceType = GMLImageSourceTypeSampleBuffer;
    _orientation = UIImageOrientationUp;
    CFRetain(sampleBuffer);
    _sampleBuffer = sampleBuffer;
    _width = CVPixelBufferGetWidth(imageBuffer);
    _height = CVPixelBufferGetHeight(imageBuffer);
  }
  return self;
}

#pragma mark - NSObject

- (void)dealloc {
  if (_sampleBuffer != NULL) {
    CFRelease(_sampleBuffer);
  }
  if (_pixelBuffer != NULL) {
    CVPixelBufferRelease(_pixelBuffer);
  }
}

@end

NS_ASSUME_NONNULL_END
