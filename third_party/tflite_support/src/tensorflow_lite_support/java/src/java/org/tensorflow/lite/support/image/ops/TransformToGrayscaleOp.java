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

import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkArgument;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.PointF;
import org.tensorflow.lite.support.image.ColorSpaceType;
import org.tensorflow.lite.support.image.ImageOperator;
import org.tensorflow.lite.support.image.TensorImage;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/**
 * Transforms an image to GrayScale as an image processing unit.
 *
 * <p>Supported color spaces:
 *
 * <ul>
 *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#RGB}
 * </ul>
 *
 * <p>The conversion is based on OpenCV RGB to GRAY conversion
 * https://docs.opencv.org/master/de/d25/imgproc_color_conversions.html#color_convert_rgb_gray
 */
public class TransformToGrayscaleOp implements ImageOperator {

  // A matrix is created that will be applied later to canvas to generate grayscale image
  // The luminance of each pixel is calculated as the weighted sum of the 3 RGB values
  // Y = 0.299R + 0.587G + 0.114B
  private static final float[] BITMAP_RGBA_GRAYSCALE_TRANSFORMATION =
      new float[] {
        0.299F, 0.587F, 0.114F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F
      };

  /** Creates a TransformToGrayscaleOp. */
  public TransformToGrayscaleOp() {}

  /**
   * Applies the transformation to grayscale and returns a {@link TensorImage}.
   *
   * <p>If the input image is already {@link
   * org.tensorflow.lite.support.image.ColorSpaceType#GRAYSCALE}, this op will be a no-op.
   *
   * @throws IllegalArgumentException if the {@code image} is not {@link
   *     org.tensorflow.lite.support.image.ColorSpaceType#RGB} or {@link
   *     org.tensorflow.lite.support.image.ColorSpaceType#GRAYSCALE}.
   */
  @Override
  public TensorImage apply(TensorImage image) {
    if (image.getColorSpaceType() == ColorSpaceType.GRAYSCALE) {
      return image;
    } else {
      checkArgument(
          image.getColorSpaceType() == ColorSpaceType.RGB,
          "Only RGB images are supported in TransformToGrayscaleOp, but not "
              + image.getColorSpaceType().name());
    }
    int h = image.getHeight();
    int w = image.getWidth();
    Bitmap bmpGrayscale = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas(bmpGrayscale);
    Paint paint = new Paint();
    ColorMatrixColorFilter colorMatrixFilter =
        new ColorMatrixColorFilter(BITMAP_RGBA_GRAYSCALE_TRANSFORMATION);
    paint.setColorFilter((ColorFilter) colorMatrixFilter);
    canvas.drawBitmap(image.getBitmap(), 0.0F, 0.0F, paint);

    // Get the pixels from the generated grayscale image
    int[] intValues = new int[w * h];
    bmpGrayscale.getPixels(intValues, 0, w, 0, 0, w, h);
    // Shape with one channel
    int[] shape = new int[] {1, h, w, 1};

    // Get R channel from ARGB color
    for (int i = 0; i < intValues.length; i++) {
      intValues[i] = ((intValues[i] >> 16) & 0xff);
    }
    TensorBuffer buffer = TensorBuffer.createFixedSize(shape, image.getDataType());
    buffer.loadArray(intValues, shape);
    image.load(buffer, ColorSpaceType.GRAYSCALE);
    return image;
  }

  @Override
  public int getOutputImageHeight(int inputImageHeight, int inputImageWidth) {
    return inputImageHeight;
  }

  @Override
  public int getOutputImageWidth(int inputImageHeight, int inputImageWidth) {
    return inputImageWidth;
  }

  @Override
  public PointF inverseTransform(PointF point, int inputImageHeight, int inputImageWidth) {
    return point;
  }
}
