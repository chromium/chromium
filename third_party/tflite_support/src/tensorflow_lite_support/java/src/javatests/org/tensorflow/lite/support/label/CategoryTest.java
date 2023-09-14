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

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests of {@link org.tensorflow.lite.support.label.Category}. */
@RunWith(RobolectricTestRunner.class)
public final class CategoryTest {
  private static final String APPLE_LABEL = "apple";
  private static final String DEFAULT_DISPLAY_NAME = "";
  private static final String APPLE_DISPLAY_NAME = "manzana"; // "apple" in Spanish.
  private static final float APPLE_SCORE = 0.5f;
  private static final int APPLE_INDEX = 10;

  @Test
  public void createShouldSucceed() {
    Category category = Category.create(APPLE_LABEL, APPLE_DISPLAY_NAME, APPLE_SCORE);

    assertThat(category.getLabel()).isEqualTo(APPLE_LABEL);
    assertThat(category.getDisplayName()).isEqualTo(APPLE_DISPLAY_NAME);
    assertThat(category.getScore()).isWithin(1e-7f).of(APPLE_SCORE);
  }

  @Test
  public void createWithIndexShouldSucceed() {
    Category category = Category.create(APPLE_LABEL, APPLE_DISPLAY_NAME, APPLE_SCORE, APPLE_INDEX);

    assertThat(category.getLabel()).isEqualTo(APPLE_LABEL);
    assertThat(category.getDisplayName()).isEqualTo(APPLE_DISPLAY_NAME);
    assertThat(category.getScore()).isWithin(1e-7f).of(APPLE_SCORE);
    assertThat(category.getIndex()).isEqualTo(APPLE_INDEX);
  }

  @Test
  public void constructorShouldSucceed() {
    Category category = new Category(APPLE_LABEL, APPLE_SCORE);

    assertThat(category.getLabel()).isEqualTo(APPLE_LABEL);
    // Using the constructor, displayName will be default to an empty string.
    assertThat(category.getDisplayName()).isEqualTo(DEFAULT_DISPLAY_NAME);
    assertThat(category.getScore()).isWithin(1e-7f).of(APPLE_SCORE);
  }

  @Test
  public void toStringWithCreateShouldProvideReadableResult() {
    Category category = Category.create(APPLE_LABEL, APPLE_DISPLAY_NAME, APPLE_SCORE);
    String categoryString = category.toString();

    assertThat(categoryString)
        .isEqualTo(
            "<Category \""
                + APPLE_LABEL
                + "\" (displayName="
                + APPLE_DISPLAY_NAME
                + " score="
                + APPLE_SCORE
                + " index=-1"
                + ")>");
  }

  @Test
  public void toStringWithCreateIndexShouldProvideReadableResult() {
    Category category = Category.create(APPLE_LABEL, APPLE_DISPLAY_NAME, APPLE_SCORE, APPLE_INDEX);
    String categoryString = category.toString();

    assertThat(categoryString)
        .isEqualTo(
            "<Category \""
                + APPLE_LABEL
                + "\" (displayName="
                + APPLE_DISPLAY_NAME
                + " score="
                + APPLE_SCORE
                + " index="
                + APPLE_INDEX
                + ")>");
  }

  @Test
  public void toStringWithConstuctorShouldProvideReadableResult() {
    Category category = new Category(APPLE_LABEL, APPLE_SCORE);
    String categoryString = category.toString();

    assertThat(categoryString)
        .isEqualTo(
            "<Category \""
                + APPLE_LABEL
                + "\" (displayName="
                + DEFAULT_DISPLAY_NAME
                + " score="
                + APPLE_SCORE
                + " index=-1"
                + ")>");
  }

  @Test
  public void equalsShouldSucceedWithCreate() {
    Category categoryA = Category.create(APPLE_LABEL, APPLE_DISPLAY_NAME, APPLE_SCORE);
    Category categoryB = Category.create(APPLE_LABEL, APPLE_DISPLAY_NAME, APPLE_SCORE);

    assertThat(categoryA).isEqualTo(categoryB);
  }

  @Test
  public void equalsShouldSucceedWithCreateIndex() {
    Category categoryA = Category.create(APPLE_LABEL, APPLE_DISPLAY_NAME, APPLE_SCORE, APPLE_INDEX);
    Category categoryB = Category.create(APPLE_LABEL, APPLE_DISPLAY_NAME, APPLE_SCORE, APPLE_INDEX);

    assertThat(categoryA).isEqualTo(categoryB);
  }

  @Test
  public void equalsShouldSucceedWithConstructor() {
    Category categoryA = new Category(APPLE_LABEL, APPLE_SCORE);
    Category categoryB = new Category(APPLE_LABEL, APPLE_SCORE);

    assertThat(categoryA).isEqualTo(categoryB);
  }
}
