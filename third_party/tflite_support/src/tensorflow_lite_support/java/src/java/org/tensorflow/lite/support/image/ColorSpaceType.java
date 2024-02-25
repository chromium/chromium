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

import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkArgument;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.ImageFormat;
import java.util.Arrays;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Represents the type of color space of an image. */
public enum ColorSpaceType {
  /** Each pixel has red, green, and blue color components. */
  RGB(0) {

    // The channel axis should always be 3 for RGB images.
    private static final int CHANNEL_VALUE = 3;

    @Override
    Bitmap convertTensorBufferToBitmap(TensorBuffer buffer) {
      return ImageConversions.convertRgbTensorBufferToBitmap(buffer);
    }

    @Override
    int getChannelValue() {
      return CHANNEL_VALUE;
    }

    @Override
    int[] getNormalizedShape(int[] shape) {
      switch (shape.length) {
          // The shape is in (h, w, c) format.
        case 3:
          return insertValue(shape, BATCH_DIM, BATCH_VALUE);
        case 4:
          return shape;
        default:
          throw new IllegalArgumentException(
              getShapeInfoMessage() + "The provided image shape is " + Arrays.toString(shape));
      }
    }

    @Override
    int getNumElements(int height, int width) {
      return height * width * CHANNEL_VALUE;
    }

    @Override
    String getShapeInfoMessage() {
      return "The shape of a RGB image should be (h, w, c) or (1, h, w, c), and channels"
          + " representing R, G, B in order. ";
    }

    @Override
    Config toBitmapConfig() {
      return Config.ARGB_8888;
    }
  },

  /** Each pixel is a single element representing only the amount of light. */
  GRAYSCALE(1) {

    // The channel axis should always be 1 for grayscale images.
    private static final int CHANNEL_VALUE = 1;

    @Override
    Bitmap convertTensorBufferToBitmap(TensorBuffer buffer) {
      return ImageConversions.convertGrayscaleTensorBufferToBitmap(buffer);
    }

    @Override
    int getChannelValue() {
      return CHANNEL_VALUE;
    }

    @Override
    int[] getNormalizedShape(int[] shape) {
      switch (shape.length) {
          // The shape is in (h, w) format.
        case 2:
          int[] shapeWithBatch = insertValue(shape, BATCH_DIM, BATCH_VALUE);
          return insertValue(shapeWithBatch, CHANNEL_DIM, CHANNEL_VALUE);
        case 4:
          return shape;
        default:
          // (1, h, w) and (h, w, 1) are potential grayscale image shapes. However, since they
          // both have three dimensions, it will require extra info to differentiate between them.
          // Since we haven't encountered real use cases of these two shapes, they are not supported
          // at this moment to avoid confusion. We may want to revisit it in the future.
          throw new IllegalArgumentException(
              getShapeInfoMessage() + "The provided image shape is " + Arrays.toString(shape));
      }
    }

    @Override
    int getNumElements(int height, int width) {
      return height * width;
    }

    @Override
    String getShapeInfoMessage() {
      return "The shape of a grayscale image should be (h, w) or (1, h, w, 1). ";
    }

    @Override
    Config toBitmapConfig() {
      return Config.ALPHA_8;
    }
  },

  /** YUV420sp format, encoded as "YYYYYYYY UVUV". */
  NV12(2) {
    @Override
    int getNumElements(int height, int width) {
      return getYuv420NumElements(height, width);
    }
  },

  /**
   * YUV420sp format, encoded as "YYYYYYYY VUVU", the standard picture format on Android Camera1
   * preview.
   */
  NV21(3) {
    @Override
    int getNumElements(int height, int width) {
      return getYuv420NumElements(height, width);
    }
  },

  /** YUV420p format, encoded as "YYYYYYYY VV UU". */
  YV12(4) {
    @Override
    int getNumElements(int height, int width) {
      return getYuv420NumElements(height, width);
    }
  },

  /** YUV420p format, encoded as "YYYYYYYY UU VV". */
  YV21(5) {
    @Override
    int getNumElements(int height, int width) {
      return getYuv420NumElements(height, width);
    }
  },

  /**
   * YUV420 format corresponding to {@link android.graphics.ImageFormat#YUV_420_888}. The actual
   * encoding format (i.e. NV12 / Nv21 / YV12 / YV21) depends on the implementation of the image.
   *
   * <p>Use this format only when you load an {@link android.media.Image}.
   */
  YUV_420_888(6) {
    @Override
    int getNumElements(int height, int width) {
      return getYuv420NumElements(height, width);
    }
  };

  private static final int BATCH_DIM = 0; // The first element of the normalizaed shape.
  private static final int BATCH_VALUE = 1; // The batch axis should always be one.
  private static final int HEIGHT_DIM = 1; // The second element of the normalizaed shape.
  private static final int WIDTH_DIM = 2; // The third element of the normalizaed shape.
  private static final int CHANNEL_DIM = 3; // The fourth element of the normalizaed shape.
  private final int value;

  ColorSpaceType(int value) {
    this.value = value;
  }

  /**
   * Converts a bitmap configuration into the corresponding color space type.
   *
   * @throws IllegalArgumentException if the config is unsupported
   */
  static ColorSpaceType fromBitmapConfig(Config config) {
    switch (config) {
      case ARGB_8888:
        return ColorSpaceType.RGB;
      case ALPHA_8:
        return ColorSpaceType.GRAYSCALE;
      default:
        throw new IllegalArgumentException(
            "Bitmap configuration: " + config + ", is not supported yet.");
    }
  }

  /**
   * Converts an {@link ImageFormat} value into the corresponding color space type.
   *
   * @throws IllegalArgumentException if the config is unsupported
   */
  static ColorSpaceType fromImageFormat(int imageFormat) {
    switch (imageFormat) {
      case ImageFormat.NV21:
        return ColorSpaceType.NV21;
      case ImageFormat.YV12:
        return ColorSpaceType.YV12;
      case ImageFormat.YUV_420_888:
        return ColorSpaceType.YUV_420_888;
      default:
        throw new IllegalArgumentException(
            "ImageFormat: " + imageFormat + ", is not supported yet.");
    }
  }

  public int getValue() {
    return value;
  }

  /**
   * Verifies if the given shape matches the color space type.
   *
   * @throws IllegalArgumentException if {@code shape} does not match the color space type
   * @throws UnsupportedOperationException if the color space type is not RGB or GRAYSCALE
   */
  void assertShape(int[] shape) {
    assertRgbOrGrayScale("assertShape()");

    int[] normalizedShape = getNormalizedShape(shape);
    checkArgument(
        isValidNormalizedShape(normalizedShape),
        getShapeInfoMessage() + "The provided image shape is " + Arrays.toString(shape));
  }

  /**
   * Verifies if the given {@code numElements} in an image buffer matches {@code height} / {@code
   * width} under this color space type. For example, the {@code numElements} of an RGB image of 30
   * x 20 should be {@code 30 * 20 * 3 = 1800}; the {@code numElements} of a NV21 image of 30 x 20
   * should be {@code 30 * 20 + ((30 + 1) / 2 * (20 + 1) / 2) * 2 = 952}.
   *
   * @throws IllegalArgumentException if {@code shape} does not match the color space type
   */
  void assertNumElements(int numElements, int height, int width) {
    checkArgument(
        numElements >= getNumElements(height, width),
        String.format(
            "The given number of elements (%d) does not match the image (%s) in %d x %d. The"
                + " expected number of elements should be at least %d.",
            numElements, this.name(), height, width, getNumElements(height, width)));
  }

  /**
   * Converts a {@link TensorBuffer} that represents an image to a Bitmap with the color space type.
   *
   * @throws IllegalArgumentException if the shape of buffer does not match the color space type,
   * @throws UnsupportedOperationException if the color space type is not RGB or GRAYSCALE
   */
  Bitmap convertTensorBufferToBitmap(TensorBuffer buffer) {
    throw new UnsupportedOperationException(
        "convertTensorBufferToBitmap() is unsupported for the color space type " + this.name());
  }

  /**
   * Returns the width of the given shape corresponding to the color space type.
   *
   * @throws IllegalArgumentException if {@code shape} does not match the color space type
   * @throws UnsupportedOperationException if the color space type is not RGB or GRAYSCALE
   */
  int getWidth(int[] shape) {
    assertRgbOrGrayScale("getWidth()");
    assertShape(shape);
    return getNormalizedShape(shape)[WIDTH_DIM];
  }

  /**
   * Returns the height of the given shape corresponding to the color space type.
   *
   * @throws IllegalArgumentException if {@code shape} does not match the color space type
   * @throws UnsupportedOperationException if the color space type is not RGB or GRAYSCALE
   */
  int getHeight(int[] shape) {
    assertRgbOrGrayScale("getHeight()");
    assertShape(shape);
    return getNormalizedShape(shape)[HEIGHT_DIM];
  }

  /**
   * Returns the channel value corresponding to the color space type.
   *
   * @throws UnsupportedOperationException if the color space type is not RGB or GRAYSCALE
   */
  int getChannelValue() {
    throw new UnsupportedOperationException(
        "getChannelValue() is unsupported for the color space type " + this.name());
  }
  /**
   * Gets the normalized shape in the form of (1, h, w, c). Sometimes, a given shape may not have
   * batch or channel axis.
   *
   * @throws UnsupportedOperationException if the color space type is not RGB or GRAYSCALE
   */
  int[] getNormalizedShape(int[] shape) {
    throw new UnsupportedOperationException(
        "getNormalizedShape() is unsupported for the color space type " + this.name());
  }

  /**
   * Returns the shape information corresponding to the color space type.
   *
   * @throws UnsupportedOperationException if the color space type is not RGB or GRAYSCALE
   */
  String getShapeInfoMessage() {
    throw new UnsupportedOperationException(
        "getShapeInfoMessage() is unsupported for the color space type " + this.name());
  }

  /**
   * Converts the color space type to the corresponding bitmap config.
   *
   * @throws UnsupportedOperationException if the color space type is not RGB or GRAYSCALE
   */
  Config toBitmapConfig() {
    throw new UnsupportedOperationException(
        "toBitmapConfig() is unsupported for the color space type " + this.name());
  }

  /**
   * Gets the number of elements given the height and width of an image. For example, the number of
   * elements of an RGB image of 30 x 20 is {@code 30 * 20 * 3 = 1800}; the number of elements of a
   * NV21 image of 30 x 20 is {@code 30 * 20 + ((30 + 1) / 2 * (20 + 1) / 2) * 2 = 952}.
   */
  abstract int getNumElements(int height, int width);

  private static int getYuv420NumElements(int height, int width) {
    // Height and width of U/V planes are half of the Y plane.
    return height * width + ((height + 1) / 2) * ((width + 1) / 2) * 2;
  }

  /** Inserts a value at the specified position and return the new array. */
  private static int[] insertValue(int[] array, int pos, int value) {
    int[] newArray = new int[array.length + 1];
    for (int i = 0; i < pos; i++) {
      newArray[i] = array[i];
    }
    newArray[pos] = value;
    for (int i = pos + 1; i < newArray.length; i++) {
      newArray[i] = array[i - 1];
    }
    return newArray;
  }

  protected boolean isValidNormalizedShape(int[] shape) {
    return shape[BATCH_DIM] == BATCH_VALUE
        && shape[HEIGHT_DIM] > 0
        && shape[WIDTH_DIM] > 0
        && shape[CHANNEL_DIM] == getChannelValue();
  }

  /** Some existing methods are only valid for RGB and GRAYSCALE images. */
  private void assertRgbOrGrayScale(String unsupportedMethodName) {
    if (this != ColorSpaceType.RGB && this != ColorSpaceType.GRAYSCALE) {
      throw new UnsupportedOperationException(
          unsupportedMethodName
              + " only supports RGB and GRAYSCALE formats, but not "
              + this.name());
    }
  }
}
