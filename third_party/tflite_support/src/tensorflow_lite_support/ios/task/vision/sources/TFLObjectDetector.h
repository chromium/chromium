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
#import "tensorflow_lite_support/ios/task/processor/sources/TFLClassificationOptions.h"
#import "tensorflow_lite_support/ios/task/processor/sources/TFLDetectionResult.h"
#import "tensorflow_lite_support/odml/ios/image/apis/GMLImage.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * Options to configure TFLObjectDetector.
 */
@interface TFLObjectDetectorOptions : NSObject

/**
 * Base options that is used for creation of any type of task.
 * @seealso TFLBaseOptions
 */
@property(nonatomic, copy) TFLBaseOptions* baseOptions;

/**
 * Options that configure the display and filtering of results.
 * @seealso TFLClassificationOptions
 */
@property(nonatomic, copy) TFLClassificationOptions* classificationOptions;

/**
 * Initializes TFLObjectDetectorOptions with the model path set to the specified
 * path to a model file.
 * @description The external model file, must be a single standalone TFLite
 * file. It could be packed with TFLite Model Metadata[1] and associated files
 * if exist. Fail to provide the necessary metadata and associated files might
 * result in errors. Check the [documentation]
 * (https://www.tensorflow.org/lite/convert/metadata) for each task about the
 * specific requirement.
 *
 * @param modelPath Path to a TFLite model file.
 * @return An instance of TFLObjectDetectorOptions set to the specified
 * modelPath.
 */
- (nullable instancetype)initWithModelPath:(nonnull NSString*)modelPath;

@end

@interface TFLObjectDetector : NSObject

/**
 * Creates TFLObjectDetector from a model file and specified options .
 *
 * @param options TFLObjectDetectorOptions instance with the necessary
 * properties set.
 *
 * @return A TFLObjectDetector instance.
 */
+ (nullable instancetype)objectDetectorWithOptions:
                             (nonnull TFLObjectDetectorOptions*)options
                                             error:(NSError**)error
    NS_SWIFT_NAME(objectDetector(options:));

/**
 * Performs object detection on a GMLImage input, returns the detected objects
 * in the image.
 *
 * @param image input to the model.
 * @return Detection Result of type TFLDetectionResult an array of
 * detected objeects  where each detected object has a bounding box and an array
 * of TFLCategory holding the predicted classes for the detected object.
 */
- (nullable TFLDetectionResult*)detectWithGMLImage:(GMLImage*)image
                                             error:(NSError* _Nullable*)error
    NS_SWIFT_NAME(detect(gmlImage:));

- (instancetype)init NS_UNAVAILABLE;

+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
