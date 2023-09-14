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
import com.google.android.odml.image.MlImage.ImageFormat;
import java.nio.ByteBuffer;

/**
 * Builds a {@link MlImage} from a {@link ByteBuffer}.
 *
 * <p>You can pass in either mutable or immutable {@link ByteBuffer}. However once {@link
 * ByteBuffer} is passed in, to keep data integrity you shouldn't modify content in it.
 *
 * <p>Use {@link ByteBufferExtractor} to get {@link ByteBuffer} you passed in.
 */
public class ByteBufferMlImageBuilder {

  // Mandatory fields.
  private final ByteBuffer buffer;
  private final int width;
  private final int height;
  @ImageFormat private final int imageFormat;

  // Optional fields.
  private int rotation;
  private Rect roi;
  private long timestamp;

  /**
   * Creates the builder with mandatory {@link ByteBuffer} and the represented image.
   *
   * <p>We will validate the size of the {@code byteBuffer} with given {@code width}, {@code height}
   * and {@code imageFormat}.
   *
   * <p>Also calls {@link #setRotation(int)} to set the optional properties. If not set, the values
   * will be set with default:
   *
   * <ul>
   *   <li>rotation: 0
   * </ul>
   *
   * @param byteBuffer image data object.
   * @param width the width of the represented image.
   * @param height the height of the represented image.
   * @param imageFormat how the data encode the image.
   */
  public ByteBufferMlImageBuilder(
      ByteBuffer byteBuffer, int width, int height, @ImageFormat int imageFormat) {
    this.buffer = byteBuffer;
    this.width = width;
    this.height = height;
    this.imageFormat = imageFormat;
    // TODO(b/180504869): Validate bytebuffer size with width, height and image format
    this.rotation = 0;
    this.roi = new Rect(0, 0, width, height);
    this.timestamp = 0;
  }

  /**
   * Sets value for {@link MlImage#getRotation()}.
   *
   * @throws IllegalArgumentException if the rotation value is not 0, 90, 180 or 270.
   */
  public ByteBufferMlImageBuilder setRotation(int rotation) {
    MlImage.validateRotation(rotation);
    this.rotation = rotation;
    return this;
  }

  /** Sets value for {@link MlImage#getRoi()}. */
  ByteBufferMlImageBuilder setRoi(Rect roi) {
    this.roi = roi;
    return this;
  }

  /** Sets value for {@link MlImage#getTimestamp()}. */
  ByteBufferMlImageBuilder setTimestamp(long timestamp) {
    this.timestamp = timestamp;
    return this;
  }

  /** Builds an {@link MlImage} instance. */
  public MlImage build() {
    return new MlImage(
        new ByteBufferImageContainer(buffer, imageFormat),
        rotation,
        roi,
        timestamp,
        width,
        height);
  }
}
