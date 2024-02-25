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

/** Instrumented unit test for {@link Rot90Op}. */
@RunWith(AndroidJUnit4.class)
public class Rot90OpInstrumentedTest {

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
  public void testRot90() {
    ImageProcessor processor = new ImageProcessor.Builder().add(new Rot90Op()).build();
    TensorImage output = processor.process(input);

    Bitmap outputBitmap = output.getBitmap();
    assertThat(outputBitmap.getWidth()).isEqualTo(EXAMPLE_HEIGHT);
    assertThat(outputBitmap.getHeight()).isEqualTo(EXAMPLE_WIDTH);
    for (int i = 0; i < exampleBitmap.getWidth(); i++) {
      for (int j = 0; j < exampleBitmap.getHeight(); j++) {
        assertThat(exampleBitmap.getPixel(i, j))
            .isEqualTo(outputBitmap.getPixel(j, EXAMPLE_WIDTH - 1 - i));
      }
    }
  }

  @Test
  public void testRot90Twice() {
    ImageProcessor processor = new ImageProcessor.Builder().add(new Rot90Op(2)).build();
    TensorImage output = processor.process(input);

    Bitmap outputBitmap = output.getBitmap();
    assertThat(outputBitmap.getWidth()).isEqualTo(EXAMPLE_WIDTH);
    assertThat(outputBitmap.getHeight()).isEqualTo(EXAMPLE_HEIGHT);
    for (int i = 0; i < exampleBitmap.getWidth(); i++) {
      for (int j = 0; j < exampleBitmap.getHeight(); j++) {
        assertThat(exampleBitmap.getPixel(i, j))
            .isEqualTo(outputBitmap.getPixel(EXAMPLE_WIDTH - 1 - i, EXAMPLE_HEIGHT - 1 - j));
      }
    }
  }

  @Test
  public void inverseTransformCorrectlyWhenRotated() {
    Rot90Op op = new Rot90Op(3);
    PointF original = op.inverseTransform(new PointF(20, 10), 200, 100);
    assertThat(original.x).isEqualTo(10);
    assertThat(original.y).isEqualTo(180);
    PointF outside = op.inverseTransform(new PointF(-10, 110), 200, 100);
    assertThat(outside.x).isEqualTo(110);
    assertThat(outside.y).isEqualTo(210);
  }

  private static Bitmap createExampleBitmap() {
    int[] colors = new int[EXAMPLE_WIDTH * EXAMPLE_HEIGHT];
    for (int i = 0; i < EXAMPLE_WIDTH * EXAMPLE_HEIGHT; i++) {
      colors[i] = (i << 16) | ((i + 1) << 8) | (i + 2);
    }
    return Bitmap.createBitmap(colors, EXAMPLE_WIDTH, EXAMPLE_HEIGHT, Bitmap.Config.ARGB_8888);
  }
}
