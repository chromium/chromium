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
#import "tensorflow_lite_support/ios/task/vision/sources/TFLObjectDetector.h"
#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"
#import "tensorflow_lite_support/ios/task/core/sources/TFLBaseOptions+Helpers.h"
#import "tensorflow_lite_support/ios/task/processor/sources/TFLClassificationOptions+Helpers.h"
#import "tensorflow_lite_support/ios/task/processor/sources/TFLDetectionResult+Helpers.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+Utils.h"

#include "tensorflow_lite_support/c/task/vision/object_detector.h"

@interface TFLObjectDetector ()
/** ObjectDetector backed by C API */
@property(nonatomic) TfLiteObjectDetector *objectDetector;
@end

@implementation TFLObjectDetectorOptions
@synthesize baseOptions;
@synthesize classificationOptions;

- (instancetype)init {
  self = [super init];
  if (self) {
    self.baseOptions = [[TFLBaseOptions alloc] init];
    self.classificationOptions = [[TFLClassificationOptions alloc] init];
  }
  return self;
}

- (nullable instancetype)initWithModelPath:(nonnull NSString *)modelPath {
  self = [self init];
  if (self) {
    self.baseOptions.modelFile.filePath = modelPath;
  }
  return self;
}

@end

@implementation TFLObjectDetector
- (void)dealloc {
  TfLiteObjectDetectorDelete(_objectDetector);
}

- (instancetype)initWithObjectDetector:(TfLiteObjectDetector *)objectDetector {
  self = [super init];
  if (self) {
    _objectDetector = objectDetector;
  }
  return self;
}

+ (nullable instancetype)objectDetectorWithOptions:(nonnull TFLObjectDetectorOptions *)options
                                             error:(NSError **)error {
  TfLiteObjectDetectorOptions cOptions = TfLiteObjectDetectorOptionsCreate();
  if (![options.classificationOptions
          copyToCOptions:&(cOptions.classification_options)
                   error:error])
    return nil;

  [options.baseOptions copyToCOptions:&(cOptions.base_options)];

  TfLiteSupportError *createObjectDetectorError = nil;
  TfLiteObjectDetector *objectDetector =
      TfLiteObjectDetectorFromOptions(&cOptions, &createObjectDetectorError);

  [options.classificationOptions
      deleteCStringArraysOfClassificationOptions:&(cOptions.classification_options)];

  if (!objectDetector || ![TFLCommonUtils checkCError:createObjectDetectorError
                                              toError:error]) {
    TfLiteSupportErrorDelete(createObjectDetectorError);
    return nil;
  }

  return [[TFLObjectDetector alloc] initWithObjectDetector:objectDetector];
}

- (nullable TFLDetectionResult *)detectWithGMLImage:(GMLImage *)image
                                              error:(NSError *_Nullable *)error {
  TfLiteFrameBuffer* cFrameBuffer = [image cFrameBufferWithError:error];

  if (!cFrameBuffer) {
    return nil;
  }

  TfLiteSupportError *detectError = nil;
  TfLiteDetectionResult *cDetectionResult =
      TfLiteObjectDetectorDetect(_objectDetector, cFrameBuffer, &detectError);

  free(cFrameBuffer->buffer);
  cFrameBuffer->buffer = nil;

  free(cFrameBuffer);
  cFrameBuffer = nil;

  if (!cDetectionResult || ![TFLCommonUtils checkCError:detectError
                                                toError:error]) {
    TfLiteSupportErrorDelete(detectError);
    return nil;
  }

  TFLDetectionResult *detectionResult =
      [TFLDetectionResult detectionResultWithCResult:cDetectionResult];
  TfLiteDetectionResultDelete(cDetectionResult);

  return detectionResult;
}

@end
