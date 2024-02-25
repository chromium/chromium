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
import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkNotNull;

import android.graphics.Bitmap;
import android.graphics.ImageFormat;
import android.media.Image;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Holds an {@link Image} and converts it to other image formats as needed. */
final class MediaImageContainer implements ImageContainer {

  private final Image image;

  /**
   * Creates a {@link MediaImageContainer} object with a YUV_420_888 {@link Image}.
   *
   * @throws IllegalArgumentException if the {@link ImageFormat} of {@code image} is not ARGB_8888
   */
  static MediaImageContainer create(Image image) {
    return new MediaImageContainer(image);
  }

  private MediaImageContainer(Image image) {
    checkNotNull(image, "Cannot load null Image.");
    checkArgument(
        image.getFormat() == ImageFormat.YUV_420_888, "Only supports loading YUV_420_888 Image.");
    this.image = image;
  }

  @Override
  public MediaImageContainer clone() {
    throw new UnsupportedOperationException(
        "android.media.Image is an abstract class and cannot be cloned.");
  }

  @Override
  public Bitmap getBitmap() {
    throw new UnsupportedOperationException(
        "Converting an android.media.Image to Bitmap is not supported.");
  }

  @Override
  public TensorBuffer getTensorBuffer(DataType dataType) {
    throw new UnsupportedOperationException(
        "Converting an android.media.Image to TesorBuffer is not supported.");
  }

  @Override
  public Image getMediaImage() {
    return image;
  }

  @Override
  public int getWidth() {
    return image.getWidth();
  }

  @Override
  public int getHeight() {
    return image.getHeight();
  }

  @Override
  public ColorSpaceType getColorSpaceType() {
    return ColorSpaceType.fromImageFormat(image.getFormat());
  }
}
