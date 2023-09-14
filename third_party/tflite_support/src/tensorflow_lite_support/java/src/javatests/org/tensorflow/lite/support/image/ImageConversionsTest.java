/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.support.image;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.tensorflow.lite.support.image.ImageConversions.convertBitmapToTensorBuffer;
import static org.tensorflow.lite.support.image.ImageConversions.convertRgbTensorBufferToBitmap;

import android.graphics.Bitmap;
import java.util.Arrays;
import java.util.Collection;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Suite;
import org.junit.runners.Suite.SuiteClasses;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Tests of {@link ImageConversions}. */
@RunWith(Suite.class)
@SuiteClasses({ImageConversionsTest.TensorBufferToBitmap.class, ImageConversionsTest.General.class})
public class ImageConversionsTest {

  /** Parameterized tests for the TensorBuffer data type and normalized form. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class TensorBufferToBitmap extends ImageConversionsTest {

    /** The data type that used to create the TensorBuffer. */
    @Parameter(0)
    public DataType dataType;

    /** Indicates whether the shape is in the normalized form of (1, h, w, 3). */
    @Parameter(1)
    public boolean isNormalized;

    @Parameters(name = "dataType={0}; isNormalized={1}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {DataType.FLOAT32, true}, {DataType.UINT8, true},
            {DataType.FLOAT32, false}, {DataType.UINT8, false},
          });
    }

    @Test
    public void convertRgbTensorBufferToBitmapShouldSuccess() {
      TensorBuffer buffer = TestImageCreator.createRgbTensorBuffer(dataType, isNormalized);
      Bitmap bitmap = convertRgbTensorBufferToBitmap(buffer);

      Bitmap expectedBitmap = TestImageCreator.createRgbBitmap();
      assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
    }

    @Test
    public void convertRgbTensorBufferToBitmapShouldRejectBufferWithInvalidShape() {
      TensorBuffer buffer = TensorBuffer.createFixedSize(new int[] {2, 5, 10, 3}, dataType);

      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> convertRgbTensorBufferToBitmap(buffer));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "The shape of a RGB image should be (h, w, c) or (1, h, w, c), and channels"
                  + " representing R, G, B in order. The provided image shape is "
                  + Arrays.toString(buffer.getShape()));
    }
  }

  /** General tests of ImageConversionsTest. */
  @RunWith(RobolectricTestRunner.class)
  public static final class General extends ImageConversionsTest {

    private static final Bitmap rgbBitmap = TestImageCreator.createRgbBitmap();
    private static final TensorBuffer rgbTensorBuffer =
        TestImageCreator.createRgbTensorBuffer(DataType.UINT8, false);

    @Test
    public void convertBitmapToTensorBufferShouldSuccess() {
      TensorBuffer intBuffer = TensorBuffer.createFixedSize(new int[] {10, 10, 3}, DataType.UINT8);
      convertBitmapToTensorBuffer(rgbBitmap, intBuffer);
      assertThat(areEqualIntTensorBuffer(intBuffer, rgbTensorBuffer)).isTrue();
    }

    @Test
    public void convertBitmapToTensorBufferShouldThrowShapeNotExactlySame() {
      TensorBuffer intBuffer = TensorBuffer.createFixedSize(new int[] {5, 20, 3}, DataType.UINT8);
      Assert.assertThrows(
          IllegalArgumentException.class, () -> convertBitmapToTensorBuffer(rgbBitmap, intBuffer));
    }

    @Test
    public void convertBitmapToTensorBufferShouldCastIntToFloatIfNeeded() {
      TensorBuffer floatBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
      convertBitmapToTensorBuffer(rgbBitmap, floatBuffer);
      assertThat(areEqualIntTensorBuffer(floatBuffer, rgbTensorBuffer)).isTrue();
    }
  }

  private static boolean areEqualIntTensorBuffer(TensorBuffer tb1, TensorBuffer tb2) {
    if (!Arrays.equals(tb1.getShape(), tb2.getShape())) {
      return false;
    }
    int[] arr1 = tb1.getIntArray();
    int[] arr2 = tb2.getIntArray();
    return Arrays.equals(arr1, arr2);
  }
}
