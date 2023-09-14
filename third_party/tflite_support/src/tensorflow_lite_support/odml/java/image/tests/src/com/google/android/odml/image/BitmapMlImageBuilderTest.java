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
import android.graphics.Bitmap.Config;
import android.graphics.Rect;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link BitmapMlImageBuilder} */
@RunWith(RobolectricTestRunner.class)
public final class BitmapMlImageBuilderTest {

  @Test
  public void build_fromBitmap_succeeds() {
    Bitmap bitmap = Bitmap.createBitmap(20, 25, Config.ARGB_8888);

    MlImage image = new BitmapMlImageBuilder(bitmap).build();
    ImageContainer container = image.getContainer(MlImage.STORAGE_TYPE_BITMAP);

    assertThat(image.getWidth()).isEqualTo(20);
    assertThat(image.getHeight()).isEqualTo(25);
    assertThat(image.getContainedImageProperties())
        .containsExactly(
            ImageProperties.builder()
                .setImageFormat(MlImage.IMAGE_FORMAT_RGBA)
                .setStorageType(MlImage.STORAGE_TYPE_BITMAP)
                .build());
    assertThat(((BitmapImageContainer) container).getBitmap().getConfig())
        .isEqualTo(Config.ARGB_8888);
  }

  @Test
  public void build_withOptionalProperties_succeeds() {
    Bitmap bitmap = Bitmap.createBitmap(20, 25, Config.ARGB_8888);

    MlImage image =
        new BitmapMlImageBuilder(bitmap)
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
    Bitmap bitmap = Bitmap.createBitmap(20, 25, Config.ARGB_8888);
    BitmapMlImageBuilder builder = new BitmapMlImageBuilder(bitmap);

    assertThrows(IllegalArgumentException.class, () -> builder.setRotation(360));
  }

  @Test
  public void release_recyclesBitmap() {
    Bitmap bitmap = Bitmap.createBitmap(20, 25, Config.ARGB_8888);

    MlImage image =
        new BitmapMlImageBuilder(bitmap)
            .setRoi(new Rect(0, 5, 10, 15))
            .setRotation(90)
            .setTimestamp(12345)
            .build();
    assertThat(bitmap.isRecycled()).isFalse();
    image.close();

    assertThat(bitmap.isRecycled()).isTrue();
  }
}
