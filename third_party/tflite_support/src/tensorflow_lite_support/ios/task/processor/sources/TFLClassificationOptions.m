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
#import "tensorflow_lite_support/ios/task/processor/sources/TFLClassificationOptions.h"

@implementation TFLClassificationOptions
@synthesize scoreThreshold;
@synthesize maxResults;
@synthesize labelAllowList;
@synthesize labelDenyList;
@synthesize displayNamesLocale;

- (instancetype)init {
  self = [super init];
  if (self) {
    self.maxResults = -1;
    self.scoreThreshold = 0;
  }
  return self;
}

- (id)copyWithZone:(NSZone *)zone {
  TFLClassificationOptions *classificationOptions = [[TFLClassificationOptions alloc] init];

  classificationOptions.scoreThreshold = self.scoreThreshold;
  classificationOptions.maxResults = self.maxResults;
  classificationOptions.labelDenyList = self.labelDenyList;
  classificationOptions.labelAllowList = self.labelAllowList;
  classificationOptions.displayNamesLocale = self.displayNamesLocale;

  return classificationOptions;
}

@end
