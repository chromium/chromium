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
#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/sources/TFLCommonUtils.h"
#import "tensorflow_lite_support/ios/task/processor/sources/TFLClassificationOptions+Helpers.h"

@implementation TFLClassificationOptions (Helpers)

+ (char **)cStringArrayFromNSArray:(NSArray<NSString *> *)strings error:(NSError **)error {
  if (strings.count <= 0) {
    [TFLCommonUtils createCustomError:error
                             withCode:TFLSupportErrorCodeInvalidArgumentError
                          description:@"Invalid length of strings found for list type options."];
    return nil;
  }

  char **cStrings = [TFLCommonUtils mallocWithSize:strings.count * sizeof(char *) error:error];
  if (!cStrings) return NULL;

  for (NSInteger i = 0; i < strings.count; i++) {
    cStrings[i] = [TFLCommonUtils
        mallocWithSize:([strings[i] lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1) *
                       sizeof(char)
                 error:error];
    if (!cStrings[i]) return NULL;

    strcpy(cStrings[i], strings[i].UTF8String);
  }
  return cStrings;
}

+ (void)deleteCStringsArray:(char **)cStrings count:(int)count {
  for (NSInteger i = 0; i < count; i++) {
    free(cStrings[i]);
  }

  free(cStrings);
}

- (BOOL)copyToCOptions:(TfLiteClassificationOptions *)cClassificationOptions
                 error:(NSError **)error {
  cClassificationOptions->score_threshold = self.scoreThreshold;
  cClassificationOptions->max_results = (int)self.maxResults;

  if (self.labelDenyList) {
    char **cClassNameBlackList =
        [TFLClassificationOptions cStringArrayFromNSArray:self.labelDenyList error:error];
    if (!cClassNameBlackList) {
      return NO;
    }
    cClassificationOptions->label_denylist.list = cClassNameBlackList;
    cClassificationOptions->label_denylist.length = (int)self.labelDenyList.count;
  }

  if (self.labelAllowList) {
    char **cClassNameWhiteList =
        [TFLClassificationOptions cStringArrayFromNSArray:self.labelAllowList error:error];
    if (!cClassNameWhiteList) {
      return NO;
    }

    cClassificationOptions->label_allowlist.list = cClassNameWhiteList;
    cClassificationOptions->label_allowlist.length = (int)self.labelAllowList.count;
  }

  if (self.displayNamesLocale) {
    if (self.displayNamesLocale.UTF8String) {
      cClassificationOptions->display_names_local = strdup(self.displayNamesLocale.UTF8String);
      if (!cClassificationOptions->display_names_local) {
        exit(-1);  // Memory Allocation Failed.
      }
    } else {
      [TFLCommonUtils createCustomError:error
                               withCode:TFLSupportErrorCodeInvalidArgumentError
                            description:@"Could not convert (NSString *) to (char *)."];
      return NO;
    }
  }

  return YES;
}

- (void)deleteAllocatedMemoryOfClassificationOptions:
    (TfLiteClassificationOptions *)cClassificationOptions {
  if (self.labelAllowList) {
    [TFLClassificationOptions deleteCStringsArray:cClassificationOptions->label_allowlist.list
                                            count:cClassificationOptions->label_allowlist.length];
  }

  if (self.labelDenyList) {
    [TFLClassificationOptions deleteCStringsArray:cClassificationOptions->label_denylist.list
                                            count:cClassificationOptions->label_denylist.length];
  }

  free(cClassificationOptions->display_names_local);
}

@end
