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
#import "tensorflow_lite_support/ios/task/vision/sources/TFLImageClassifier.h"
#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"
#import "tensorflow_lite_support/ios/task/core/sources/TFLBaseOptions+Helpers.h"
#import "tensorflow_lite_support/ios/task/processor/sources/TFLClassificationOptions+Helpers.h"
#import "tensorflow_lite_support/ios/task/processor/sources/TFLClassificationResult+Helpers.h"
#import "tensorflow_lite_support/ios/task/vision/utils/sources/GMLImage+Utils.h"

#include "tensorflow_lite_support/c/task/vision/image_classifier.h"

@interface TFLImageClassifier ()
/** ImageClassifier backed by C API */
@property(nonatomic) TfLiteImageClassifier *imageClassifier;
@end

@implementation TFLImageClassifierOptions
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

@implementation TFLImageClassifier
- (void)dealloc {
  TfLiteImageClassifierDelete(_imageClassifier);
}

- (instancetype)initWithImageClassifier:(TfLiteImageClassifier *)imageClassifier {
  self = [super init];
  if (self) {
    _imageClassifier = imageClassifier;
  }
  return self;
}

+ (nullable instancetype)imageClassifierWithOptions:(TFLImageClassifierOptions *)options
                                              error:(NSError **)error {
  if (!options) {
    [TFLCommonUtils createCustomError:error
                             withCode:TFLSupportErrorCodeInvalidArgumentError
                          description:@"TFLImageClassifierOptions argument cannot be nil."];
    return nil;
  }

  TfLiteImageClassifierOptions cOptions = TfLiteImageClassifierOptionsCreate();

  if (![options.classificationOptions copyToCOptions:&(cOptions.classification_options)
                                               error:error]) {
    [options.classificationOptions
        deleteAllocatedMemoryOfClassificationOptions:&(cOptions.classification_options)];
    return nil;
  }

  [options.baseOptions copyToCOptions:&(cOptions.base_options)];

  TfLiteSupportError *cCreateClassifierError = NULL;
  TfLiteImageClassifier *cImageClassifier =
      TfLiteImageClassifierFromOptions(&cOptions, &cCreateClassifierError);

  [options.classificationOptions
      deleteAllocatedMemoryOfClassificationOptions:&(cOptions.classification_options)];

  // Populate iOS error if TfliteSupportError is not null and afterwards delete it.
  if (![TFLCommonUtils checkCError:cCreateClassifierError toError:error]) {
    TfLiteSupportErrorDelete(cCreateClassifierError);
  }

  // Return nil if classifier evaluates to nil. If an error was generted by the C layer, it has
  // already been populated to an NSError and deleted before returning from the method.
  if (!cImageClassifier) {
    return nil;
  }

  return [[TFLImageClassifier alloc] initWithImageClassifier:cImageClassifier];
}

- (nullable TFLClassificationResult *)classifyWithGMLImage:(GMLImage *)image
                                                     error:(NSError **)error {
  return [self classifyWithGMLImage:image
                   regionOfInterest:CGRectMake(0, 0, image.width, image.height)
                              error:error];
}

- (nullable TFLClassificationResult *)classifyWithGMLImage:(GMLImage *)image
                                          regionOfInterest:(CGRect)roi
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

  TfLiteBoundingBox boundingBox = {.origin_x = roi.origin.x,
                                   .origin_y = roi.origin.y,
                                   .width = roi.size.width,
                                   .height = roi.size.height};

  TfLiteSupportError *classifyError = NULL;
  TfLiteClassificationResult *cClassificationResult = TfLiteImageClassifierClassifyWithRoi(
      _imageClassifier, cFrameBuffer, &boundingBox, &classifyError);

  free(cFrameBuffer->buffer);
  cFrameBuffer->buffer = NULL;

  free(cFrameBuffer);
  cFrameBuffer = NULL;

  // Populate iOS error if C Error is not null and afterwards delete it.
  if (![TFLCommonUtils checkCError:classifyError toError:error]) {
    TfLiteSupportErrorDelete(classifyError);
  }

  // Return nil if C result evaluates to nil. If an error was generted by the C layer, it has
  // already been populated to an NSError and deleted before returning from the method.
  if (!cClassificationResult) {
    return nil;
  }

  TFLClassificationResult *classificationHeadsResults =
      [TFLClassificationResult classificationResultWithCResult:cClassificationResult];
  TfLiteClassificationResultDelete(cClassificationResult);

  return classificationHeadsResults;
}
@end
