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

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;

import android.graphics.Bitmap;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/**
 * Tests for {@link ByteBufferExtractor}.
 *
 * <p>{@code RGBA}{@link Bitmap} to {@code RGBA}{@link ByteBuffer} convesion is not tested here,
 * like {@link ByteBufferExtractorInstrumentedTest#extract_rgbaFromRgbaBitmap_succeeds()}, because
 * Robolectric seems not handling {@link Bitmap#copyPixelsToBuffer(Buffer)} correctly. So that we
 * only test that path in the instrumented unit test.
 */
@RunWith(RobolectricTestRunner.class)
public final class ByteBufferExtractorTest {

  @Test
  public void extract_fromByteBuffer_succeeds() {
    ByteBuffer byteBuffer = TestImageCreator.createRgbBuffer();
    MlImage image =
        new ByteBufferMlImageBuilder(
                byteBuffer,
                TestImageCreator.getWidth(),
                TestImageCreator.getHeight(),
                MlImage.IMAGE_FORMAT_RGB)
            .build();

    ByteBuffer result = ByteBufferExtractor.extract(image);

    assertThat(result).isEquivalentAccordingToCompareTo(byteBuffer);
    assertThat(result.isReadOnly()).isTrue();
  }

  @Test
  public void extract_fromBitmap_throws() {
    Bitmap rgbaBitmap = TestImageCreator.createRgbaBitmap();
    MlImage image = new BitmapMlImageBuilder(rgbaBitmap).build();

    assertThrows(IllegalArgumentException.class, () -> ByteBufferExtractor.extract(image));
  }

  @Test
  public void extract_rgbFromRgbByteBuffer_succeeds() {
    ByteBuffer buffer = TestImageCreator.createRgbBuffer();
    MlImage image =
        new ByteBufferMlImageBuilder(
                buffer,
                TestImageCreator.getWidth(),
                TestImageCreator.getHeight(),
                MlImage.IMAGE_FORMAT_RGB)
            .build();

    ByteBuffer result = ByteBufferExtractor.extract(image, MlImage.IMAGE_FORMAT_RGB);

    assertThat(result.isReadOnly()).isTrue();
    assertThat(result).isEquivalentAccordingToCompareTo(TestImageCreator.createRgbBuffer());
  }

  @Test
  public void extract_rgbFromRgbaByteBuffer_succeeds() {
    ByteBuffer buffer = TestImageCreator.createRgbaBuffer();
    MlImage image =
        new ByteBufferMlImageBuilder(
                buffer,
                TestImageCreator.getWidth(),
                TestImageCreator.getHeight(),
                MlImage.IMAGE_FORMAT_RGBA)
            .build();

    ByteBuffer result = ByteBufferExtractor.extract(image, MlImage.IMAGE_FORMAT_RGB);

    assertThat(result).isEquivalentAccordingToCompareTo(TestImageCreator.createRgbBuffer());
    assertThat(buffer.position()).isEqualTo(0);
  }

  @Test
  public void extract_rgbaFromRgbByteBuffer_succeeds() {
    ByteBuffer buffer = TestImageCreator.createRgbBuffer();
    MlImage image =
        new ByteBufferMlImageBuilder(
                buffer,
                TestImageCreator.getWidth(),
                TestImageCreator.getHeight(),
                MlImage.IMAGE_FORMAT_RGB)
            .build();

    ByteBuffer result = ByteBufferExtractor.extract(image, MlImage.IMAGE_FORMAT_RGBA);

    assertThat(result).isEquivalentAccordingToCompareTo(TestImageCreator.createOpaqueRgbaBuffer());
    assertThat(buffer.position()).isEqualTo(0);
  }

  @Test
  public void extract_rgbFromRgbaBitmap_succeeds() {
    Bitmap rgbaBitmap = TestImageCreator.createRgbaBitmap();
    MlImage image = new BitmapMlImageBuilder(rgbaBitmap).build();

    ByteBuffer result = ByteBufferExtractor.extract(image, MlImage.IMAGE_FORMAT_RGB);

    assertThat(result.isReadOnly()).isTrue();
    assertThat(result).isEquivalentAccordingToCompareTo(TestImageCreator.createRgbBuffer());

    // Verifies ByteBuffer is cached inside MlImage.
    ByteBufferImageContainer byteBufferImageContainer =
        (ByteBufferImageContainer) image.getContainer(MlImage.STORAGE_TYPE_BYTEBUFFER);
    assertThat(byteBufferImageContainer.getByteBuffer()).isEqualTo(result);
    assertThat(byteBufferImageContainer.getImageFormat()).isEqualTo(MlImage.IMAGE_FORMAT_RGB);

    // Verifies that extracted ByteBuffer is the cached one.
    ByteBuffer result2 = ByteBufferExtractor.extract(image, MlImage.IMAGE_FORMAT_RGB);
    assertThat(result2).isEqualTo(result);
  }

  @Test
  public void extract_unsupportedFormatFromByteBuffer_throws() {
    ByteBuffer buffer = TestImageCreator.createRgbaBuffer();
    MlImage image =
        new ByteBufferMlImageBuilder(
                buffer,
                TestImageCreator.getWidth(),
                TestImageCreator.getHeight(),
                MlImage.IMAGE_FORMAT_RGBA)
            .build();

    assertThrows(
        IllegalArgumentException.class,
        () -> ByteBufferExtractor.extract(image, MlImage.IMAGE_FORMAT_YUV_420_888));
  }

  @Test
  public void extractInRecommendedFormat_anyFormatFromRgbByteBuffer_succeeds() {
    ByteBuffer buffer = TestImageCreator.createRgbBuffer();
    MlImage image =
        new ByteBufferMlImageBuilder(
                buffer,
                TestImageCreator.getWidth(),
                TestImageCreator.getHeight(),
                MlImage.IMAGE_FORMAT_RGB)
            .build();

    ByteBufferExtractor.Result result = ByteBufferExtractor.extractInRecommendedFormat(image);

    assertThat(result.buffer().isReadOnly()).isTrue();
    assertThat(result.format()).isEqualTo(MlImage.IMAGE_FORMAT_RGB);

    // Verifies ByteBuffer is cached inside MlImage.
    ByteBufferImageContainer byteBufferImageContainer =
        (ByteBufferImageContainer) image.getContainer(MlImage.STORAGE_TYPE_BYTEBUFFER);
    assertThat(byteBufferImageContainer.getByteBuffer()).isEqualTo(result.buffer());
    assertThat(byteBufferImageContainer.getImageFormat()).isEqualTo(MlImage.IMAGE_FORMAT_RGB);

    // Verifies that extracted ByteBuffer is the cached one.
    ByteBufferExtractor.Result result2 = ByteBufferExtractor.extractInRecommendedFormat(image);
    assertThat(result2.buffer()).isEqualTo(result.buffer());
    assertThat(result2.format()).isEqualTo(result.format());
  }
}
