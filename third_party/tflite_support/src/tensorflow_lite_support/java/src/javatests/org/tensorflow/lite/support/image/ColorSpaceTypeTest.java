/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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
import static org.tensorflow.lite.support.image.TestImageCreator.createRgbBitmap;
import static org.tensorflow.lite.support.image.TestImageCreator.createRgbTensorBuffer;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.ImageFormat;
import java.util.Arrays;
import java.util.Collection;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ErrorCollector;
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
@SuiteClasses({
  ColorSpaceTypeTest.ValidShapeTest.class,
  ColorSpaceTypeTest.InvalidShapeTest.class,
  ColorSpaceTypeTest.BitmapConfigTest.class,
  ColorSpaceTypeTest.ImageFormatTest.class,
  ColorSpaceTypeTest.YuvImageTest.class,
  ColorSpaceTypeTest.AssertNumElementsTest.class,
  ColorSpaceTypeTest.General.class
})
public class ColorSpaceTypeTest {

  /** Parameterized tests for valid shapes. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class ValidShapeTest extends ColorSpaceTypeTest {

    @Parameter(0)
    public ColorSpaceType colorSpaceType;

    /** The shape that matches the colorSpaceType. */
    @Parameter(1)
    public int[] validShape;

    /** The height of validShape. */
    @Parameter(2)
    public int expectedHeight;

    /** The width of validShape. */
    @Parameter(3)
    public int expectedWidth;

    @Parameters(name = "colorSpaceType={0}; validShape={1}; height={2}; width={3}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {ColorSpaceType.RGB, new int[] {1, 10, 20, 3}, 10, 20},
            {ColorSpaceType.RGB, new int[] {10, 20, 3}, 10, 20},
            {ColorSpaceType.GRAYSCALE, new int[] {10, 20}, 10, 20},
            {ColorSpaceType.GRAYSCALE, new int[] {1, 10, 20, 1}, 10, 20},
          });
    }

    @Test
    public void getHeightSucceedsWithValidShape() {
      assertThat(colorSpaceType.getHeight(validShape)).isEqualTo(expectedHeight);
    }

    @Test
    public void getWidthSucceedsWithValidShape() {
      assertThat(colorSpaceType.getWidth(validShape)).isEqualTo(expectedWidth);
    }
  }

  /** Parameterized tests for invalid shapes. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class InvalidShapeTest extends ColorSpaceTypeTest {

    private static final String RGB_ASSERT_SHAPE_MESSAGE =
        "The shape of a RGB image should be (h, w, c) or (1, h, w, c), and channels"
            + " representing R, G, B in order. The provided image shape is ";
    private static final String GRAYSCALE_ASSERT_SHAPE_MESSAGE =
        "The shape of a grayscale image should be (h, w) or (1, h, w, 1). The provided image"
            + " shape is ";

    @Parameter(0)
    public ColorSpaceType colorSpaceType;

    /** The shape that does not match the colorSpaceType. */
    @Parameter(1)
    public int[] invalidShape;

    @Parameter(2)
    public String errorMessage;

    @Parameters(name = "colorSpaceType={0}; invalidShape={1}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {ColorSpaceType.RGB, new int[] {2, 10, 20, 3}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {1, 10, 20, 3, 4}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {1, 10, 20, 5}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {1, 10, 20}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {1, -10, 20, 3}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {1, 10, -20, 3}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {10, 20, 3, 4}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {10, 20, 5}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {10, 20}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {-10, 20, 3}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {10, -20, 3}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {2, 10, 20}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {1, 10, 20, 3}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {1, -10, 20}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {1, 10, -20}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {10, 20, 4}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {10}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {-10, 20}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {10, -20}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
          });
    }

    @Test
    public void assertShapeFaislsWithInvalidShape() {
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> colorSpaceType.assertShape(invalidShape));
      assertThat(exception).hasMessageThat().contains(errorMessage + Arrays.toString(invalidShape));
    }

    @Test
    public void getHeightFaislsWithInvalidShape() {
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> colorSpaceType.getHeight(invalidShape));
      assertThat(exception).hasMessageThat().contains(errorMessage + Arrays.toString(invalidShape));
    }

    @Test
    public void getWidthFaislsWithInvalidShape() {
      IllegalArgumentException exception =
          assertThrows(IllegalArgumentException.class, () -> colorSpaceType.getWidth(invalidShape));
      assertThat(exception).hasMessageThat().contains(errorMessage + Arrays.toString(invalidShape));
    }
  }

  /** Parameterized tests for Bitmap Config. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class BitmapConfigTest extends ColorSpaceTypeTest {

    @Parameter(0)
    public ColorSpaceType colorSpaceType;

    /** The Bitmap configuration match the colorSpaceType. */
    @Parameter(1)
    public Config config;

    @Parameters(name = "colorSpaceType={0}; config={1}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {ColorSpaceType.RGB, Config.ARGB_8888},
            {ColorSpaceType.GRAYSCALE, Config.ALPHA_8},
          });
    }

    @Test
    public void fromBitmapConfigSucceedsWithSupportedConfig() {
      assertThat(ColorSpaceType.fromBitmapConfig(config)).isEqualTo(colorSpaceType);
    }

    @Test
    public void toBitmapConfigSucceedsWithSupportedConfig() {
      assertThat(colorSpaceType.toBitmapConfig()).isEqualTo(config);
    }
  }

  /** Parameterized tests for ImageFormat. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class ImageFormatTest extends ColorSpaceTypeTest {

    @Parameter(0)
    public ColorSpaceType colorSpaceType;

    /** The ImageFormat that matches the colorSpaceType. */
    @Parameter(1)
    public int imageFormat;

    @Parameters(name = "colorSpaceType={0}; imageFormat={1}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {ColorSpaceType.NV21, ImageFormat.NV21},
            {ColorSpaceType.YV12, ImageFormat.YV12},
            {ColorSpaceType.YUV_420_888, ImageFormat.YUV_420_888},
          });
    }

    @Test
    public void fromImageFormatSucceedsWithSupportedImageFormat() {
      assertThat(ColorSpaceType.fromImageFormat(imageFormat)).isEqualTo(colorSpaceType);
    }
  }

  /** Parameterized tests for YUV image formats: NV12, NV21, YV12, YV21, YUV_420_888. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class YuvImageTest extends ColorSpaceTypeTest {

    @Parameter(0)
    public ColorSpaceType colorSpaceType;

    @Parameters(name = "colorSpaceType={0}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {ColorSpaceType.NV12},
            {ColorSpaceType.NV21},
            {ColorSpaceType.YV12},
            {ColorSpaceType.YV21},
            {ColorSpaceType.YUV_420_888},
          });
    }

    @Test
    public void convertTensorBufferToBitmapShouldFail() {
      UnsupportedOperationException exception =
          assertThrows(
              UnsupportedOperationException.class,
              () ->
                  colorSpaceType.convertTensorBufferToBitmap(
                      TensorBuffer.createDynamic(DataType.FLOAT32)));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "convertTensorBufferToBitmap() is unsupported for the color space type "
                  + colorSpaceType.name());
    }

    @Test
    public void getWidthShouldFail() {
      UnsupportedOperationException exception =
          assertThrows(
              UnsupportedOperationException.class, () -> colorSpaceType.getWidth(new int[] {}));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "getWidth() only supports RGB and GRAYSCALE formats, but not "
                  + colorSpaceType.name());
    }

    @Test
    public void getHeightShouldFail() {
      UnsupportedOperationException exception =
          assertThrows(
              UnsupportedOperationException.class, () -> colorSpaceType.getHeight(new int[] {}));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "getHeight() only supports RGB and GRAYSCALE formats, but not "
                  + colorSpaceType.name());
    }

    @Test
    public void assertShapeShouldFail() {
      UnsupportedOperationException exception =
          assertThrows(
              UnsupportedOperationException.class, () -> colorSpaceType.assertShape(new int[] {}));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "assertShape() only supports RGB and GRAYSCALE formats, but not "
                  + colorSpaceType.name());
    }

    @Test
    public void getChannelValueShouldFail() {
      UnsupportedOperationException exception =
          assertThrows(UnsupportedOperationException.class, () -> colorSpaceType.getChannelValue());
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "getChannelValue() is unsupported for the color space type " + colorSpaceType.name());
    }

    @Test
    public void getNormalizedShapeShouldFail() {
      UnsupportedOperationException exception =
          assertThrows(
              UnsupportedOperationException.class,
              () -> colorSpaceType.getNormalizedShape(new int[] {}));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "getNormalizedShape() is unsupported for the color space type "
                  + colorSpaceType.name());
    }

    @Test
    public void getShapeInfoMessageShouldFail() {
      UnsupportedOperationException exception =
          assertThrows(
              UnsupportedOperationException.class, () -> colorSpaceType.getShapeInfoMessage());
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "getShapeInfoMessage() is unsupported for the color space type "
                  + colorSpaceType.name());
    }

    @Test
    public void toBitmapConfigShouldFail() {
      UnsupportedOperationException exception =
          assertThrows(UnsupportedOperationException.class, () -> colorSpaceType.toBitmapConfig());
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "toBitmapConfig() is unsupported for the color space type " + colorSpaceType.name());
    }
  }

  /** Parameterized tests for assertNumElements/getNumElements with all image formats. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class AssertNumElementsTest extends ColorSpaceTypeTest {
    private static final int HEIGHT = 2;
    private static final int WIDTH = 3;
    private static final int LESS_NUM_ELEMENTS = 5; // less than expected
    private static final int MORE_NUM_ELEMENTS = 20; // more than expected. OK.
    @Rule public ErrorCollector errorCollector = new ErrorCollector();

    @Parameter(0)
    public ColorSpaceType colorSpaceType;

    @Parameter(1)
    public int expectedNumElements;

    @Parameters(name = "colorSpaceType={0};expectedNumElements={1}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {ColorSpaceType.RGB, 18},
            {ColorSpaceType.GRAYSCALE, 6},
            {ColorSpaceType.NV12, 10},
            {ColorSpaceType.NV21, 10},
            {ColorSpaceType.YV12, 10},
            {ColorSpaceType.YV21, 10},
          });
    }

    @Test
    public void getNumElementsShouldSucceedWithExpectedNumElements() {
      assertThat(colorSpaceType.getNumElements(HEIGHT, WIDTH)).isEqualTo(expectedNumElements);
    }

    @Test
    public void assertNumElementsShouldSucceedWithMoreNumElements() {
      errorCollector.checkSucceeds(
          () -> {
            colorSpaceType.assertNumElements(MORE_NUM_ELEMENTS, HEIGHT, WIDTH);
            return null;
          });
    }

    @Test
    public void assertNumElementsShouldFailWithLessNumElements() {
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () -> colorSpaceType.assertNumElements(LESS_NUM_ELEMENTS, HEIGHT, WIDTH));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              String.format(
                  "The given number of elements (%d) does not match the image (%s) in %d x %d. The"
                      + " expected number of elements should be at least %d.",
                  LESS_NUM_ELEMENTS, colorSpaceType.name(), HEIGHT, WIDTH, expectedNumElements));
    }
  }

  /** General tests of ColorSpaceTypeTest. */
  @RunWith(RobolectricTestRunner.class)
  public static final class General extends ColorSpaceTypeTest {

    @Test
    public void convertTensorBufferToBitmapShouldSuccessWithRGB() {
      TensorBuffer buffer = createRgbTensorBuffer(DataType.UINT8, false);
      Bitmap bitmap = ColorSpaceType.RGB.convertTensorBufferToBitmap(buffer);

      Bitmap expectedBitmap = createRgbBitmap();
      assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
    }

    @Test
    public void fromBitmapConfigFailsWithUnsupportedConfig() {
      Config unsupportedConfig = Config.ARGB_4444;
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () -> ColorSpaceType.fromBitmapConfig(unsupportedConfig));
      assertThat(exception)
          .hasMessageThat()
          .contains("Bitmap configuration: " + unsupportedConfig + ", is not supported yet.");
    }
  }
}
