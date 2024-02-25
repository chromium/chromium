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
import static org.tensorflow.lite.support.image.TestImageCreator.createGrayscaleBitmap;
import static org.tensorflow.lite.support.image.TestImageCreator.createGrayscaleTensorBuffer;

import android.graphics.Bitmap;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

@RunWith(JUnit4.class)
public final class ColorSpaceTypeInstrumentedTest {

  @Test
  public void convertTensorBufferToBitmapShouldSuccessWithGrayscaleWithUint8() {
    TensorBuffer buffer = createGrayscaleTensorBuffer(DataType.UINT8, false);
    Bitmap bitmap = ColorSpaceType.GRAYSCALE.convertTensorBufferToBitmap(buffer);

    Bitmap expectedBitmap = createGrayscaleBitmap();
    assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
  }

  @Test
  public void convertTensorBufferToBitmapShouldSuccessWithGrayscaleWithFloat() {
    TensorBuffer buffer = createGrayscaleTensorBuffer(DataType.FLOAT32, false);
    Bitmap bitmap = ColorSpaceType.GRAYSCALE.convertTensorBufferToBitmap(buffer);

    Bitmap expectedBitmap = createGrayscaleBitmap();
    assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
  }
}
