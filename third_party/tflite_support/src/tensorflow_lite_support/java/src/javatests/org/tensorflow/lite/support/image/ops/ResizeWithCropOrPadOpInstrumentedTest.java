/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.support.image.ops;

import static com.google.common.truth.Truth.assertThat;

import android.graphics.Bitmap;
import android.graphics.PointF;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.image.ImageProcessor;
import org.tensorflow.lite.support.image.TensorImage;

/** Instrumented unit test for {@link ResizeWithCropOrPadOp}. */
@RunWith(AndroidJUnit4.class)
public class ResizeWithCropOrPadOpInstrumentedTest {

  private Bitmap exampleBitmap;
  private TensorImage input;

  private static final int EXAMPLE_WIDTH = 10;
  private static final int EXAMPLE_HEIGHT = 15;

  @Before
  public void setUp() {
    exampleBitmap = createExampleBitmap();
    input = new TensorImage(DataType.UINT8);
    input.load(exampleBitmap);
  }

  @Test
  public void testResizeWithCrop() {
    int targetWidth = 6;
    int targetHeight = 5;
    ImageProcessor processor =
        new ImageProcessor.Builder()
            .add(new ResizeWithCropOrPadOp(targetHeight, targetWidth))
            .build();
    TensorImage output = processor.process(input);

    Bitmap outputBitmap = output.getBitmap();
    assertThat(outputBitmap.getWidth()).isEqualTo(targetWidth);
    assertThat(outputBitmap.getHeight()).isEqualTo(targetHeight);
    for (int i = 0; i < outputBitmap.getWidth(); i++) {
      for (int j = 0; j < outputBitmap.getHeight(); j++) {
        int expected =
            exampleBitmap.getPixel(
                i + (EXAMPLE_WIDTH - targetWidth) / 2, j + (EXAMPLE_HEIGHT - targetHeight) / 2);
        assertThat(outputBitmap.getPixel(i, j)).isEqualTo(expected);
      }
    }
  }

  @Test
  public void testResizeWithPad() {
    int targetWidth = 15;
    int targetHeight = 20;
    ImageProcessor processor =
        new ImageProcessor.Builder()
            .add(new ResizeWithCropOrPadOp(targetHeight, targetWidth))
            .build();
    TensorImage output = processor.process(input);
    // Pad 2 rows / columns on top / left, and 3 rows / columns on bottom / right

    Bitmap outputBitmap = output.getBitmap();
    assertThat(outputBitmap.getWidth()).isEqualTo(targetWidth);
    assertThat(outputBitmap.getHeight()).isEqualTo(targetHeight);
    int leftPad = (targetWidth - EXAMPLE_WIDTH) / 2;
    int topPad = (targetHeight - EXAMPLE_HEIGHT) / 2;
    for (int i = 0; i < outputBitmap.getWidth(); i++) {
      for (int j = 0; j < outputBitmap.getHeight(); j++) {
        int expected = 0; // ZERO padding
        if (i >= leftPad
            && i < leftPad + EXAMPLE_WIDTH
            && j >= topPad
            && j < topPad + EXAMPLE_HEIGHT) {
          expected = exampleBitmap.getPixel(i - leftPad, j - topPad);
        }
        assertThat(outputBitmap.getPixel(i, j)).isEqualTo(expected);
      }
    }
  }

  @Test
  public void testResizeWithCropAndPad() {
    int targetSize = 12;
    // Pad 1 column on left & right, crop 1 row on top and 2 rows on bottom
    ImageProcessor processor =
        new ImageProcessor.Builder().add(new ResizeWithCropOrPadOp(targetSize, targetSize)).build();
    TensorImage output = processor.process(input);

    Bitmap outputBitmap = output.getBitmap();
    assertThat(outputBitmap.getWidth()).isEqualTo(targetSize);
    assertThat(outputBitmap.getHeight()).isEqualTo(targetSize);

    int leftPad = (targetSize - EXAMPLE_WIDTH) / 2;
    int topCrop = (EXAMPLE_HEIGHT - targetSize) / 2;
    for (int i = 0; i < outputBitmap.getWidth(); i++) {
      for (int j = 0; j < outputBitmap.getHeight(); j++) {
        int expected = 0;
        if (i >= leftPad && i < leftPad + EXAMPLE_WIDTH) {
          expected = exampleBitmap.getPixel(i - leftPad, j + topCrop);
        }
        assertThat(outputBitmap.getPixel(i, j)).isEqualTo(expected);
      }
    }
  }

  @Test
  public void inverseTransformCorrectlyWhenCropped() {
    ResizeWithCropOrPadOp op = new ResizeWithCropOrPadOp(300, 300);
    // The point (100, 50) is transformed from 600x500 image
    PointF original = op.inverseTransform(new PointF(100, 50), 500, 600);
    assertThat(original.x).isEqualTo(250);
    assertThat(original.y).isEqualTo(150);
    PointF cropped = op.inverseTransform(new PointF(-10, -10), 500, 600);
    assertThat(cropped.x).isEqualTo(140);
    assertThat(cropped.y).isEqualTo(90);
  }

  @Test
  public void inverseTransformCorrectlyWhenPadded() {
    ResizeWithCropOrPadOp op = new ResizeWithCropOrPadOp(300, 300);
    // The point (100, 50) is transformed from 100x200 image
    PointF original = op.inverseTransform(new PointF(100, 50), 200, 100);
    assertThat(original.x).isEqualTo(0);
    assertThat(original.y).isEqualTo(0);
    PointF outside = op.inverseTransform(new PointF(50, 10), 200, 100);
    assertThat(outside.x).isEqualTo(-50);
    assertThat(outside.y).isEqualTo(-40);
  }

  /**
   * Creates an example bitmap, which is a 10x15 ARGB bitmap and pixels are set by: - pixel[i] = {A:
   * 255, R: i + 2, G: i + 1, B: i}, where i is the flatten index
   */
  private static Bitmap createExampleBitmap() {
    int[] colors = new int[EXAMPLE_WIDTH * EXAMPLE_HEIGHT];
    for (int i = 0; i < EXAMPLE_WIDTH * EXAMPLE_HEIGHT; i++) {
      colors[i] = (i << 16) | ((i + 1) << 8) | (i + 2);
    }
    return Bitmap.createBitmap(colors, EXAMPLE_WIDTH, EXAMPLE_HEIGHT, Bitmap.Config.ARGB_8888);
  }
}
