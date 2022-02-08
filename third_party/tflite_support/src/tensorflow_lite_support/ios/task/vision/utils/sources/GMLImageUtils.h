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

#include "tensorflow_lite_support/c/task/vision/core/frame_buffer.h"
#import "tensorflow_lite_support/odml/ios/image/apis/GMLImage.h"

NS_ASSUME_NONNULL_BEGIN

/** Helper utility for performing operations on GMLImage specific to the
 * TF Lite Task Vision library
 */
@interface GMLImageUtils : NSObject
/**
 * Creates and returns a TfLiteFrameBuffer from a GMLImage. TfLiteFrameBuffer
 * is used by the TFLite Task Vision C library to hold the backing buffer of
 * any image. Image inputs to the TFLite Task Vision C library is of type
 * TfLiteFrameBuffer.
 *
 * @param gmlImage Image of type GMLImage which is to be converted into a
 * TfLiteFrameBuffer.
 * @param error Pointer to the memory location where errors if any should be
 * saved. If `nil`, no error will be saved.
 *
 * @return The TfLiteFrameBuffer created from the gmlImage which can be used
 * with the TF Lite Task Vision C library.
 */
+ (nullable TfLiteFrameBuffer*)cFrameBufferWithGMLImage:(GMLImage*)gmlImage
                                                  error:(NSError* _Nullable*)
                                                            error;

- (instancetype)init NS_UNAVAILABLE;

+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
