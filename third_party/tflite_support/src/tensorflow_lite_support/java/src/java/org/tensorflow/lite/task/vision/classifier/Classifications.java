/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.task.vision.classifier;

import com.google.auto.value.AutoValue;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.tensorflow.lite.support.label.Category;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/**
 * The classification results of one head in a multihead (a.k.a. multi-output) {@link
 * ImageClassifier}. A multihead {@link ImageClassifier} can perform classification for multiple
 * purposes, such as a fine grained classifier to describe apparel items (e.g. color, material,
 * type, etc.).
 */
@AutoValue
@UsedByReflection("image_classifier_jni.cc")
public abstract class Classifications {

  @UsedByReflection("image_classifier_jni.cc")
  static Classifications create(List<Category> categories, int headIndex) {
    return new AutoValue_Classifications(
        Collections.unmodifiableList(new ArrayList<Category>(categories)), headIndex);
  }

  // Same reason for not using ImmutableList as stated in
  // {@link ImageClassifier#ImageClassifierOptions#labelAllowList}.
  public abstract List<Category> getCategories();

  public abstract int getHeadIndex();
}
