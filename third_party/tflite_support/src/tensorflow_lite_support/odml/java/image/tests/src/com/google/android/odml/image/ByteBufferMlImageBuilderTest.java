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

import android.graphics.Rect;
import java.nio.ByteBuffer;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link ByteBufferMlImageBuilder} */
@RunWith(RobolectricTestRunner.class)
public final class ByteBufferMlImageBuilderTest {

  @Test
  public void build_fromByteBuffer_succeeds() {
    ByteBuffer buffer = ByteBuffer.allocate(500);

    MlImage image = new ByteBufferMlImageBuilder(buffer, 20, 25, MlImage.IMAGE_FORMAT_RGB).build();
    ImageContainer container = image.getContainer(MlImage.STORAGE_TYPE_BYTEBUFFER);

    assertThat(image.getWidth()).isEqualTo(20);
    assertThat(image.getHeight()).isEqualTo(25);
    assertThat(image.getRoi()).isEqualTo(new Rect(0, 0, 20, 25));
    assertThat(image.getRotation()).isEqualTo(0);
    assertThat(image.getContainedImageProperties())
        .containsExactly(
            ImageProperties.builder()
                .setStorageType(MlImage.STORAGE_TYPE_BYTEBUFFER)
                .setImageFormat(MlImage.IMAGE_FORMAT_RGB)
                .build());
    assertThat(((ByteBufferImageContainer) container).getImageFormat())
        .isEqualTo(MlImage.IMAGE_FORMAT_RGB);
  }

  @Test
  public void build_withOptionalProperties_succeeds() {
    ByteBuffer buffer = ByteBuffer.allocate(500);

    MlImage image =
        new ByteBufferMlImageBuilder(buffer, 20, 25, MlImage.IMAGE_FORMAT_RGB)
            .setRoi(new Rect(0, 5, 10, 15))
            .setRotation(90)
            .setTimestamp(12345)
            .build();

    assertThat(image.getTimestamp()).isEqualTo(12345);
    assertThat(image.getRotation()).isEqualTo(90);
    assertThat(image.getRoi()).isEqualTo(new Rect(0, 5, 10, 15));
  }

  @Test
  public void build_withInvalidRotation_throwsException() {
    ByteBuffer buffer = ByteBuffer.allocate(500);
    ByteBufferMlImageBuilder builder =
        new ByteBufferMlImageBuilder(buffer, 20, 25, MlImage.IMAGE_FORMAT_RGB);

    assertThrows(IllegalArgumentException.class, () -> builder.setRotation(360));
  }
}
