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
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImageUtils.h"
#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"

#include "tensorflow_lite_support/c/task/vision/core/frame_buffer.h"

#import <Accelerate/Accelerate.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreImage/CoreImage.h>
#import <CoreVideo/CoreVideo.h>

@interface TFLCVPixelBufferUtils : NSObject
+ (uint8_t *_Nullable)convertBGRAtoRGBforPixelBufferBaseAddress:(CVPixelBufferRef)pixelBuffer
                                                          error:(NSError **)error;
@end

@interface UIImage (RawPixelDataUtils)
- (TfLiteFrameBuffer *)frameBufferWithError:(NSError **)error;
@end

@implementation TFLCVPixelBufferUtils

+ (uint8_t *)convertBGRAtoRGBforPixelBufferBaseAddress:(CVPixelBufferRef)pixelBuffer
                                                 error:(NSError **)error {
  size_t width = CVPixelBufferGetWidth(pixelBuffer);
  size_t height = CVPixelBufferGetHeight(pixelBuffer);
  size_t stride = CVPixelBufferGetBytesPerRow(pixelBuffer);

  int destinationChannelCount = 3;
  size_t destinationBytesPerRow = destinationChannelCount * width;

  uint8_t *pixelBufferBaseAddress = (uint8_t *)CVPixelBufferGetBaseAddress(pixelBuffer);

  uint8_t *destPixelBufferAddress = [TFLCommonUtils mallocWithSize:height * destinationBytesPerRow
                                                             error:error];

  if (!destPixelBufferAddress) {
    return NULL;
  }

  vImage_Buffer srcBuffer = {
      .data = pixelBufferBaseAddress, .height = height, .width = width, .rowBytes = stride};

  vImage_Buffer destBuffer = {.data = destPixelBufferAddress,
                              .height = height,
                              .width = width,
                              .rowBytes = destinationBytesPerRow};

  vImage_Error convertError = kvImageNoError;
  convertError = vImageConvert_BGRA8888toRGB888(&srcBuffer, &destBuffer, kvImageNoFlags);

  if (convertError != kvImageNoError) {
    if (error) {
      *error = [TFLCommonUtils customErrorWithCode:TFLSupportErrorCodeImageProcessingError
                                       description:@"Image format conversion failed."];
    }

    return NULL;
  }

  return destPixelBufferAddress;
}

@end

@implementation UIImage (RawPixelDataUtils)

- (TfLiteFrameBuffer *)frameBufferWithError:(NSError **)error {
  TfLiteFrameBuffer *frameBuffer = NULL;

  if (self.CGImage) {
    frameBuffer = [self frameBufferFromCGImage:self.CGImage error:error];
  } else if (self.CIImage) {
    frameBuffer = [self frameBufferFromCIImage:self.CIImage error:error];
  } else if (error) {
    *error = [TFLCommonUtils customErrorWithCode:TFLSupportErrorCodeInvalidArgumentError
                                     description:@"UIImage should be initialized from"
                                                  " CIImage or CGImage."];
  }

  return frameBuffer;
}

+ (UInt8 *_Nullable)pixelDataFromCGImage:(CGImageRef)cgImage error:(NSError **)error {
  long width = CGImageGetWidth(cgImage);
  long height = CGImageGetHeight(cgImage);

  int bitsPerComponent = 8;
  UInt8 *buffer_to_return = NULL;

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(nil, width, height, bitsPerComponent, 0, colorSpace,
                                               kCGImageAlphaNoneSkipLast);

  if (context) {
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);
    buffer_to_return =
        [UIImage populateRGBBufferFromSourceRGBABuffer:CGBitmapContextGetData(context)
                                                 width:width
                                                height:height];
    CGContextRelease(context);
  }

  if ((buffer_to_return == NULL) && (error)) {
    *error = [TFLCommonUtils customErrorWithCode:TFLSupportErrorCodeImageProcessingError
                                     description:@"Image format conversion failed."];
  }

  CGColorSpaceRelease(colorSpace);

  return buffer_to_return;
}

+ (nullable UInt8 *)populateRGBBufferFromSourceRGBABuffer:(UInt8 *)buffer
                                                    width:(size_t)width
                                                   height:(size_t)height {
  if (!buffer) return nil;

  int sourceChannelCount = 4;
  int destChannelCount = 3;

  UInt8 *buffer_to_return = malloc(height * destChannelCount * width);
  if (!buffer_to_return) {
    return nil;
  }
  for (int row = 0; row < width; row++) {
    for (int col = 0; col < height; col++) {
      long offset = sourceChannelCount * (col * width + row);
      long rgbOffset = destChannelCount * (col * width + row);
      buffer_to_return[rgbOffset] = buffer[offset];
      buffer_to_return[rgbOffset + 1] = buffer[offset + 1];
      buffer_to_return[rgbOffset + 2] = buffer[offset + 2];
    }
  }
  return buffer_to_return;
}

- (TfLiteFrameBuffer *)frameBufferFromCGImage:(CGImageRef)cgImage error:(NSError **)error {
  UInt8 *buffer = [UIImage pixelDataFromCGImage:cgImage error:error];

  if (buffer == NULL) {
    return NULL;
  }

  TfLiteFrameBuffer *cFrameBuffer = malloc(sizeof(TfLiteFrameBuffer));

  cFrameBuffer->dimension.width = (int)CGImageGetWidth(cgImage);
  cFrameBuffer->dimension.height = (int)CGImageGetHeight(cgImage);
  cFrameBuffer->buffer = buffer;

  enum TfLiteFrameBufferFormat cPixelFormat = kRGB;
  cFrameBuffer->format = cPixelFormat;

  return cFrameBuffer;
}

- (TfLiteFrameBuffer *)frameBufferFromCIImage:(CIImage *)ciImage error:(NSError **)error {
  uint8_t *buffer = nil;

  int width = 0;
  int height = 0;
  if (ciImage.pixelBuffer) {
    buffer = [TFLCVPixelBufferUtils convertBGRAtoRGBforPixelBufferBaseAddress:ciImage.pixelBuffer
                                                                        error:error];
    width = (int)CVPixelBufferGetWidth(ciImage.pixelBuffer);
    height = (int)CVPixelBufferGetHeight(ciImage.pixelBuffer);

  } else if (ciImage.CGImage) {
    buffer = [UIImage pixelDataFromCGImage:ciImage.CGImage error:error];
    width = (int)CGImageGetWidth(ciImage.CGImage);
    height = (int)CGImageGetWidth(ciImage.CGImage);
  } else if (error) {
    *error = [TFLCommonUtils customErrorWithCode:TFLSupportErrorCodeInvalidArgumentError
                                     description:@"CIImage should have CGImage or "
                                                  "CVPixelBuffer info."];
  }

  if (buffer == NULL) {
    return NULL;
  }

  TfLiteFrameBuffer *cFrameBuffer = malloc(sizeof(TfLiteFrameBuffer));
  cFrameBuffer->buffer = buffer;
  cFrameBuffer->dimension.width = width;
  cFrameBuffer->dimension.height = height;

  enum TfLiteFrameBufferFormat cPixelFormat = kRGBA;
  cFrameBuffer->format = cPixelFormat;

  return cFrameBuffer;
}

@end

@implementation GMLImageUtils

+ (nullable TfLiteFrameBuffer *)cFrameBufferWithGMLImage:(GMLImage *)gmlImage
                                                   error:(NSError *_Nullable *)error {
  TfLiteFrameBuffer *cFrameBuffer = NULL;

  switch (gmlImage.imageSourceType) {
    case GMLImageSourceTypeSampleBuffer: {
      CVPixelBufferRef sampleImagePixelBuffer = CMSampleBufferGetImageBuffer(gmlImage.sampleBuffer);
      cFrameBuffer = [GMLImageUtils bufferFromCVPixelBuffer:sampleImagePixelBuffer error:error];
      break;
    }
    case GMLImageSourceTypePixelBuffer: {
      cFrameBuffer = [GMLImageUtils bufferFromCVPixelBuffer:gmlImage.pixelBuffer error:error];
      break;
    }
    case GMLImageSourceTypeImage: {
      cFrameBuffer = [GMLImageUtils frameBufferFromUIImage:gmlImage.image error:error];
    }

    default:
      if (error) {
        *error = [TFLCommonUtils customErrorWithCode:TFLSupportErrorCodeInvalidArgumentError
                                         description:@"Invalid source type for GMLImage."];
      }
      break;
  }

  return cFrameBuffer;
}

+ (TfLiteFrameBuffer *)frameBufferFromUIImage:(UIImage *)image error:(NSError **)error {
  return [image frameBufferWithError:error];
}

+ (TfLiteFrameBuffer *)bufferFromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer
                                         error:(NSError **)error {
  uint8_t *buffer = nil;
  enum TfLiteFrameBufferFormat cPixelFormat = kRGB;

  CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  OSType pixelBufferFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);

  switch (pixelBufferFormat) {
    case kCVPixelFormatType_24RGB: {
      cPixelFormat = kRGB;
      buffer = [GMLImageUtils copyPixelufferDataForInference:pixelBuffer error:error];
      break;
    }
    case kCVPixelFormatType_32RGBA: {
      cPixelFormat = kRGBA;
      buffer = [GMLImageUtils copyPixelufferDataForInference:pixelBuffer error:error];
      break;
    }
    case kCVPixelFormatType_32BGRA: {
      cPixelFormat = kRGB;
      buffer = [TFLCVPixelBufferUtils convertBGRAtoRGBforPixelBufferBaseAddress:pixelBuffer
                                                                          error:error];
      break;
    }

    default: {
      if (error) {
        *error = [TFLCommonUtils
            customErrorWithCode:TFLSupportErrorCodeInvalidArgumentError
                    description:@"Unsupported pixel format for TfLiteFrameBufferFormat."];
      }
      break;
    }
  }

  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

  if (!buffer) {
    return nil;
  }

  TfLiteFrameBuffer *cFrameBuffer = malloc(sizeof(TfLiteFrameBuffer));

  cFrameBuffer->dimension.width = (int)CVPixelBufferGetWidth(pixelBuffer);
  cFrameBuffer->dimension.height = (int)CVPixelBufferGetHeight(pixelBuffer);
  cFrameBuffer->buffer = buffer;
  cFrameBuffer->format = cPixelFormat;

  return cFrameBuffer;
}

+ (UInt8 *)copyPixelufferDataForInference:(CVPixelBufferRef)pixelBuffer error:(NSError **)error {
  size_t height = CVPixelBufferGetHeight(pixelBuffer);
  size_t stride = CVPixelBufferGetBytesPerRow(pixelBuffer);
  UInt8 *buffer = [TFLCommonUtils mallocWithSize:height * stride error:error];

  if (buffer) memcpy(buffer, CVPixelBufferGetBaseAddress(pixelBuffer), height * stride);

  return buffer;
}

@end
