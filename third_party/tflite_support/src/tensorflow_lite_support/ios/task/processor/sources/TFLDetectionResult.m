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
#import "tensorflow_lite_support/ios/task/processor/sources/TFLDetectionResult.h"

@implementation TFLDetection

- (instancetype)initWithBoundingBox:(CGRect)boundingBox
                         categories:(NSArray<TFLCategory *> *)categories {
  self = [super init];
  if (self) {
    _boundingBox = boundingBox;
    _categories = categories;
  }
  return self;
}

@end

@implementation TFLDetectionResult

- (instancetype)initWithDetections:(NSArray<TFLDetection *> *)detections {
  self = [super init];
  if (self) {
    _detections = detections;
  }
  return self;
}

@end
