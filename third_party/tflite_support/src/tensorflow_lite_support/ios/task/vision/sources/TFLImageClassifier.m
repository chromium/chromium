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

- (nullable instancetype)initWithModelPath:(NSString*)modelPath {
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

+ (nullable instancetype)imageClassifierWithOptions:
                             (TFLImageClassifierOptions*)options
                                              error:(NSError**)error {
  TfLiteImageClassifierOptions cOptions = TfLiteImageClassifierOptionsCreate();
  if (![options.classificationOptions
          copyToCOptions:&(cOptions.classification_options)
                   error:error])
    return nil;

  [options.baseOptions copyToCOptions:&(cOptions.base_options)];

  TfLiteSupportError *createClassifierError = nil;
  TfLiteImageClassifier *imageClassifier =
      TfLiteImageClassifierFromOptions(&cOptions, &createClassifierError);

  [options.classificationOptions
      deleteCStringArraysOfClassificationOptions:&(cOptions.classification_options)];

  if (!imageClassifier || ![TFLCommonUtils checkCError:createClassifierError
                                               toError:error]) {
    TfLiteSupportErrorDelete(createClassifierError);
    return nil;
  }

  return [[TFLImageClassifier alloc] initWithImageClassifier:imageClassifier];
}

- (nullable TFLClassificationResult *)classifyWithGMLImage:(GMLImage *)image
                                                     error:(NSError *_Nullable *)error {
  return [self classifyWithGMLImage:image
                   regionOfInterest:CGRectMake(0, 0, image.width, image.height)
                              error:error];
}

- (nullable TFLClassificationResult *)classifyWithGMLImage:(GMLImage *)image
                                          regionOfInterest:(CGRect)roi
                                                     error:(NSError *_Nullable *)error {
  if (!image) {
    [TFLCommonUtils createCustomError:error
                             withCode:TFLSupportErrorCodeInvalidArgumentError
                          description:@"GMLImage argument cannot be nil."];
    return nil;
  }

  TfLiteFrameBuffer* cFrameBuffer = [image cFrameBufferWithError:error];

  if (!cFrameBuffer) {
    return nil;
  }

  TfLiteBoundingBox boundingBox = {.origin_x = roi.origin.x,
                                   .origin_y = roi.origin.y,
                                   .width = roi.size.width,
                                   .height = roi.size.height};

  TfLiteSupportError *classifyError = nil;
  TfLiteClassificationResult *cClassificationResult = TfLiteImageClassifierClassifyWithRoi(
      _imageClassifier, cFrameBuffer, &boundingBox, &classifyError);

  free(cFrameBuffer->buffer);
  cFrameBuffer->buffer = nil;

  free(cFrameBuffer);
  cFrameBuffer = nil;

  if (!cClassificationResult || ![TFLCommonUtils checkCError:classifyError
                                                     toError:error]) {
    TfLiteSupportErrorDelete(classifyError);
    return nil;
  }

  TFLClassificationResult *classificationHeadsResults =
      [TFLClassificationResult classificationResultWithCResult:cClassificationResult];
  TfLiteClassificationResultDelete(cClassificationResult);

  return classificationHeadsResults;
}
@end
