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
#import "tensorflow_lite_support/ios/task/processor/sources/TFLCategory.h"

NS_ASSUME_NONNULL_BEGIN

/** Encapsulates list of predicted classes (aka labels) for a given image
 * classifier head. */
@interface TFLClassifications : NSObject

/**
 * The index of the image classifier head these classes refer to. This is useful
 * for multi-head models.
 */
@property(nonatomic, assign) int headIndex;

/** The array of predicted classes, usually sorted by descending scores
 * (e.g.from high to low probability). */
@property(nonatomic, copy) NSArray<TFLCategory*>* categories;

@end

/** Encapsulates results of any classification task. */
@interface TFLClassificationResult : NSObject

@property(nonatomic, copy) NSArray<TFLClassifications*>* classifications;

@end

NS_ASSUME_NONNULL_END
