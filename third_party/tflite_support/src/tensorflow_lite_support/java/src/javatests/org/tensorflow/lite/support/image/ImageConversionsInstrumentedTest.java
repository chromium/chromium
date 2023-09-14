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

import static android.graphics.Bitmap.Config.ARGB_8888;
import static android.graphics.Color.BLACK;
import static android.graphics.Color.BLUE;
import static android.graphics.Color.GREEN;
import static android.graphics.Color.RED;
import static android.graphics.Color.WHITE;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.tensorflow.lite.support.image.ImageConversions.convertGrayscaleTensorBufferToBitmap;

import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.io.IOException;
import java.util.Arrays;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Suite;
import org.junit.runners.Suite.SuiteClasses;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Instrumented unit test for {@link ImageConversions}. */
@RunWith(Suite.class)
@SuiteClasses({
  ImageConversionsInstrumentedTest.TensorBufferToBitmap.class,
  ImageConversionsInstrumentedTest.BitmapToTensorBuffer.class
})
public class ImageConversionsInstrumentedTest {

  /** Tests for the TensorBuffer data type and normalized form. */
  // Note that parameterized test with android_library_instrumentation_tests is currently not
  // supported internally.
  @RunWith(AndroidJUnit4.class)
  public static final class TensorBufferToBitmap extends ImageConversionsInstrumentedTest {

    @Test
    public void convertGrayscaleTensorBufferToBitmapShouldSuccessWithFloatNormalized() {
      DataType dataType = DataType.FLOAT32;
      boolean isNormalized = true;

      TensorBuffer buffer = TestImageCreator.createGrayscaleTensorBuffer(dataType, isNormalized);
      Bitmap bitmap = convertGrayscaleTensorBufferToBitmap(buffer);

      Bitmap expectedBitmap = TestImageCreator.createGrayscaleBitmap();
      assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
    }

    @Test
    public void convertGrayscaleTensorBufferToBitmapShouldSuccessWithFloatUnnormalized() {
      DataType dataType = DataType.FLOAT32;
      boolean isNormalized = false;

      TensorBuffer buffer = TestImageCreator.createGrayscaleTensorBuffer(dataType, isNormalized);
      Bitmap bitmap = convertGrayscaleTensorBufferToBitmap(buffer);

      Bitmap expectedBitmap = TestImageCreator.createGrayscaleBitmap();
      assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
    }

    @Test
    public void convertGrayscaleTensorBufferToBitmapShouldSuccessWithUint8Normalized() {
      DataType dataType = DataType.UINT8;
      boolean isNormalized = true;

      TensorBuffer buffer = TestImageCreator.createGrayscaleTensorBuffer(dataType, isNormalized);
      Bitmap bitmap = convertGrayscaleTensorBufferToBitmap(buffer);

      Bitmap expectedBitmap = TestImageCreator.createGrayscaleBitmap();
      assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
    }

    @Test
    public void convertGrayscaleTensorBufferToBitmapShouldSuccessWithUint8Unnormalized() {
      DataType dataType = DataType.UINT8;
      boolean isNormalized = false;

      TensorBuffer buffer = TestImageCreator.createGrayscaleTensorBuffer(dataType, isNormalized);
      Bitmap bitmap = convertGrayscaleTensorBufferToBitmap(buffer);

      Bitmap expectedBitmap = TestImageCreator.createGrayscaleBitmap();
      assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
    }

    @Test
    public void convertGrayscaleTensorBufferToBitmapShouldRejectBufferWithInvalidShapeWithFloat() {
      DataType dataType = DataType.FLOAT32;
      TensorBuffer buffer = TensorBuffer.createFixedSize(new int[] {2, 5, 10}, dataType);

      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> convertGrayscaleTensorBufferToBitmap(buffer));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "The shape of a grayscale image should be (h, w) or (1, h, w, 1). The provided image"
                  + " shape is "
                  + Arrays.toString(buffer.getShape()));
    }

    @Test
    public void convertGrayscaleTensorBufferToBitmapShouldRejectBufferWithInvalidShapeWithUint8() {
      DataType dataType = DataType.UINT8;
      TensorBuffer buffer = TensorBuffer.createFixedSize(new int[] {2, 5, 10}, dataType);

      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> convertGrayscaleTensorBufferToBitmap(buffer));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "The shape of a grayscale image should be (h, w) or (1, h, w, 1). The provided image"
                  + " shape is "
                  + Arrays.toString(buffer.getShape()));
    }
  }

  /** BitmapToTensorBuffer tests of ImageConversionsInstrumentedTest. */
  @RunWith(AndroidJUnit4.class)
  public static final class BitmapToTensorBuffer extends ImageConversionsInstrumentedTest {

    private Bitmap greyGrid;
    private Bitmap colorGrid;
    private TensorBuffer buffer;

    static final String GREY_GRID_PATH = "grey_grid.png";
    static final String COLOR_GRID_PATH = "color_grid.png";

    @Before
    public void loadAssets() {
      Context context = ApplicationProvider.getApplicationContext();
      AssetManager assetManager = context.getAssets();
      try {
        greyGrid = BitmapFactory.decodeStream(assetManager.open(GREY_GRID_PATH));
        colorGrid = BitmapFactory.decodeStream(assetManager.open(COLOR_GRID_PATH));
      } catch (IOException e) {
        Log.e("Test", "Cannot load asset files");
      }
      Assert.assertEquals(ARGB_8888, greyGrid.getConfig());
      Assert.assertEquals(ARGB_8888, colorGrid.getConfig());
      buffer = TensorBuffer.createDynamic(DataType.UINT8);
    }

    @Test
    public void testBitmapDimensionLayout() {
      // This test is not only for proving the correctness of bitmap -> TensorBuffer conversion, but
      // also for us to better understand how Android Bitmap is storing pixels - height first or
      // width first.
      // We use a black image which has a white corner to understand what happens. By setting up the
      // correct loop to pass the test, we can reveal the order of pixels returned from `getPixels`.
      // The result shows that Android stores bitmap in an h-first manner. The returned array of
      // `getPixels` is like [ 1st row, 2nd row, ... ] which is the same with TFLite.
      Assert.assertEquals(100, greyGrid.getWidth());
      Assert.assertEquals(100, greyGrid.getHeight());
      Assert.assertEquals(BLACK, greyGrid.getPixel(25, 25)); // left top
      Assert.assertEquals(BLACK, greyGrid.getPixel(75, 25)); // right top
      Assert.assertEquals(WHITE, greyGrid.getPixel(25, 75)); // left bottom
      Assert.assertEquals(BLACK, greyGrid.getPixel(75, 75)); // right bottom

      ImageConversions.convertBitmapToTensorBuffer(greyGrid, buffer);
      Assert.assertArrayEquals(new int[] {100, 100, 3}, buffer.getShape());
      Assert.assertEquals(DataType.UINT8, buffer.getDataType());

      int[] pixels = buffer.getIntArray();
      int index = 0;
      for (int h = 0; h < 100; h++) {
        for (int w = 0; w < 100; w++) {
          int expected = (w < 50 && h >= 50) ? 255 : 0;
          Assert.assertEquals(expected, pixels[index++]);
          Assert.assertEquals(expected, pixels[index++]);
          Assert.assertEquals(expected, pixels[index++]);
        }
      }
    }

    @Test
    public void testBitmapARGB8888ChannelLayout() {
      // This test is not only for proving the correctness of bitmap -> TensorBuffer conversion, but
      // also for us to better understand how Android Bitmap is storing pixels - RGB channel or
      // other possible ordering.
      // We use an colored grid image to understand what happens. It's a simple grid image with 4
      // grid in different colors. Passed through our Bitmap -> TensorBuffer conversion which simply
      // unpack channels from an integer returned from `getPixel`, its channel sequence could be
      // revealed directly.
      // The result shows that Android Bitmap has no magic when loading channels. If loading from
      // PNG images, channel order still remains R-G-B.
      Assert.assertEquals(100, colorGrid.getWidth());
      Assert.assertEquals(100, colorGrid.getHeight());
      Assert.assertEquals(BLUE, colorGrid.getPixel(25, 25)); // left top
      Assert.assertEquals(BLACK, colorGrid.getPixel(75, 25)); // right top
      Assert.assertEquals(GREEN, colorGrid.getPixel(25, 75)); // left bottom
      Assert.assertEquals(RED, colorGrid.getPixel(75, 75)); // right bottom

      ImageConversions.convertBitmapToTensorBuffer(colorGrid, buffer);
      Assert.assertArrayEquals(new int[] {100, 100, 3}, buffer.getShape());
      Assert.assertEquals(DataType.UINT8, buffer.getDataType());

      int[] pixels = buffer.getIntArray();
      Assert.assertArrayEquals(new int[] {0, 0, 255}, getChannels(pixels, 25, 25)); // left top
      Assert.assertArrayEquals(new int[] {0, 0, 0}, getChannels(pixels, 25, 75)); // right top
      Assert.assertArrayEquals(new int[] {0, 255, 0}, getChannels(pixels, 75, 25)); // left bottom
      Assert.assertArrayEquals(new int[] {255, 0, 0}, getChannels(pixels, 75, 75)); // right bottom
    }

    /** Helper function only for {@link #testBitmapARGB8888ChannelLayout()}. */
    private static int[] getChannels(int[] pixels, int h, int w) {
      int id = (h * 100 + w) * 3;
      return new int[] {pixels[id++], pixels[id++], pixels[id]};
    }
  }
}
