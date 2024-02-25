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
import java.nio.ByteBuffer;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** Unit test for {@link BitmapExtractor}. */
@RunWith(RobolectricTestRunner.class)
public class BitmapExtractorTest {

  @Test
  public void extract_fromBitmap_succeeds() {
    Bitmap bitmap = TestImageCreator.createRgbaBitmap();
    MlImage image = new BitmapMlImageBuilder(bitmap).build();

    Bitmap result = BitmapExtractor.extract(image);

    assertThat(result).isSameInstanceAs(bitmap);
  }

  @Test
  public void extract_fromByteBuffer_throwsException() {
    ByteBuffer buffer = TestImageCreator.createRgbBuffer();
    MlImage image =
        new ByteBufferMlImageBuilder(
                buffer,
                TestImageCreator.getWidth(),
                TestImageCreator.getHeight(),
                MlImage.IMAGE_FORMAT_RGB)
            .build();

    assertThrows(IllegalArgumentException.class, () -> BitmapExtractor.extract(image));
  }
}
