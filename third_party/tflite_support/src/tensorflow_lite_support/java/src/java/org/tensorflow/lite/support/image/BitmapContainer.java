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
import android.graphics.Bitmap.Config;
import android.media.Image;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Holds a {@link Bitmap} and converts it to other image formats as needed. */
final class BitmapContainer implements ImageContainer {

  private final Bitmap bitmap;

  /**
   * Creates a {@link BitmapContainer} object with ARGB_8888 {@link Bitmap}.
   *
   * @throws IllegalArgumentException if the bitmap configuration is not ARGB_8888
   */
  static BitmapContainer create(Bitmap bitmap) {
    return new BitmapContainer(bitmap);
  }

  private BitmapContainer(Bitmap bitmap) {
    checkNotNull(bitmap, "Cannot load null bitmap.");
    checkArgument(
        bitmap.getConfig().equals(Config.ARGB_8888), "Only supports loading ARGB_8888 bitmaps.");
    this.bitmap = bitmap;
  }

  @Override
  public BitmapContainer clone() {
    return create(bitmap.copy(bitmap.getConfig(), bitmap.isMutable()));
  }

  @Override
  public Bitmap getBitmap() {
    // Not making a defensive copy for performance considerations. During image processing,
    // users may need to set and get the bitmap many times.
    return bitmap;
  }

  @Override
  public TensorBuffer getTensorBuffer(DataType dataType) {
    TensorBuffer buffer = TensorBuffer.createDynamic(dataType);
    ImageConversions.convertBitmapToTensorBuffer(bitmap, buffer);
    return buffer;
  }

  @Override
  public Image getMediaImage() {
    throw new UnsupportedOperationException(
        "Converting from Bitmap to android.media.Image is unsupported.");
  }

  @Override
  public int getWidth() {
    return bitmap.getWidth();
  }

  @Override
  public int getHeight() {
    return bitmap.getHeight();
  }

  @Override
  public ColorSpaceType getColorSpaceType() {
    return ColorSpaceType.fromBitmapConfig(bitmap.getConfig());
  }
}
