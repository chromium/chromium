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

import android.graphics.Bitmap;
import android.graphics.Color;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/**
 * Implements some stateless image conversion methods.
 *
 * <p>This class is an internal helper for {@link org.tensorflow.lite.support.image}.
 */
class ImageConversions {

  /**
   * Converts a {@link TensorBuffer} that represents a RGB image to an ARGB_8888 Bitmap.
   *
   * <p>Data in buffer will be converted into integer to match the Bitmap API.
   *
   * @param buffer a RGB image. Its shape should be either (h, w, 3) or (1, h, w, 3)
   * @throws IllegalArgumentException if the shape of buffer is neither (h, w, 3) nor (1, h, w, 3)
   */
  static Bitmap convertRgbTensorBufferToBitmap(TensorBuffer buffer) {
    int[] shape = buffer.getShape();
    ColorSpaceType rgb = ColorSpaceType.RGB;
    rgb.assertShape(shape);

    int h = rgb.getHeight(shape);
    int w = rgb.getWidth(shape);
    Bitmap bitmap = Bitmap.createBitmap(w, h, rgb.toBitmapConfig());

    // TODO(b/138904567): Find a way to avoid creating multiple intermediate buffers every time.
    int[] intValues = new int[w * h];
    int[] rgbValues = buffer.getIntArray();
    for (int i = 0, j = 0; i < intValues.length; i++) {
      int r = rgbValues[j++];
      int g = rgbValues[j++];
      int b = rgbValues[j++];
      intValues[i] = Color.rgb(r, g, b);
    }
    bitmap.setPixels(intValues, 0, w, 0, 0, w, h);

    return bitmap;
  }

  /**
   * Converts a {@link TensorBuffer} that represents a grayscale image to an ALPHA_8 Bitmap.
   *
   * <p>Data in buffer will be converted into integer to match the Bitmap API.
   *
   * @param buffer a grayscale image. Its shape should be either (h, w) or (1, h, w)
   * @throws IllegalArgumentException if the shape of buffer is neither (h, w) nor (1, h, w, 1)
   */
  static Bitmap convertGrayscaleTensorBufferToBitmap(TensorBuffer buffer) {
    // Convert buffer into Uint8 as needed.
    TensorBuffer uint8Buffer =
        buffer.getDataType() == DataType.UINT8
            ? buffer
            : TensorBuffer.createFrom(buffer, DataType.UINT8);

    int[] shape = uint8Buffer.getShape();
    ColorSpaceType grayscale = ColorSpaceType.GRAYSCALE;
    grayscale.assertShape(shape);

    // Even though `Bitmap.createBitmap(int[] colors, int width, int height, Bitmap.Config config)`
    // seems to work for internal Android testing framework, but it actually doesn't work for the
    // real Android environment.
    //
    // The only reliable way to create an ALPHA_8 Bitmap is to use `copyPixelsFromBuffer()` to load
    // the pixels from a ByteBuffer, and then use `copyPixelsToBuffer` to read out.
    // Note: for ALPHA_8 Bitmap, methods such as, `setPixels()` and `getPixels()` do not work.
    Bitmap bitmap =
        Bitmap.createBitmap(
            grayscale.getWidth(shape), grayscale.getHeight(shape), grayscale.toBitmapConfig());
    uint8Buffer.getBuffer().rewind();
    bitmap.copyPixelsFromBuffer(uint8Buffer.getBuffer());
    return bitmap;
  }

  /**
   * Converts an Image in a Bitmap to a TensorBuffer (3D Tensor: Width-Height-Channel) whose memory
   * is already allocated, or could be dynamically allocated.
   *
   * @param bitmap The Bitmap object representing the image. Currently we only support ARGB_8888
   *     config.
   * @param buffer The destination of the conversion. Needs to be created in advance. If it's
   *     fixed-size, its flat size should be w*h*3.
   * @throws IllegalArgumentException if the buffer is fixed-size, but the size doesn't match.
   */
  static void convertBitmapToTensorBuffer(Bitmap bitmap, TensorBuffer buffer) {
    int w = bitmap.getWidth();
    int h = bitmap.getHeight();
    int[] intValues = new int[w * h];
    bitmap.getPixels(intValues, 0, w, 0, 0, w, h);
    // TODO(b/138904567): Find a way to avoid creating multiple intermediate buffers every time.
    int[] shape = new int[] {h, w, 3};
    switch (buffer.getDataType()) {
      case UINT8:
        byte[] byteArr = new byte[w * h * 3];
        for (int i = 0, j = 0; i < intValues.length; i++) {
          byteArr[j++] = (byte) ((intValues[i] >> 16) & 0xff);
          byteArr[j++] = (byte) ((intValues[i] >> 8) & 0xff);
          byteArr[j++] = (byte) (intValues[i] & 0xff);
        }
        ByteBuffer byteBuffer = ByteBuffer.wrap(byteArr);
        byteBuffer.order(ByteOrder.nativeOrder());
        buffer.loadBuffer(byteBuffer, shape);
        break;
      case FLOAT32:
        float[] floatArr = new float[w * h * 3];
        for (int i = 0, j = 0; i < intValues.length; i++) {
          floatArr[j++] = (float) ((intValues[i] >> 16) & 0xff);
          floatArr[j++] = (float) ((intValues[i] >> 8) & 0xff);
          floatArr[j++] = (float) (intValues[i] & 0xff);
        }
        buffer.loadArray(floatArr, shape);
        break;
      default:
        // Should never happen.
        throw new IllegalStateException(
            "The type of TensorBuffer, " + buffer.getBuffer() + ", is unsupported.");
    }
  }

  // Hide the constructor as the class is static.
  private ImageConversions() {}
}
