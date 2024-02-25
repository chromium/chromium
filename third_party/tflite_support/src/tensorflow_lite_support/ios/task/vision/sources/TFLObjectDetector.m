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
#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
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

- (instancetype)initWithModelPath:(NSString *)modelPath {
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

+ (nullable instancetype)objectDetectorWithOptions:(TFLObjectDetectorOptions *)options
                                             error:(NSError **)error {
  if (!options) {
    [TFLCommonUtils createCustomError:error
                             withCode:TFLSupportErrorCodeInvalidArgumentError
                          description:@"TFLObjectDetectorOptions argument cannot be nil."];
    return nil;
  }

  TfLiteObjectDetectorOptions cOptions = TfLiteObjectDetectorOptionsCreate();
  if (![options.classificationOptions copyToCOptions:&(cOptions.classification_options)
                                               error:error]) {
    // Deallocating any allocated memory on failure.
    [options.classificationOptions
        deleteAllocatedMemoryOfClassificationOptions:&(cOptions.classification_options)];
    return nil;
  }

  [options.baseOptions copyToCOptions:&(cOptions.base_options)];

  TfLiteSupportError *cCreateObjectDetectorError = nil;
  TfLiteObjectDetector *cObjectDetector =
      TfLiteObjectDetectorFromOptions(&cOptions, &cCreateObjectDetectorError);

  [options.classificationOptions
      deleteAllocatedMemoryOfClassificationOptions:&(cOptions.classification_options)];

  // Populate iOS error if TfliteSupportError is not null and afterwards delete  it.
  if (![TFLCommonUtils checkCError:cCreateObjectDetectorError toError:error]) {
    TfLiteSupportErrorDelete(cCreateObjectDetectorError);
  }

  // Return nil if C object detector evaluates to nil. If an error was generted by the C layer, it
  // has already been populated to an NSError and deleted before returning from the method.
  if (!cObjectDetector) {
    return nil;
  }

  return [[TFLObjectDetector alloc] initWithObjectDetector:cObjectDetector];
}

- (nullable TFLDetectionResult *)detectWithGMLImage:(GMLImage *)image
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

  TfLiteSupportError *cDetectError = nil;
  TfLiteDetectionResult *cDetectionResult =
      TfLiteObjectDetectorDetect(_objectDetector, cFrameBuffer, &cDetectError);

  free(cFrameBuffer->buffer);
  cFrameBuffer->buffer = nil;

  free(cFrameBuffer);
  cFrameBuffer = nil;

  // Populate iOS error if C Error is not null and afterwards delete it.
  if (![TFLCommonUtils checkCError:cDetectError toError:error]) {
    TfLiteSupportErrorDelete(cDetectError);
  }

  // Return nil if C result evaluates to nil. If an error was generted by the C layer, it has
  // already been populated to an NSError and deleted before returning from the method.
  if (!cDetectionResult) {
    return nil;
  }

  TFLDetectionResult *detectionResult =
      [TFLDetectionResult detectionResultWithCResult:cDetectionResult];
  TfLiteDetectionResultDelete(cDetectionResult);

  return detectionResult;
}

@end
