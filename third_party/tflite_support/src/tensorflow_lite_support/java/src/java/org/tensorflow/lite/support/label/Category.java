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

package org.tensorflow.lite.support.label;

import java.util.Objects;
import org.tensorflow.lite.annotations.UsedByReflection;

/**
 * Category is a util class, contains a label, its display name, a float value as score, and the
 * index of the label in the corresponding label file. Typically it's used as result of
 * classification tasks.
 */
// This class also acts as a Java wrapper for
// third_party/tensorflow_lite_support/cc/task/core/proto/class.proto.
@UsedByReflection("TFLiteSupport/Task")
public final class Category {
  private static final int DEFAULT_INDEX = -1;
  private static final float TOLERANCE = 1e-6f;
  private final int index;
  private final String label;
  private final String displayName;
  private final float score;

  /**
   * Constructs a {@link Category} object.
   *
   * @param label the label of this category object
   * @param displayName the display name of the label, which may be translated for different
   *     locales. For exmaple, a label, "apple", may be translated into Spanish for display purpose,
   *     so that the displayName is "manzana".
   * @param score the probability score of this label category
   * @param index the index of the label in the corresponding label file
   */
  @UsedByReflection("TFLiteSupport/Task")
  public static Category create(String label, String displayName, float score, int index) {
    return new Category(label, displayName, score, index);
  }

  /** Constructs a {@link Category} object with the default index (-1). */
  @UsedByReflection("TFLiteSupport/Task")
  public static Category create(String label, String displayName, float score) {
    return new Category(label, displayName, score, DEFAULT_INDEX);
  }

  /** Constructs a {@link Category} object with an empty displayName and the default index (-1). */
  @UsedByReflection("TFLiteSupport/Task")
  public Category(String label, float score) {
    this(label, /*displayName=*/ "", score, DEFAULT_INDEX);
  }

  private Category(String label, String displayName, float score, int index) {
    this.label = label;
    this.displayName = displayName;
    this.score = score;
    this.index = index;
  }

  /** Gets the reference of category's label. */
  public String getLabel() {
    return label;
  }

  /**
   * Gets the reference of category's displayName, a name in locale of the label.
   *
   * <p>The display name can be an empty string if this {@link Category} object is constructed
   * without displayName, such as when using {@link #Category(String label, float score)}.
   */
  public String getDisplayName() {
    return displayName;
  }

  /** Gets the score of the category. */
  public float getScore() {
    return score;
  }

  /**
   * Gets the index of the category. The index value might be -1, which means it has not been set up
   * properly and is invalid.
   */
  public int getIndex() {
    return index;
  }

  @Override
  public boolean equals(Object o) {
    if (o instanceof Category) {
      Category other = (Category) o;
      return (other.getLabel().equals(this.label)
          && other.getDisplayName().equals(this.displayName)
          && Math.abs(other.getScore() - this.score) < TOLERANCE
          && other.getIndex() == this.index);
    }
    return false;
  }

  @Override
  public int hashCode() {
    return Objects.hash(label, displayName, score, index);
  }

  @Override
  public String toString() {
    return "<Category \""
        + label
        + "\" (displayName="
        + displayName
        + " score="
        + score
        + " index="
        + index
        + ")>";
  }
}
