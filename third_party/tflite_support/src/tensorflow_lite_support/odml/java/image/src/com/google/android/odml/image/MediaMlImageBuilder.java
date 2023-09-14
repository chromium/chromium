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

import android.graphics.Rect;
import android.media.Image;
import android.os.Build.VERSION_CODES;
import androidx.annotation.RequiresApi;

/**
 * Builds {@link MlImage} from {@link android.media.Image}.
 *
 * <p>Once {@link android.media.Image} is passed in, to keep data integrity you shouldn't modify
 * content in it.
 *
 * <p>Use {@link MediaImageExtractor} to get {@link android.media.Image} you passed in.
 */
@RequiresApi(VERSION_CODES.KITKAT)
public class MediaMlImageBuilder {

  // Mandatory fields.
  private final Image mediaImage;

  // Optional fields.
  private int rotation;
  private Rect roi;
  private long timestamp;

  /**
   * Creates the builder with a mandatory {@link android.media.Image}.
   *
   * <p>Also calls {@link #setRotation(int)} to set the optional properties. If not set, the values
   * will be set with default:
   *
   * <ul>
   *   <li>rotation: 0
   * </ul>
   *
   * @param mediaImage image data object.
   */
  public MediaMlImageBuilder(Image mediaImage) {
    this.mediaImage = mediaImage;
    this.rotation = 0;
    this.roi = new Rect(0, 0, mediaImage.getWidth(), mediaImage.getHeight());
    this.timestamp = 0;
  }

  /**
   * Sets value for {@link MlImage#getRotation()}.
   *
   * @throws IllegalArgumentException if the rotation value is not 0, 90, 180 or 270.
   */
  public MediaMlImageBuilder setRotation(int rotation) {
    MlImage.validateRotation(rotation);
    this.rotation = rotation;
    return this;
  }

  /** Sets value for {@link MlImage#getRoi()}. */
  MediaMlImageBuilder setRoi(Rect roi) {
    this.roi = roi;
    return this;
  }

  /** Sets value for {@link MlImage#getTimestamp()}. */
  MediaMlImageBuilder setTimestamp(long timestamp) {
    this.timestamp = timestamp;
    return this;
  }

  /** Builds an {@link MlImage} instance. */
  public MlImage build() {
    return new MlImage(
        new MediaImageContainer(mediaImage),
        rotation,
        roi,
        timestamp,
        mediaImage.getWidth(),
        mediaImage.getHeight());
  }
}
