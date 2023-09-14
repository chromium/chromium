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

package org.tensorflow.lite.support.image.ops;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.doReturn;
import static org.tensorflow.lite.DataType.UINT8;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.ImageFormat;
import android.media.Image;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.image.ColorSpaceType;
import org.tensorflow.lite.support.image.ImageProcessor;
import org.tensorflow.lite.support.image.TensorImage;

/** Instrumented unit test for {@link TransformToGrayscaleOp}. */
@RunWith(AndroidJUnit4.class)
public class TransformToGrayScaleOpInstrumentedTest {

  @Rule public final MockitoRule mockito = MockitoJUnit.rule();

  private TensorImage input;

  private static final int EXAMPLE_WIDTH = 2;
  private static final int EXAMPLE_HEIGHT = 3;
  @Mock Image imageMock;

  @Before
  public void setUp() {
    Bitmap exampleBitmap = createExampleBitmap();
    input = new TensorImage(DataType.UINT8);
    input.load(exampleBitmap);
  }

  @Test
  public void apply_onRgb_succeeds() {
    ImageProcessor processor =
        new ImageProcessor.Builder().add(new TransformToGrayscaleOp()).build();

    TensorImage output = processor.process(input);
    int[] pixels = output.getTensorBuffer().getIntArray();

    assertThat(output.getWidth()).isEqualTo(EXAMPLE_WIDTH);
    assertThat(output.getHeight()).isEqualTo(EXAMPLE_HEIGHT);
    assertThat(output.getColorSpaceType()).isEqualTo(ColorSpaceType.GRAYSCALE);
    assertThat(pixels).isEqualTo(new int[] {0, 255, 76, 29, 150, 179});
  }

  @Test
  public void apply_onYuv_throws() {
    setUpImageMock(imageMock, ImageFormat.YUV_420_888);
    TensorImage tensorImage = new TensorImage(UINT8);
    tensorImage.load(imageMock);
    ImageProcessor processor =
        new ImageProcessor.Builder().add(new TransformToGrayscaleOp()).build();

    assertThrows(IllegalArgumentException.class, () -> processor.process(tensorImage));
  }

  private static Bitmap createExampleBitmap() {
    int[] colors =
        new int[] {Color.BLACK, Color.WHITE, Color.RED, Color.BLUE, Color.GREEN, Color.CYAN};
    return Bitmap.createBitmap(colors, EXAMPLE_WIDTH, EXAMPLE_HEIGHT, Bitmap.Config.ARGB_8888);
  }

  private static void setUpImageMock(Image imageMock, int imageFormat) {
    doReturn(imageFormat).when(imageMock).getFormat();
  }
}
