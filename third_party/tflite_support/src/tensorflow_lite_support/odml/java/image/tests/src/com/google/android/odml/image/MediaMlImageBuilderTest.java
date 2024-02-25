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
import static org.mockito.Mockito.when;

import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.media.Image;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link MediaMlImageBuilder} */
@RunWith(RobolectricTestRunner.class)
public final class MediaMlImageBuilderTest {
  private static final int HEIGHT = 100;
  private static final int WIDTH = 50;

  @Mock private Image mediaImage;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);

    when(mediaImage.getHeight()).thenReturn(HEIGHT);
    when(mediaImage.getWidth()).thenReturn(WIDTH);
    when(mediaImage.getFormat()).thenReturn(ImageFormat.YUV_420_888);
  }

  @Test
  public void build_fromMediaImage_succeeds() {
    MlImage image = new MediaMlImageBuilder(mediaImage).build();
    ImageContainer container = image.getContainer(MlImage.STORAGE_TYPE_MEDIA_IMAGE);

    assertThat(image.getWidth()).isEqualTo(WIDTH);
    assertThat(image.getHeight()).isEqualTo(HEIGHT);
    assertThat(image.getRoi()).isEqualTo(new Rect(0, 0, WIDTH, HEIGHT));
    assertThat(image.getRotation()).isEqualTo(0);
    assertThat(image.getTimestamp()).isAtLeast(0);
    assertThat(image.getContainedImageProperties())
        .containsExactly(
            ImageProperties.builder()
                .setStorageType(MlImage.STORAGE_TYPE_MEDIA_IMAGE)
                .setImageFormat(MlImage.IMAGE_FORMAT_YUV_420_888)
                .build());
    assertThat(((MediaImageContainer) container).getImage().getFormat())
        .isEqualTo(ImageFormat.YUV_420_888);
  }

  @Test
  public void build_withOptionalProperties_succeeds() {
    MlImage image =
        new MediaMlImageBuilder(mediaImage)
            .setTimestamp(12345)
            .setRoi(new Rect(0, 5, 10, 15))
            .setRotation(90)
            .build();

    assertThat(image.getTimestamp()).isEqualTo(12345);
    assertThat(image.getRotation()).isEqualTo(90);
    assertThat(image.getRoi()).isEqualTo(new Rect(0, 5, 10, 15));
  }

  @Test
  public void build_withInvalidRotation_throwsException() {
    MediaMlImageBuilder builder = new MediaMlImageBuilder(mediaImage);

    assertThrows(IllegalArgumentException.class, () -> builder.setRotation(360));
  }
}
