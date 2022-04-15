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
  }
  return self;
}

- (nullable instancetype)initWithModelPath:(nonnull NSString*)modelPath {
  self = [self init];
  if (self) {
    self.baseOptions.modelFile.filePath = modelPath;
  }
  return self;
}

@end

@implementation TFLImageSegmenter {
  /** ImageSegmenter backed by C API */
  TfLiteImageSegmenter* _imageSegmenter;
}

- (void)dealloc {
  TfLiteImageSegmenterDelete(_imageSegmenter);
}

- (instancetype)initWithImageSegmenter:(TfLiteImageSegmenter*)imageSegmenter {
  self = [super init];
  if (self) {
    _imageSegmenter = imageSegmenter;
  }
  return self;
}

+ (nullable instancetype)imageSegmenterWithOptions:
                             (nonnull TFLImageSegmenterOptions*)options
                                             error:(NSError**)error {
  TfLiteImageSegmenterOptions cOptions = TfLiteImageSegmenterOptionsCreate();

  [options.baseOptions copyToCOptions:&(cOptions.base_options)];

  TfLiteSupportError* createImageSegmenterError = nil;
  TfLiteImageSegmenter* imageSegmenter =
      TfLiteImageSegmenterFromOptions(&cOptions, &createImageSegmenterError);

  if (!imageSegmenter || ![TFLCommonUtils checkCError:createImageSegmenterError
                                              toError:error]) {
    TfLiteSupportErrorDelete(createImageSegmenterError);
    return nil;
  }

  return [[TFLImageSegmenter alloc] initWithImageSegmenter:imageSegmenter];
}

- (nullable TFLSegmentationResult*)segmentWithGMLImage:(GMLImage*)image
                                                 error:(NSError* _Nullable*)
                                                           error {
  TfLiteFrameBuffer* cFrameBuffer = [image cFrameBufferWithError:error];

  if (!cFrameBuffer) {
    return nil;
  }

  TfLiteSupportError* segmentError = nil;
  TfLiteSegmentationResult* cSegmentationResult =
      TfLiteImageSegmenterSegment(_imageSegmenter, cFrameBuffer, &segmentError);

  free(cFrameBuffer->buffer);
  cFrameBuffer->buffer = nil;

  free(cFrameBuffer);
  cFrameBuffer = nil;

  if (!cSegmentationResult || ![TFLCommonUtils checkCError:segmentError
                                                   toError:error]) {
    TfLiteSupportErrorDelete(segmentError);
    return nil;
  }

  TFLSegmentationResult* segmentationResult =
      [TFLSegmentationResult segmentationResultWithCResult:cSegmentationResult];
  TfLiteSegmentationResultDelete(cSegmentationResult);

  return segmentationResult;
}

@end
