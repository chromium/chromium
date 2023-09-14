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
import android.media.Image;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/**
 * Handles image conversion across different image types.
 *
 * <p>An {@link ImageContainer} should support the conversion between the underlying image format to
 * the following image types:
 *
 * <ul>
 *   <li>{@link Bitmap}
 *   <li>{@link TensorBuffer} of the specified data type.
 * </ul>
 */
interface ImageContainer {

  /** Performs deep copy of the {@link ImageContainer}. */
  ImageContainer clone();

  /** Returns the width of the image. */
  int getWidth();

  /** Returns the height of the image. */
  int getHeight();

  /** Gets the {@link Bitmap} representation of the underlying image format. */
  Bitmap getBitmap();

  /**
   * Gets the {@link TensorBuffer} representation with the specific {@code dataType} of the
   * underlying image format.
   */
  TensorBuffer getTensorBuffer(DataType dataType);

  /** Gets the {@link Image} representation of the underlying image format. */
  Image getMediaImage();

  /** Returns the color space type of the image. */
  ColorSpaceType getColorSpaceType();
}
