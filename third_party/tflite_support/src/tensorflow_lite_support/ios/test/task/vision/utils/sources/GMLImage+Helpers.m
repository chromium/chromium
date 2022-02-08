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
#import "tensorflow_lite_support/ios/test/task/vision/utils/sources/GMLImage+Helpers.h"

@implementation GMLImage (Helpers)

+ (GMLImage *)imageFromBundleWithClass:(Class)classObject
                              fileName:(NSString *)name
                                ofType:(NSString *)type {
  NSString *imagePath = [[NSBundle bundleForClass:classObject] pathForResource:name ofType:type];
  if (!imagePath) return nil;

  UIImage *image = [[UIImage alloc] initWithContentsOfFile:imagePath];
  if (!image) return nil;

  return [[GMLImage alloc] initWithImage:image];
}

@end
