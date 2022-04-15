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
@property(nonatomic, assign, readonly) NSInteger headIndex;

/** The array of predicted classes, usually sorted by descending scores
 * (e.g.from high to low probability). */
@property(nonatomic, copy, readonly) NSArray<TFLCategory*>* categories;

/**
 * Initializes TFLClassifications.
 *
 * @param categories Array of TFLCategory objects encapsulating a list of
 * predictions usually sorted by descending scores (e.g. from high to low
 * probability).
 * @seealso TFLCategory
 *
 * @return An instance of TFLClassifications initialized to
 * the specified values.
 */
- (instancetype)initWithHeadIndex:(NSInteger)headIndex
                       categories:(NSArray<TFLCategory*>*)categories;

@end

/** Encapsulates results of any classification task. */
@interface TFLClassificationResult : NSObject

/** Array of TFLClassifications objects containing image classifier predictions
 * per image classifier head.
 */
@property(nonatomic, copy, readonly)
    NSArray<TFLClassifications*>* classifications;

/**
 * Initializes TFLClassificationResult.
 *
 * @param classifications Array of TFLClassifications objects containing image
 * classifier predictions per image classifier head.
 * @seealso TFLClassifications
 *
 * @return An instance of TFLClassificationResult initialized to the specified
 * values.
 */
- (instancetype)initWithClassifications:
    (NSArray<TFLClassifications*>*)classifications;

@end

NS_ASSUME_NONNULL_END
