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

package org.tensorflow.lite.support.image;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import android.graphics.Bitmap;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.image.ops.ResizeWithCropOrPadOp;
import org.tensorflow.lite.support.image.ops.Rot90Op;

/** Instrumented unit test for {@link ImageProcessor}. */
@RunWith(AndroidJUnit4.class)
public final class ImageProcessorInstrumentedTest {

  private Bitmap exampleBitmap;
  private TensorImage input;
  private ImageProcessor processor;

  private static final int EXAMPLE_WIDTH = 10;
  private static final int EXAMPLE_HEIGHT = 15;

  @Before
  public void setUp() {
    // The default number of rotation is once.
    processor = new ImageProcessor.Builder().add(new Rot90Op()).build();
    exampleBitmap = createExampleBitmap();
    input = new TensorImage(DataType.UINT8);
    input.load(exampleBitmap);
  }

  @Test
  public void updateNumberOfRotations_rotateTwice() {
    int numberOfRotations = 2;

    processor.updateNumberOfRotations(numberOfRotations);
    TensorImage output = processor.process(input);

    Bitmap outputBitmap = output.getBitmap();
    assertExampleBitmapWithTwoRotations(outputBitmap);
  }

  @Test
  public void updateNumberOfRotationsWithOpIndex_rotateTwiceAndOpIndex0() {
    int numberOfRotations = 2;
    int occurrence = 0;

    processor.updateNumberOfRotations(numberOfRotations, occurrence);
    TensorImage output = processor.process(input);

    Bitmap outputBitmap = output.getBitmap();
    assertExampleBitmapWithTwoRotations(outputBitmap);
  }

  @Test
  public void updateNumberOfRotationsWithOpIndex_negativeOpIndex() {
    int numberOfRotations = 2;
    int negativeOpIndex = -1;

    IndexOutOfBoundsException exception =
        assertThrows(
            IndexOutOfBoundsException.class,
            () -> processor.updateNumberOfRotations(numberOfRotations, negativeOpIndex));
    assertThat(exception).hasMessageThat().isEqualTo("occurrence (-1) must not be negative");
  }

  @Test
  public void updateNumberOfRotationsWithOpIndex_occurrenceEqualToTheNumberOfRot90Op() {
    int numberOfRotations = 2;
    int occurrence = 1;

    IndexOutOfBoundsException exception =
        assertThrows(
            IndexOutOfBoundsException.class,
            () -> processor.updateNumberOfRotations(numberOfRotations, occurrence));
    assertThat(exception).hasMessageThat().isEqualTo("occurrence (1) must be less than size (1)");
  }

  @Test
  public void updateNumberOfRotationsWithOpIndex_noRot90OpIsAddedToImageProcessor() {
    int numberOfRotations = 2;
    int occurrence = 1;
    // Add an op other than Rot90Op into ImageProcessor.
    ImageProcessor processor =
        new ImageProcessor.Builder().add(new ResizeWithCropOrPadOp(5, 5)).build();

    IllegalStateException exception =
        assertThrows(
            IllegalStateException.class,
            () -> processor.updateNumberOfRotations(numberOfRotations, occurrence));
    assertThat(exception)
        .hasMessageThat()
        .isEqualTo("The Rot90Op has not been added to the ImageProcessor.");
  }

  @Test
  public void updateNumberOfRotationsWithOpIndex_twoRot90Ops() {
    // The overall effect of the two rotations is equivalent to rotating for twice.
    int numberOfRotations0 = 5;
    int numberOfRotations1 = 1;

    // Add two Rot90Ops into ImageProcessor.
    ImageProcessor processor =
        new ImageProcessor.Builder().add(new Rot90Op()).add(new Rot90Op()).build();
    processor.updateNumberOfRotations(numberOfRotations0, /*occurrence=*/ 0);
    processor.updateNumberOfRotations(numberOfRotations1, /*occurrence=*/ 1);

    TensorImage output = processor.process(input);
    Bitmap outputBitmap = output.getBitmap();
    assertExampleBitmapWithTwoRotations(outputBitmap);
  }

  private void assertExampleBitmapWithTwoRotations(Bitmap bitmapRotated) {
    assertThat(bitmapRotated.getWidth()).isEqualTo(EXAMPLE_WIDTH);
    assertThat(bitmapRotated.getHeight()).isEqualTo(EXAMPLE_HEIGHT);
    for (int i = 0; i < exampleBitmap.getWidth(); i++) {
      for (int j = 0; j < exampleBitmap.getHeight(); j++) {
        assertThat(exampleBitmap.getPixel(i, j))
            .isEqualTo(bitmapRotated.getPixel(EXAMPLE_WIDTH - 1 - i, EXAMPLE_HEIGHT - 1 - j));
      }
    }
  }

  private static Bitmap createExampleBitmap() {
    int[] colors = new int[EXAMPLE_WIDTH * EXAMPLE_HEIGHT];
    for (int i = 0; i < EXAMPLE_WIDTH * EXAMPLE_HEIGHT; i++) {
      colors[i] = (i << 16) | ((i + 1) << 8) | (i + 2);
    }
    return Bitmap.createBitmap(colors, EXAMPLE_WIDTH, EXAMPLE_HEIGHT, Bitmap.Config.ARGB_8888);
  }
}
