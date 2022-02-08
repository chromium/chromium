// Copyright 2021 The TensorFlow Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"
#import "tensorflow_lite_support/ios/sources/TFLCommon.h"

/** Error domain of TensorFlow Lite Support related errors. */
static NSString *const TFLSupportTaskErrorDomain = @"org.tensorflow.lite.tasks";

@implementation TFLCommonUtils

+ (NSError *)customErrorWithCode:(NSInteger)code description:(NSString *)description {
  return [NSError errorWithDomain:TFLSupportTaskErrorDomain
                             code:code
                         userInfo:@{NSLocalizedDescriptionKey : description}];
}

+ (NSError *)errorWithCError:(TfLiteSupportError *)supportError {
  return [NSError
      errorWithDomain:TFLSupportTaskErrorDomain
                 code:supportError->code
             userInfo:@{
               NSLocalizedDescriptionKey : [NSString stringWithCString:supportError->message
                                                              encoding:NSUTF8StringEncoding]
             }];
}

+ (void *)mallocWithSize:(size_t)memSize error:(NSError **)error {
  if (!memSize) {
    if (error) {
      *error = [TFLCommonUtils
          customErrorWithCode:TFLSupportErrorCodeInvalidArgumentError
                  description:@"Invalid memory size passed for allocation of object."];
    }
    return NULL;
  }

  void *allocedMemory = malloc(memSize);
  if (!allocedMemory && memSize) {
    exit(-1);
  }

  return allocedMemory;
}

@end
