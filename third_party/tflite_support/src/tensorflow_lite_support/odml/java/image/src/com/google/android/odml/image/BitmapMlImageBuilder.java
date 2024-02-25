/* Copyright 2021 Google LLC. All Rights Reserved.

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

package com.google.android.odml.image;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.net.Uri;
import android.provider.MediaStore;
import java.io.IOException;

/**
 * Builds {@link MlImage} from {@link android.graphics.Bitmap}.
 *
 * <p>You can pass in either mutable or immutable {@link android.graphics.Bitmap}. However once
 * {@link android.graphics.Bitmap} is passed in, to keep data integrity you shouldn't modify content
 * in it.
 *
 * <p>Use {@link BitmapExtractor} to get {@link android.graphics.Bitmap} you passed in.
 */
public class BitmapMlImageBuilder {

  // Mandatory fields.
  private final Bitmap bitmap;

  // Optional fields.
  private int rotation;
  private Rect roi;
  private long timestamp;

  /**
   * Creates the builder with a mandatory {@link android.graphics.Bitmap}.
   *
   * <p>Also calls {@link #setRotation(int)} to set the optional properties. If not set, the values
   * will be set with default:
   *
   * <ul>
   *   <li>rotation: 0
   * </ul>
   *
   * @param bitmap image data object.
   */
  public BitmapMlImageBuilder(Bitmap bitmap) {
    this.bitmap = bitmap;
    rotation = 0;
    roi = new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight());
    timestamp = 0;
  }

  /**
   * Creates the builder to build {@link MlImage} from a file.
   *
   * <p>Also calls {@link #setRotation(int)} to set the optional properties. If not set, the values
   * will be set with default:
   *
   * <ul>
   *   <li>rotation: 0
   * </ul>
   *
   * @param context the application context.
   * @param uri the path to the resource file.
   */
  public BitmapMlImageBuilder(Context context, Uri uri) throws IOException {
    this(MediaStore.Images.Media.getBitmap(context.getContentResolver(), uri));
  }

  /**
   * Sets value for {@link MlImage#getRotation()}.
   *
   * @throws IllegalArgumentException if the rotation value is not 0, 90, 180 or 270.
   */
  public BitmapMlImageBuilder setRotation(int rotation) {
    MlImage.validateRotation(rotation);
    this.rotation = rotation;
    return this;
  }

  /** Sets value for {@link MlImage#getRoi()}. */
  BitmapMlImageBuilder setRoi(Rect roi) {
    this.roi = roi;
    return this;
  }

  /** Sets value for {@link MlImage#getTimestamp()}. */
  BitmapMlImageBuilder setTimestamp(long timestamp) {
    this.timestamp = timestamp;
    return this;
  }

  /** Builds an {@link MlImage} instance. */
  public MlImage build() {
    return new MlImage(
        new BitmapImageContainer(bitmap),
        rotation,
        roi,
        timestamp,
        bitmap.getWidth(),
        bitmap.getHeight());
  }
}
