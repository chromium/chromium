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

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.ImageFormat;
import android.media.Image;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link MediaImageExtractor} */
@RunWith(RobolectricTestRunner.class)
public final class MediaImageExtractorTest {
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
  public void extract_fromMediaMlImage_succeeds() {
    MlImage image = new MediaMlImageBuilder(mediaImage).build();
    Image extractedMediaImage = MediaImageExtractor.extract(image);

    assertThat(extractedMediaImage).isSameInstanceAs(image);
  }

  @Test
  public void extract_fromBitmapMlImage_throwsException() {
    MlImage image =
        new BitmapMlImageBuilder(
                Bitmap.createBitmap(/* width= */ 20, /* height= */ 25, Config.ARGB_8888))
            .build();
    assertThrows(IllegalArgumentException.class, () -> MediaImageExtractor.extract(image));
  }
}
