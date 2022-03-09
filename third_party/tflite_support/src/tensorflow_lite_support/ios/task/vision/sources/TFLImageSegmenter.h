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
#import <Foundation/Foundation.h>
#import "tensorflow_lite_support/ios/task/core/sources/TFLBaseOptions.h"
#import "tensorflow_lite_support/ios/task/processor/sources/TFLSegmentationResult.h"
#import "tensorflow_lite_support/odml/ios/image/apis/GMLImage.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * Specifies the type of output segmentation mask to be returned as a result
 * of the image segmentation operation. This allows specifying the type of
 * post-processing to perform on the raw model results
 *
 * @seealso TfLiteSegmentationResult for more.
 */
typedef NS_ENUM(NSUInteger, TFLOutputType) {
  /** Unspecified output type. */
  TFLUnspecifiedOutputType,

  /**
   * Gives a single output mask where each pixel represents the class which
   * the pixel in the original image was predicted to belong to.
   */
  TFLCategoryMaskOutputType,

  /**
   * Gives a list of output masks where, for each mask, each pixel represents
   * the prediction confidence, usually in the [0, 1] range.
   */
  TFLConfidenceMasksOutputType,

};

/**
 * Options to configure TFLImageSegmenter.
 */
@interface TFLImageSegmenterOptions : NSObject

/**
 * Base options that is used for creation of any type of task.
 * @seealso TFLBaseOptions
 */
@property(nonatomic, copy) TFLBaseOptions* baseOptions;

/**
 * Specifies the type of output segmentation mask to be returned as a result
 * of the image segmentation operation.
 * @seealso TFLOutputType
 */
@property(nonatomic, assign) TFLOutputType outputType;

/** Display names local for display names*/
@property(nonatomic, copy) NSString* displayNamesLocale;

/**
 * Initializes TFLImageSegmenterOptions with the model path set to the specified
 * path to a model file.
 * @description The external model file, must be a single standalone TFLite
 * file. It could be packed with TFLite Model Metadata[1] and associated files
 * if exist. Fail to provide the necessary metadata and associated files might
 * result in errors. Check the
 * [documentation](https://www.tensorflow.org/lite/convert/metadata) for each
 * task about the specific requirement.
 *
 * @param modelPath Path to a TFLite model file.
 *
 * @return An instance of TFLImageSegmenterOptions set to the specified
 * modelPath.
 */
- (nullable instancetype)initWithModelPath:(nonnull NSString*)modelPath;

@end

@interface TFLImageSegmenter : NSObject

/**
 * Creates TFLImageSegmenter from a model file and specified options .
 *
 * @param options TFLImageSegmenterOptions instance with the necessary
 * properties set.
 *
 * @return A TFLImageSegmenter instance.
 */
+ (nullable instancetype)imageSegmenterWithOptions:
                             (nonnull TFLImageSegmenterOptions*)options
                                             error:(NSError**)error
    NS_SWIFT_NAME(imageSegmenter(options:));

/**
 * Performs image segmentation on a GMLImage input, returns the segmentation
 * results.
 *
 * @param image input to the model.
 *
 * @return Segmentation Result of type TFLSegmentationResult holds the
 * segmentation masks returned by the image segmentation task.
 */
- (nullable TFLSegmentationResult*)segmentWithGMLImage:(GMLImage*)image
                                                 error:
                                                     (NSError* _Nullable*)error
    NS_SWIFT_NAME(segment(gmlImage:));

- (instancetype)init NS_UNAVAILABLE;

+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
