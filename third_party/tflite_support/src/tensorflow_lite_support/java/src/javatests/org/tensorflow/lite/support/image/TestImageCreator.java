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

import android.graphics.Bitmap;
import android.graphics.Color;
import java.nio.ByteBuffer;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Creates test images for other test files. */
final class TestImageCreator {
  /**
   * Creates an example bitmap, which is a 10x10 ARGB bitmap and pixels are set by: <br>
   * pixel[i] = {A: 255, B: i + 2, G: i + 1, R: i}, where i is the flatten index.
   */
  static Bitmap createRgbBitmap() {
    int[] colors = new int[100];
    for (int i = 0; i < 100; i++) {
      colors[i] = Color.rgb(i, i + 1, i + 2);
    }
    return Bitmap.createBitmap(colors, 10, 10, Bitmap.Config.ARGB_8888);
  }

  /**
   * Creates a 10*10*3 float or uint8 TensorBuffer representing the same image in createRgbBitmap.
   *
   * <p>Adds a default delta, 0.1f, to the generated float values, such that the float array is
   * [0.1, 1.1, 2.1, 3.1, ...], while the uint8 array is[0, 1, 2, 3, ...].
   *
   * @param isNormalized if true, the shape is (1, h, w, 3), otherwise it's (h, w, 3)
   */
  static TensorBuffer createRgbTensorBuffer(DataType dataType, boolean isNormalized) {
    return createRgbTensorBuffer(dataType, isNormalized, /*delta=*/ 0.1f);
  }

  /**
   * Creates a 10*10*3 float or uint8 TensorBuffer representing the same image in createRgbBitmap.
   *
   * @param isNormalized if true, the shape is (1, h, w, 3), otherwise it's (h, w)
   * @param delta the delta that applied to the float values, such that the float array is [0 + +
   *     delta, 1+ delta, 2+ delta, 3+ delta, ...], while the uint8 array is [0, 1, 2, 3, ...]
   */
  static TensorBuffer createRgbTensorBuffer(DataType dataType, boolean isNormalized, float delta) {
    float[] rgbValues = new float[300];
    for (int i = 0, j = 0; i < 100; i++) {
      rgbValues[j++] = i + delta;
      rgbValues[j++] = i + 1 + delta;
      rgbValues[j++] = i + 2 + delta;
    }

    int[] shape = isNormalized ? new int[] {1, 10, 10, 3} : new int[] {10, 10, 3};
    TensorBuffer buffer = TensorBuffer.createFixedSize(shape, dataType);
    // If dataType is UINT8, rgbValues will be converted into uint8, such as from
    // [0.1, 1.1, 2.1, 3.1, ...] to [0, 1, 2, 3, ...].
    buffer.loadArray(rgbValues, shape);
    return buffer;
  }

  /**
   * Creates an example bitmap, which is a 10x10 ALPHA_8 bitmap and pixels are set by: <br>
   * pixel[i] = i, where i is the flatten index.
   */
  static Bitmap createGrayscaleBitmap() {
    byte[] grayValues = new byte[100];
    for (int i = 0; i < 100; i++) {
      grayValues[i] = (byte) i;
    }
    ByteBuffer buffer = ByteBuffer.wrap(grayValues);
    Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ALPHA_8);
    buffer.rewind();
    bitmap.copyPixelsFromBuffer(buffer);
    return bitmap;
  }

  /**
   * Creates a 10*10 float or uint8 TensorBuffer representing the same image in
   * createGrayscaleBitmap.
   *
   * <p>Adds a default delta, 0.1f, to the generated float values, such that the float array is
   * [0.1, 1.1, 2.1, 3.1, ...], while the uint8 array is[0, 1, 2, 3, ...].
   *
   * @param isNormalized if true, the shape is (1, h, w, 1), otherwise it's (h, w)
   */
  static TensorBuffer createGrayscaleTensorBuffer(DataType dataType, boolean isNormalized) {
    return createGrayscaleTensorBuffer(dataType, isNormalized, /*delta=*/ 0.1f);
  }

  /**
   * Creates a 10*10 float or uint8 TensorBuffer representing the same image in
   * createGrayscaleBitmap.
   *
   * @param isNormalized if true, the shape is (1, h, w, 1), otherwise it's (h, w)
   * @param delta the delta that applied to the float values, such that the float array is [0 +
   *     delta, 1+ delta, 2+ delta, 3+ delta, ...], while the uint8 array is [0, 1, 2, 3, ...]
   */
  static TensorBuffer createGrayscaleTensorBuffer(
      DataType dataType, boolean isNormalized, float delta) {
    float[] grayValues = new float[100];
    for (int i = 0; i < 100; i++) {
      grayValues[i] = i + delta;
    }
    int[] shape = isNormalized ? new int[] {1, 10, 10, 1} : new int[] {10, 10};
    TensorBuffer buffer = TensorBuffer.createFixedSize(shape, dataType);
    // If dataType is UINT8, grayValues will be converted into uint8, such as from
    // [0.1, 1.1, 2.1, 3.1, ...] to [0, 1, 2, 3, ...].
    buffer.loadArray(grayValues, shape);
    return buffer;
  }

  private TestImageCreator() {}
}
