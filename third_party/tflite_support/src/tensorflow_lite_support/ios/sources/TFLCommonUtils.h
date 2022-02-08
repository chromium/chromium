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
#include "tensorflow_lite_support/c/common.h"

NS_ASSUME_NONNULL_BEGIN

/** Helper utility for the all tasks which encapsulates common functionality. */
@interface TFLCommonUtils : NSObject

/**
 * Creates and saves an error originating from the task library with the given
 * error code and description.
 *
 * @param code Error code.
 * @param description Error description.
 * @return A NSError instance.
 */
+ (NSError*)customErrorWithCode:(NSInteger)code
                    description:(NSString*)description;

/**
 * Creates and saves an error originating from the task library from a C library
 * error, TfLiteSupportError .
 *
 * @param supportError C library error.
 * @return A NSError instance.
 */
+ (NSError*)errorWithCError:(TfLiteSupportError*)supportError;

/**
 * Allocates a block of memory with the specified size and returns a pointer to
 * it. If memory cannot be allocated because of an invalid memSize, it saves an
 * error. In other cases, it terminates program execution.
 *
 * @param memSize size of memory to be allocated
 * @param error Pointer to the memory location where errors if any should be
 * saved. If `nil`, no error will be saved.
 *
 * @return Pointer to the allocated block of memory on successfull allocation.
 * nil in case as error is encountered because of invalid memSize. If failure is
 * due to any other reason, method terminates program execution.
 */
+ (void*)mallocWithSize:(size_t)memSize error:(NSError**)error;
@end

NS_ASSUME_NONNULL_END
