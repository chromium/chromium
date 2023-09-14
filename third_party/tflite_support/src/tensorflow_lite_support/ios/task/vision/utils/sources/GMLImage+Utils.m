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
#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+Utils.h"

#import <Accelerate/Accelerate.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreImage/CoreImage.h>
#import <CoreVideo/CoreVideo.h>

@interface TFLCVPixelBufferUtils : NSObject

+ (uint8_t *)bufferFromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer error:(NSError **)error;

@end

@interface UIImage (RawPixelDataUtils)
@property(nonatomic, readonly) CGSize bitmapSize;

- (uint8_t *)bufferWithError:(NSError **)error;
- (CVPixelBufferRef)grayScalePixelBuffer;
@end

@implementation TFLCVPixelBufferUtils

+ (uint8_t *)createRGBImageDatafromImageData:(uint8_t *)data
                                   withWidth:(size_t)width
                                      height:(size_t)height
                                      stride:(size_t)stride
                           pixelBufferFormat:(OSType)pixelBufferFormatType
                                       error:(NSError **)error {
  NSInteger destinationChannelCount = 3;
  size_t destinationBytesPerRow = width * destinationChannelCount;

  uint8_t *destPixelBufferAddress =
      [TFLCommonUtils mallocWithSize:sizeof(uint8_t) * height * destinationBytesPerRow error:error];

  if (!destPixelBufferAddress) {
    return NULL;
  }

  vImage_Buffer srcBuffer = {.data = data,
                             .height = (vImagePixelCount)height,
                             .width = (vImagePixelCount)width,
                             .rowBytes = stride};

  vImage_Buffer destBuffer = {.data = destPixelBufferAddress,
                              .height = (vImagePixelCount)height,
                              .width = (vImagePixelCount)width,
                              .rowBytes = destinationBytesPerRow};

  vImage_Error convertError = kvImageNoError;

  switch (pixelBufferFormatType) {
    case kCVPixelFormatType_32RGBA: {
      convertError = vImageConvert_RGBA8888toRGB888(&srcBuffer, &destBuffer, kvImageNoFlags);
      break;
    }
    case kCVPixelFormatType_32BGRA: {
      convertError = vImageConvert_BGRA8888toRGB888(&srcBuffer, &destBuffer, kvImageNoFlags);
      break;
    }
    default: {
      [TFLCommonUtils createCustomError:error
                               withCode:TFLSupportErrorCodeInvalidArgumentError
                            description:@"Invalid source pixel buffer format. Expecting one of "
                                        @"kCVPixelFormatType_32RGBA, kCVPixelFormatType_32BGRA, "
                                        @"kCVPixelFormatType_32ARGB"];

      free(destPixelBufferAddress);
      return NULL;
    }
  }

  if (convertError != kvImageNoError) {
    [TFLCommonUtils createCustomError:error
                             withCode:TFLSupportErrorCodeImageProcessingError
                          description:@"Image format conversion failed."];

    free(destPixelBufferAddress);
    return NULL;
  }

  return destPixelBufferAddress;
}

+ (uint8_t *)createRGBImageDatafromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer
                                           error:(NSError **)error {
  CVPixelBufferLockBaseAddress(pixelBuffer, 0);

  uint8_t *rgbData = [TFLCVPixelBufferUtils
      createRGBImageDatafromImageData:CVPixelBufferGetBaseAddress(pixelBuffer)
                            withWidth:CVPixelBufferGetWidth(pixelBuffer)
                               height:CVPixelBufferGetHeight(pixelBuffer)
                               stride:CVPixelBufferGetBytesPerRow(pixelBuffer)
                    pixelBufferFormat:CVPixelBufferGetPixelFormatType(pixelBuffer)
                                error:error];

  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

  return rgbData;
}

+ (nullable uint8_t *)bufferFromCVPixelBuffer:(CVPixelBufferRef)pixelBuffer
                                        error:(NSError **)error {
  uint8_t *buffer = NULL;

  OSType pixelBufferFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);

  switch (pixelBufferFormat) {
    case kCVPixelFormatType_32BGRA: {
      buffer = [TFLCVPixelBufferUtils createRGBImageDatafromCVPixelBuffer:pixelBuffer error:error];
      break;
    }
    default: {
      [TFLCommonUtils createCustomError:error
                               withCode:TFLSupportErrorCodeInvalidArgumentError
                            description:@"Unsupported pixel format for CVPixelBuffer. Supported "
                                        @"pixel format types are kCVPixelFormatType_32BGRA"];
    }
  }

  return buffer;
}

@end

@implementation UIImage (RawPixelDataUtils)

- (uint8_t *)bufferWithError:(NSError **)error {
  uint8_t *frameBuffer = nil;

  if (self.CGImage) {
    frameBuffer = [self frameBufferFromCGImage:self.CGImage error:error];
  } else if (self.CIImage) {
    frameBuffer = [self frameBufferFromCIImage:self.CIImage error:error];
  } else {
    [TFLCommonUtils createCustomError:error
                             withCode:TFLSupportErrorCodeInvalidArgumentError
                          description:@"UIImage should be initialized from"
                                       " CIImage or CGImage."];
  }

  return frameBuffer;
}

- (CGSize)bitmapSize {
  CGFloat width = 0;
  CGFloat height = 0;

  if (self.CGImage) {
    width = CGImageGetWidth(self.CGImage);
    height = CGImageGetHeight(self.CGImage);
  } else if (self.CIImage.pixelBuffer) {
    width = CVPixelBufferGetWidth(self.CIImage.pixelBuffer);
    height = CVPixelBufferGetHeight(self.CIImage.pixelBuffer);
  } else if (self.CIImage.CGImage) {
    width = CGImageGetWidth(self.CIImage.CGImage);
    height = CGImageGetHeight(self.CIImage.CGImage);
  }
  return CGSizeMake(width, height);
}

- (CVPixelBufferRef)grayScalePixelBuffer {
  CFDictionaryRef options = (__bridge CFDictionaryRef) @{};

  CGImageRef cgImage = [self CGImage];
  if (cgImage == nil) {
    return nil;
  }

  CGDataProviderRef imageDataProvider = CGImageGetDataProvider(cgImage);
  CFMutableDataRef mutableDataRef =
      CFDataCreateMutableCopy(kCFAllocatorDefault, 0, CGDataProviderCopyData(imageDataProvider));

  UInt8 *pixelData = CFDataGetMutableBytePtr(mutableDataRef);

  if (pixelData == nil) return nil;

  CVPixelBufferRef cvPixelBuffer = nil;

  CVPixelBufferCreateWithBytes(kCFAllocatorDefault, CGImageGetWidth(cgImage),
                               CGImageGetHeight(cgImage), kCVPixelFormatType_OneComponent8,
                               pixelData, CGImageGetBytesPerRow(cgImage), nil, nil, options,
                               &cvPixelBuffer);

  return cvPixelBuffer;
}

+ (UInt8 *_Nullable)pixelDataFromCGImage:(CGImageRef)cgImage error:(NSError **)error {
  size_t width = CGImageGetWidth(cgImage);
  size_t height = CGImageGetHeight(cgImage);

  NSInteger bitsPerComponent = 8;
  NSInteger channelCount = 4;
  UInt8 *buffer_to_return = NULL;

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  size_t bytesPerRow = channelCount * width;

  // iOS infers bytesPerRow if it is set to 0.
  // See https://developer.apple.com/documentation/coregraphics/1455939-cgbitmapcontextcreate
  // But for segmentation test image, this was not the case.
  // Hence setting it to the value of channelCount*width.
  // kCGImageAlphaNoneSkipLast specifies that Alpha will always be next to B.
  // kCGBitmapByteOrder32Big specifies that R will be stored before B.
  // In combination they signify a pixelFormat of kCVPixelFormatType32RGBA.
  CGBitmapInfo bitMapinfoFor32RGBA = kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big;
  CGContextRef context = CGBitmapContextCreate(nil, width, height, bitsPerComponent, bytesPerRow,
                                               colorSpace, bitMapinfoFor32RGBA);

  if (context) {
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);
    uint8_t *srcData = CGBitmapContextGetData(context);

    if (srcData) {
      // We have drawn the image as an RGBA image with 8 bitsPerComponent and hence can safely input
      // a pixel format of type kCVPixelFormatType_32RGBA for conversion by vImage.
      buffer_to_return =
          [TFLCVPixelBufferUtils createRGBImageDatafromImageData:srcData
                                                       withWidth:width
                                                          height:height
                                                          stride:bytesPerRow
                                               pixelBufferFormat:kCVPixelFormatType_32RGBA
                                                           error:error];
    }

    CGContextRelease(context);
  }

  CGColorSpaceRelease(colorSpace);

  return buffer_to_return;
}

- (uint8_t *)frameBufferFromCGImage:(CGImageRef)cgImage error:(NSError **)error {
  uint8_t *buffer = [UIImage pixelDataFromCGImage:cgImage error:error];

  return buffer;
}

- (uint8_t *)frameBufferFromCIImage:(CIImage *)ciImage error:(NSError **)error {
  uint8_t *buffer = NULL;

  if (ciImage.pixelBuffer) {
    buffer = [TFLCVPixelBufferUtils createRGBImageDatafromCVPixelBuffer:ciImage.pixelBuffer
                                                                  error:error];

  } else if (ciImage.CGImage) {
    buffer = [UIImage pixelDataFromCGImage:ciImage.CGImage error:error];
  } else {
    [TFLCommonUtils createCustomError:error
                             withCode:TFLSupportErrorCodeInvalidArgumentError
                          description:@"CIImage should have CGImage or CVPixelBuffer info."];
  }

  return buffer;
}

@end

@implementation GMLImage (Utils)

- (nullable uint8_t *)bufferWithError:(NSError **)error {
  uint8_t *buffer = NULL;

  switch (self.imageSourceType) {
    case GMLImageSourceTypeSampleBuffer: {
      CVPixelBufferRef sampleImagePixelBuffer = CMSampleBufferGetImageBuffer(self.sampleBuffer);
      buffer = [TFLCVPixelBufferUtils bufferFromCVPixelBuffer:sampleImagePixelBuffer error:error];
      break;
    }
    case GMLImageSourceTypePixelBuffer: {
      buffer = [TFLCVPixelBufferUtils bufferFromCVPixelBuffer:self.pixelBuffer error:error];
      break;
    }
    case GMLImageSourceTypeImage: {
      buffer = [self.image bufferWithError:error];
      break;
    }
    default:
      [TFLCommonUtils createCustomError:error
                               withCode:TFLSupportErrorCodeInvalidArgumentError
                            description:@"Invalid source type for GMLImage."];
  }

  return buffer;
}

- (CGSize)bitmapSize {
  CGFloat width = 0;
  CGFloat height = 0;

  switch (self.imageSourceType) {
    case GMLImageSourceTypeSampleBuffer: {
      CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(self.sampleBuffer);
      width = CVPixelBufferGetWidth(pixelBuffer);
      height = CVPixelBufferGetHeight(pixelBuffer);
      break;
    }
    case GMLImageSourceTypePixelBuffer: {
      width = CVPixelBufferGetWidth(self.pixelBuffer);
      height = CVPixelBufferGetHeight(self.pixelBuffer);
      break;
    }
    case GMLImageSourceTypeImage: {
      width = self.image.bitmapSize.width;
      height = self.image.bitmapSize.height;
      break;
    }
    default:
      break;
  }

  return CGSizeMake(width, height);
}

- (CVPixelBufferRef)grayScalePixelBuffer {
  switch (self.imageSourceType) {
    case GMLImageSourceTypeSampleBuffer:
      break;
    case GMLImageSourceTypePixelBuffer:
      break;
    case GMLImageSourceTypeImage:
      return [self.image grayScalePixelBuffer];
    default:
      break;
  }

  return nil;
}

- (nullable TfLiteFrameBuffer *)cFrameBufferWithError:(NSError **)error {
  uint8_t *buffer = [self bufferWithError:error];

  if (!buffer) {
    return NULL;
  }

  CGSize bitmapSize = self.bitmapSize;
  enum TfLiteFrameBufferFormat cFrameBufferFormat = kRGB;

  TfLiteFrameBuffer *cFrameBuffer = [TFLCommonUtils mallocWithSize:sizeof(TfLiteFrameBuffer)
                                                             error:error];

  if (cFrameBuffer) {
    cFrameBuffer->dimension.width = bitmapSize.width;
    cFrameBuffer->dimension.height = bitmapSize.height;
    cFrameBuffer->buffer = buffer;
    cFrameBuffer->format = cFrameBufferFormat;
  }

  return cFrameBuffer;
}

+ (GMLImage *)imageFromBundleWithClass:(Class)classObject
                              fileName:(NSString *)name
                                ofType:(NSString *)type {
  NSString *imagePath = [[NSBundle bundleForClass:classObject] pathForResource:name ofType:type];
  if (!imagePath) return nil;

  UIImage *image = [[UIImage alloc] initWithContentsOfFile:imagePath];
  if (!image) return nil;

  return [[GMLImage alloc] initWithImage:image];
}

@end
