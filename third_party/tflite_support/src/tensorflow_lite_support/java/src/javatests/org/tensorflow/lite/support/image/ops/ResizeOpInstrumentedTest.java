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
import org.tensorflow.lite.support.image.ops.ResizeOp.ResizeMethod;

/** Instrumented unit test for {@link ResizeOp}. */
@RunWith(AndroidJUnit4.class)
public class ResizeOpInstrumentedTest {

  private static final int EXAMPLE_WIDTH = 10;
  private static final int EXAMPLE_HEIGHT = 15;

  private Bitmap exampleBitmap;
  private TensorImage input;

  @Before
  public void setUp() {
    exampleBitmap = createExampleBitmap();
    input = new TensorImage(DataType.UINT8);
    input.load(exampleBitmap);
  }

  @Test
  public void resizeShouldSuccess() {
    int targetWidth = EXAMPLE_WIDTH * 2;
    int targetHeight = EXAMPLE_HEIGHT * 2;
    ImageProcessor processor =
        new ImageProcessor.Builder()
            .add(new ResizeOp(targetHeight, targetWidth, ResizeMethod.NEAREST_NEIGHBOR))
            .build();
    TensorImage output = processor.process(input);

    Bitmap outputBitmap = output.getBitmap();
    assertThat(outputBitmap.getWidth()).isEqualTo(targetWidth);
    assertThat(outputBitmap.getHeight()).isEqualTo(targetHeight);
    for (int i = 0; i < outputBitmap.getWidth(); i++) {
      for (int j = 0; j < outputBitmap.getHeight(); j++) {
        int expected = exampleBitmap.getPixel(i / 2, j / 2);
        assertThat(outputBitmap.getPixel(i, j)).isEqualTo(expected);
      }
    }
  }

  @Test
  public void inverseTransformPointShouldSuccess() {
    ResizeOp op = new ResizeOp(200, 300, ResizeMethod.NEAREST_NEIGHBOR);
    PointF transformed = new PointF(32.0f, 42.0f);
    // The original image size is 900x400 assumed
    PointF original = op.inverseTransform(transformed, 400, 900);
    assertThat(original.x).isEqualTo(96);
    assertThat(original.y).isEqualTo(84);
    PointF outside = op.inverseTransform(new PointF(500, 1000), 400, 900);
    assertThat(outside.x).isEqualTo(1500);
    assertThat(outside.y).isEqualTo(2000);
  }

  /**
   * Creates an example bitmap, which is a 10x15 ARGB bitmap and pixels are set by: - pixel[i] = {A:
   * 255, B: i + 2, G: i + 1, G: i}, where i is the flatten index
   */
  private static Bitmap createExampleBitmap() {
    int[] colors = new int[EXAMPLE_WIDTH * EXAMPLE_HEIGHT];
    for (int i = 0; i < EXAMPLE_WIDTH * EXAMPLE_HEIGHT; i++) {
      colors[i] = (i << 16) | ((i + 1) << 8) | (i + 2);
    }
    return Bitmap.createBitmap(colors, EXAMPLE_WIDTH, EXAMPLE_HEIGHT, Bitmap.Config.ARGB_8888);
  }
}
