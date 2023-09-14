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
package org.tensorflow.lite.support.image;

import static com.google.common.truth.Truth.assertThat;
import static org.tensorflow.lite.DataType.FLOAT32;
import static org.tensorflow.lite.DataType.UINT8;
import static org.tensorflow.lite.support.image.TestImageCreator.createGrayscaleBitmap;
import static org.tensorflow.lite.support.image.TestImageCreator.createGrayscaleTensorBuffer;
import static org.tensorflow.lite.support.image.TestImageCreator.createRgbBitmap;
import static org.tensorflow.lite.support.image.TestImageCreator.createRgbTensorBuffer;

import android.graphics.Bitmap;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

@RunWith(JUnit4.class)
public final class TensorImageInstrumentedTest {

  /**
   * Difference between the pair of float and uint8 values. It is used to test the data conversion.
   */
  private static final float DELTA = 0.1f;

  // Note that parameterized test with android_library_instrumentation_tests is currently not
  // supported in internally.
  @Test
  public void loadAndGetBitmapSucceedsWithFloatBufferFloatImage() {
    DataType tensorBufferDataType = FLOAT32;
    DataType tensorImageDataType = FLOAT32;
    boolean isNormalized = true;
    ColorSpaceType colorSpaceType = ColorSpaceType.GRAYSCALE;

    TensorBuffer tensorBuffer =
        createTensorBuffer(tensorBufferDataType, isNormalized, colorSpaceType, DELTA);
    TensorImage tensorImage = new TensorImage(tensorImageDataType);

    tensorImage.load(tensorBuffer, colorSpaceType);
    Bitmap bitmap = tensorImage.getBitmap();

    Bitmap expectedBitmap = createBitmap(colorSpaceType);
    assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
  }

  @Test
  public void loadAndGetBitmapSucceedsWithFloatBufferUINT8Image() {
    DataType tensorBufferDataType = FLOAT32;
    DataType tensorImageDataType = UINT8;
    boolean isNormalized = false;
    ColorSpaceType colorSpaceType = ColorSpaceType.GRAYSCALE;

    TensorBuffer tensorBuffer =
        createTensorBuffer(tensorBufferDataType, isNormalized, colorSpaceType, DELTA);
    TensorImage tensorImage = new TensorImage(tensorImageDataType);

    tensorImage.load(tensorBuffer, colorSpaceType);
    Bitmap bitmap = tensorImage.getBitmap();

    Bitmap expectedBitmap = createBitmap(colorSpaceType);
    assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
  }

  @Test
  public void loadAndGetBitmapSucceedsWithUINT8BufferFloatImage() {
    DataType tensorBufferDataType = UINT8;
    DataType tensorImageDataType = FLOAT32;
    boolean isNormalized = true;
    ColorSpaceType colorSpaceType = ColorSpaceType.GRAYSCALE;

    TensorBuffer tensorBuffer =
        createTensorBuffer(tensorBufferDataType, isNormalized, colorSpaceType, DELTA);
    TensorImage tensorImage = new TensorImage(tensorImageDataType);

    tensorImage.load(tensorBuffer, colorSpaceType);
    Bitmap bitmap = tensorImage.getBitmap();

    Bitmap expectedBitmap = createBitmap(colorSpaceType);
    assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
  }

  @Test
  public void loadAndGetBitmapSucceedsWithUINT8BufferUINT8Image() {
    DataType tensorBufferDataType = UINT8;
    DataType tensorImageDataType = UINT8;
    boolean isNormalized = false;
    ColorSpaceType colorSpaceType = ColorSpaceType.GRAYSCALE;

    TensorBuffer tensorBuffer =
        createTensorBuffer(tensorBufferDataType, isNormalized, colorSpaceType, DELTA);
    TensorImage tensorImage = new TensorImage(tensorImageDataType);

    tensorImage.load(tensorBuffer, colorSpaceType);
    Bitmap bitmap = tensorImage.getBitmap();

    Bitmap expectedBitmap = createBitmap(colorSpaceType);
    assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
  }

  private static TensorBuffer createTensorBuffer(
      DataType dataType, boolean isNormalized, ColorSpaceType colorSpaceType, float delta) {
    switch (colorSpaceType) {
      case RGB:
        return createRgbTensorBuffer(dataType, isNormalized, delta);
      case GRAYSCALE:
        return createGrayscaleTensorBuffer(dataType, isNormalized, delta);
      default:
        break;
    }
    throw new IllegalArgumentException(
        "The ColorSpaceType, " + colorSpaceType + ", is unsupported.");
  }

  private static Bitmap createBitmap(ColorSpaceType colorSpaceType) {
    switch (colorSpaceType) {
      case RGB:
        return createRgbBitmap();
      case GRAYSCALE:
        return createGrayscaleBitmap();
      default:
        break;
    }
    throw new IllegalArgumentException(
        "The ColorSpaceType, " + colorSpaceType + ", is unsupported.");
  }
}
