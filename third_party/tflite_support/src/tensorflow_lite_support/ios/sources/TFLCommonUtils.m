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
NSString *const TFLSupportTaskErrorDomain = @"org.tensorflow.lite.tasks";

@implementation TFLCommonUtils

+ (void)createCustomError:(NSError **)error
                 withCode:(NSUInteger)code
              description:(NSString *)description {
  [TFLCommonUtils createCustomError:error
                         withDomain:TFLSupportTaskErrorDomain
                               code:code
                        description:description];
}

+ (void)createCustomError:(NSError **)error
               withDomain:(NSString *)domain
                     code:(NSUInteger)code
              description:(NSString *)description {
  if (error) {
    *error = [NSError errorWithDomain:domain
                                 code:code
                             userInfo:@{NSLocalizedDescriptionKey : description}];
  }
}

+ (BOOL)checkCError:(TfLiteSupportError *)supportError toError:(NSError **)error {
  if (!supportError) {
    return YES;
  }
  NSString *description = [NSString stringWithCString:supportError->message
                                             encoding:NSUTF8StringEncoding];
  [TFLCommonUtils createCustomError:error withCode:supportError->code description:description];
  return NO;
}

+ (void *)mallocWithSize:(size_t)memSize error:(NSError **)error {
  if (!memSize) {
    [TFLCommonUtils createCustomError:error
                             withCode:TFLSupportErrorCodeInvalidArgumentError
                          description:@"memSize cannot be zero."];
    return NULL;
  }

  void *allocedMemory = malloc(memSize);
  if (!allocedMemory) {
    exit(-1);
  }

  return allocedMemory;
}

@end
