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

package org.tensorflow.lite.support.image;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import android.graphics.Bitmap;
import android.graphics.RectF;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.common.ops.NormalizeOp;
import org.tensorflow.lite.support.image.ops.ResizeOp;
import org.tensorflow.lite.support.image.ops.ResizeOp.ResizeMethod;
import org.tensorflow.lite.support.image.ops.ResizeWithCropOrPadOp;
import org.tensorflow.lite.support.image.ops.Rot90Op;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Tests for {@link ImageProcessor}. */
@RunWith(RobolectricTestRunner.class)
public final class ImageProcessorTest {

  private static final int EXAMPLE_WIDTH = 10;
  private static final int EXAMPLE_HEIGHT = 15;
  private static final int EXAMPLE_NUM_PIXELS = EXAMPLE_HEIGHT * EXAMPLE_WIDTH;
  private static final int EXAMPLE_NUM_CHANNELS = 3;
  private static final float MEAN = 127.5f;
  private static final float STDDEV = 127.5f;

  @Test
  public void testBuild() {
    ImageProcessor processor =
        new ImageProcessor.Builder().add(new NormalizeOp(MEAN, STDDEV)).build();
    assertThat(processor).isNotNull();
  }

  @Test
  public void testNormalize() {
    TensorImage input = new TensorImage(DataType.FLOAT32);
    input.load(createExampleBitmap());
    ImageProcessor processor =
        new ImageProcessor.Builder().add(new NormalizeOp(MEAN, STDDEV)).build();
    TensorImage output = processor.process(input);

    float[] pixels = output.getTensorBuffer().getFloatArray();
    assertThat(pixels.length).isEqualTo(EXAMPLE_NUM_CHANNELS * EXAMPLE_NUM_PIXELS);
    for (float p : pixels) {
      assertThat(p).isAtLeast(-1);
      assertThat(p).isAtMost(1);
    }
  }

  @Test
  public void testMultipleNormalize() {
    TensorImage input = new TensorImage(DataType.FLOAT32);
    input.load(createExampleBitmap());
    ImageProcessor processor =
        new ImageProcessor.Builder()
            .add(new NormalizeOp(MEAN, STDDEV)) // [0, 255] -> [-1, 1]
            .add(new NormalizeOp(-1, 2)) // [-1, 1] -> [0, 1]
            .build();
    TensorImage output = processor.process(input);

    float[] pixels = output.getTensorBuffer().getFloatArray();
    assertThat(pixels.length).isEqualTo(EXAMPLE_NUM_CHANNELS * EXAMPLE_NUM_PIXELS);
    for (float p : pixels) {
      assertThat(p).isAtLeast(0);
      assertThat(p).isAtMost(1);
    }
  }

  @Test
  public void inverseTransformRectCorrectly() {
    ImageProcessor processor =
        new ImageProcessor.Builder()
            .add(new ResizeOp(200, 300, ResizeMethod.BILINEAR))
            .add(new ResizeWithCropOrPadOp(100, 200))
            .add(new Rot90Op(1))
            .add(new NormalizeOp(127, 128))
            .build();
    RectF transformed = new RectF(0, 50, 100, 150);
    RectF original = processor.inverseTransform(transformed, 400, 600);
    assertThat(original.top).isEqualTo(100);
    assertThat(original.left).isEqualTo(200);
    assertThat(original.right).isEqualTo(400);
    assertThat(original.bottom).isEqualTo(300);
  }

  @Test
  public void resizeShouldFailWithNonRgbImages() {
    int[] data = new int[] {1, 2, 3};
    TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.UINT8);
    tensorBuffer.loadArray(data, new int[] {1, 3});
    TensorImage image = new TensorImage();
    image.load(tensorBuffer, ColorSpaceType.GRAYSCALE);

    ImageProcessor processor =
        new ImageProcessor.Builder().add(new ResizeOp(200, 300, ResizeMethod.BILINEAR)).build();

    IllegalArgumentException exception =
        assertThrows(IllegalArgumentException.class, () -> processor.process(image));
    assertThat(exception)
        .hasMessageThat()
        .contains(
            "Only RGB images are supported in ResizeOp, but not "
                + image.getColorSpaceType().name());
  }

  @Test
  public void normalizeShouldSuccessWithNonRgbImages() {
    int[] data = new int[] {1, 2, 3};
    TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.UINT8);
    tensorBuffer.loadArray(data, new int[] {1, 3});
    TensorImage image = new TensorImage();
    image.load(tensorBuffer, ColorSpaceType.GRAYSCALE);

    ImageProcessor processor =
        new ImageProcessor.Builder().add(new NormalizeOp(0.5f, 1f)).build();
    TensorImage output = processor.process(image);

    float[] pixels = output.getTensorBuffer().getFloatArray();
    assertThat(pixels).isEqualTo(new float[]{0.5f, 1.5f, 2.5f});
  }

  private static Bitmap createExampleBitmap() {
    int[] colors = new int[EXAMPLE_NUM_PIXELS];
    for (int i = 0; i < EXAMPLE_NUM_PIXELS; i++) {
      colors[i] = (i << 16) | ((i + 1) << 8) | (i + 2);
    }

    return Bitmap.createBitmap(colors, EXAMPLE_WIDTH, EXAMPLE_HEIGHT, Bitmap.Config.ARGB_8888);
  }
}
