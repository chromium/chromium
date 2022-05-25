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
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * Holds cpu settings.
 */
NS_SWIFT_NAME(CpuSettings)
@interface TFLCpuSettings : NSObject <NSCopying>

/** Specifies the number of threads to be used for TFLite ops that support
 * multi-threadingwhen running inference with CPU.
 * @discussion This property hould be greater than 0 or equal to -1. Setting  it
 * to -1 has the effect to let TFLite runtime set the value.
 */
@property(nonatomic) int numThreads;

@end

/**
 * Holds settings for one possible acceleration configuration.
 */
NS_SWIFT_NAME(ComputeSettings)
@interface TFLComputeSettings : NSObject <NSCopying>

/** Holds cpu settings. */
@property(nonatomic, copy) TFLCpuSettings* cpuSettings;

@end

/**
 * Holds settings for one possible acceleration configuration.
 */
NS_SWIFT_NAME(ExternalFile)
@interface TFLExternalFile : NSObject <NSCopying>

/** Path to the file in bundle. */
@property(nonatomic, copy) NSString* filePath;
/// Add provision for other sources in future.

@end

/**
 * Holds the base options that is used for creation of any type of task. It has
 * fields with important information acceleration configuration, tflite model
 * source etc.
 */
NS_SWIFT_NAME(BaseOptions)
@interface TFLBaseOptions : NSObject <NSCopying>

/**
 * The external model file, as a single standalone TFLite file. It could be
 * packed with TFLite Model Metadata[1] and associated files if exist. Fail to
 * provide the necessary metadata and associated files might result in errors.
 */
@property(nonatomic, copy) TFLExternalFile* modelFile;

/**
 * Holds settings for one possible acceleration configuration including.cpu/gpu
 * settings. Please see documentation of TfLiteComputeSettings and its members
 * for more details.
 */
@property(nonatomic, copy) TFLComputeSettings* computeSettings;

@end

NS_ASSUME_NONNULL_END
