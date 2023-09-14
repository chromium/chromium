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
#import "tensorflow_lite_support/ios/task/vision/sources/TFLImageSegmenter.h"
#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"
#import "tensorflow_lite_support/ios/task/core/sources/TFLBaseOptions+Helpers.h"
#import "tensorflow_lite_support/ios/task/processor/sources/TFLSegmentationResult+Helpers.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+Utils.h"

#include "tensorflow_lite_support/c/task/vision/image_segmenter.h"

@implementation TFLImageSegmenterOptions
@synthesize baseOptions;
@synthesize outputType;
@synthesize displayNamesLocale;

- (instancetype)init {
  self = [super init];
  if (self) {
    self.baseOptions = [[TFLBaseOptions alloc] init];
    self.outputType = TFLOutputTypeCategoryMask;
  }
  return self;
}

- (instancetype)initWithModelPath:(NSString *)modelPath {
  self = [self init];
  if (self) {
    self.baseOptions.modelFile.filePath = modelPath;
  }
  return self;
}

@end

@implementation TFLImageSegmenter {
  /** ImageSegmenter backed by C API */
  TfLiteImageSegmenter *_imageSegmenter;
}

- (void)dealloc {
  TfLiteImageSegmenterDelete(_imageSegmenter);
}

- (instancetype)initWithImageSegmenter:(TfLiteImageSegmenter *)imageSegmenter {
  self = [super init];
  if (self) {
    _imageSegmenter = imageSegmenter;
  }
  return self;
}

+ (nullable instancetype)imageSegmenterWithOptions:(nonnull TFLImageSegmenterOptions *)options
                                             error:(NSError **)error {
  TfLiteImageSegmenterOptions cOptions = TfLiteImageSegmenterOptionsCreate();

  [options.baseOptions copyToCOptions:&(cOptions.base_options)];
  cOptions.output_type = (TfLiteImageSegmenterOutputType)options.outputType;

  if (options.displayNamesLocale) {
    if (options.displayNamesLocale.UTF8String) {
      cOptions.display_names_locale = strdup(options.displayNamesLocale.UTF8String);
      if (!cOptions.display_names_locale) {
        exit(-1);  // Memory Allocation Failed.
      }
    } else {
      [TFLCommonUtils createCustomError:error
                               withCode:TFLSupportErrorCodeInvalidArgumentError
                            description:@"Could not convert (NSString *) to (char *)."];
      return nil;
    }
  }

  TfLiteSupportError *cCreateImageSegmenterError = nil;
  TfLiteImageSegmenter *cImageSegmenter =
      TfLiteImageSegmenterFromOptions(&cOptions, &cCreateImageSegmenterError);

  // Freeing memory of allocated string.
  free(cOptions.display_names_locale);

  if (![TFLCommonUtils checkCError:cCreateImageSegmenterError toError:error]) {
    TfLiteSupportErrorDelete(cCreateImageSegmenterError);
  }

  // Return nil if C object detector evaluates to nil. If an error was generted by the C layer, it
  // has already been populated to an NSError and deleted before returning from the method.
  if (!cImageSegmenter) {
    return nil;
  }
  return [[TFLImageSegmenter alloc] initWithImageSegmenter:cImageSegmenter];
}

- (nullable TFLSegmentationResult *)segmentWithGMLImage:(GMLImage *)image
                                                  error:(NSError **)error {
  if (!image) {
    [TFLCommonUtils createCustomError:error
                             withCode:TFLSupportErrorCodeInvalidArgumentError
                          description:@"GMLImage argument cannot be nil."];
    return nil;
  }

  TfLiteFrameBuffer *cFrameBuffer = [image cFrameBufferWithError:error];

  if (!cFrameBuffer) {
    return nil;
  }

  TfLiteSupportError *cSegmentError = nil;
  TfLiteSegmentationResult *cSegmentationResult =
      TfLiteImageSegmenterSegment(_imageSegmenter, cFrameBuffer, &cSegmentError);

  free(cFrameBuffer->buffer);
  cFrameBuffer->buffer = nil;

  free(cFrameBuffer);
  cFrameBuffer = nil;

  // Populate iOS error if C Error is not null and afterwards delete it.
  if (![TFLCommonUtils checkCError:cSegmentError toError:error]) {
    TfLiteSupportErrorDelete(cSegmentError);
  }

  // Return nil if C result evaluates to nil. If an error was generted by the C layer, it has
  // already been populated to an NSError and deleted before returning from the method.
  if (!cSegmentationResult) {
    return nil;
  }

  TFLSegmentationResult *segmentationResult =
      [TFLSegmentationResult segmentationResultWithCResult:cSegmentationResult];
  TfLiteSegmentationResultDelete(cSegmentationResult);

  return segmentationResult;
}

@end
