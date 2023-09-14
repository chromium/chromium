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

import android.graphics.Bitmap;
import android.graphics.Color;
import java.nio.ByteBuffer;

/**
 * Creates test images.
 *
 * <p>Typically, {@link TestImageCreator} creates a 10x2 image, which looks like:
 *
 * <p>{@code BBBBBWWWWW}
 *
 * <p>{@code GGGGGRRRRR}
 *
 * <p>where B=0x000093, W=0xffffff, G=0x009300, R=0x930000.
 *
 * <p>The value of ALPHA channel is 0x70 or 0xff, depending on the settings.
 *
 * <p>The created {@link Bitmap} is not pre-multiplied.
 */
final class TestImageCreator {

  private static final int RED = 0x73;
  private static final int GREEN = 0x85;
  private static final int BLUE = 0x96;
  private static final int ALPHA = 0x70;

  static int getWidth() {
    return 10;
  }

  static int getHeight() {
    return 2;
  }

  /**
   * Creates an example non-pre-multiplied bitmap which is 100% opaque.
   *
   * @see TestImageCreator for details.
   */
  static Bitmap createOpaqueRgbaBitmap() {
    return createRgbaBitmap(0xff);
  }

  /**
   * Creates an example non-pre-multiplied bitmap which has non-trivial alpha channel.
   *
   * @see TestImageCreator for details.
   */
  static Bitmap createRgbaBitmap() {
    return createRgbaBitmap(ALPHA);
  }

  /**
   * Creates an example 10x2 bitmap demonstrated in the class doc. A channel sets to {@code alpha}.
   */
  static Bitmap createRgbaBitmap(int alpha) {
    int[] colors = new int[20];
    for (int i = 0; i < 5; i++) {
      colors[i] = Color.argb(alpha, 0, 0, BLUE);
      colors[i + 5] = Color.argb(alpha, 0xff, 0xff, 0xff);
      colors[i + 10] = Color.argb(alpha, 0, GREEN, 0);
      colors[i + 15] = Color.argb(alpha, RED, 0, 0);
    }
    // We don't use Bitmap#createBitmap(int[] ...) here, because that method creates pre-multiplied
    // bitmaps.
    Bitmap bitmap = Bitmap.createBitmap(10, 2, Bitmap.Config.ARGB_8888);
    bitmap.setPremultiplied(false);
    bitmap.setPixels(colors, 0, 10, 0, 0, 10, 2);
    return bitmap;
  }

  /**
   * Creates an example 10*10*3 bytebuffer in R-G-B format.
   *
   * @see TestImageCreator for details.
   */
  static ByteBuffer createRgbBuffer() {
    return createRgbOrRgbaBuffer(false, 0xff);
  }

  /**
   * Creates an example 10*10*4 bytebuffer in R-G-B-A format.
   *
   * @see TestImageCreator for details.
   */
  static ByteBuffer createRgbaBuffer() {
    return createRgbOrRgbaBuffer(true, ALPHA);
  }

  /**
   * Creates an example 10*10*4 bytebuffer in R-G-B-A format, but the A channel is 0xFF.
   *
   * @see TestImageCreator for details.
   */
  static ByteBuffer createOpaqueRgbaBuffer() {
    return createRgbOrRgbaBuffer(true, 0xff);
  }

  /**
   * Creates an example 10x2x4 (or 10x2x3 if no alpha) bytebuffer demonstrated in the class doc.
   *
   * @param withAlpha if true, set A to {@code alpha}, otherwise A channel is ignored.
   * @param alpha alpha channel value. Only effective when {@code withAlpha} is {@code true}.
   */
  static ByteBuffer createRgbOrRgbaBuffer(boolean withAlpha, int alpha) {
    int capacity = withAlpha ? 80 : 60;
    ByteBuffer buffer = ByteBuffer.allocateDirect(capacity);
    putColorInByteBuffer(buffer, 0, 0, BLUE, withAlpha, alpha, 5);
    putColorInByteBuffer(buffer, 0xff, 0xff, 0xff, withAlpha, alpha, 5);
    putColorInByteBuffer(buffer, 0, GREEN, 0, withAlpha, alpha, 5);
    putColorInByteBuffer(buffer, RED, 0, 0, withAlpha, alpha, 5);
    buffer.rewind();
    return buffer;
  }

  private static void putColorInByteBuffer(
      ByteBuffer buffer, int r, int g, int b, boolean withAlpha, int alpha, int num) {
    for (int i = 0; i < num; i++) {
      buffer.put((byte) r);
      buffer.put((byte) g);
      buffer.put((byte) b);
      if (withAlpha) {
        buffer.put((byte) alpha);
      }
    }
  }

  // Should not be instantiated.
  private TestImageCreator() {}
}
