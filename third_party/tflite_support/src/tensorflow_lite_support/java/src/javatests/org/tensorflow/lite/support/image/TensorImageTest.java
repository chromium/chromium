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
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.when;
import static org.tensorflow.lite.DataType.FLOAT32;
import static org.tensorflow.lite.DataType.UINT8;
import static org.tensorflow.lite.support.image.TestImageCreator.createGrayscaleBitmap;
import static org.tensorflow.lite.support.image.TestImageCreator.createGrayscaleTensorBuffer;
import static org.tensorflow.lite.support.image.TestImageCreator.createRgbBitmap;
import static org.tensorflow.lite.support.image.TestImageCreator.createRgbTensorBuffer;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.graphics.ImageFormat;
import android.media.Image;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Collection;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Suite;
import org.junit.runners.Suite.SuiteClasses;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/** Tests of {@link org.tensorflow.lite.support.image.TensorImage}. */
@RunWith(Suite.class)
@SuiteClasses({
  TensorImageTest.General.class,
  TensorImageTest.LoadTensorBufferWithRgbAndGrayscale.class,
  TensorImageTest.LoadTensorBufferWithInvalidShapeTest.class,
  TensorImageTest.LoadTensorBufferWithYUV.class,
  TensorImageTest.LoadTensorBufferWithImageProperties.class
})
public class TensorImageTest {

  @RunWith(RobolectricTestRunner.class)
  public static final class General extends TensorImageTest {

    private static final Bitmap exampleBitmap = createExampleBitmap();
    private static final float[] exampleFloatPixels = createExampleFloatPixels();
    private static final int[] exampleUint8Pixels = createExampleUint8Pixels();

    private static final int EXAMPLE_WIDTH = 5;
    private static final int EXAMPLE_HEIGHT = 10;
    private static final int EXAMPLE_NUM_PIXELS = EXAMPLE_HEIGHT * EXAMPLE_WIDTH;
    private static final int EXAMPLE_NUM_CHANNELS = 3;
    private static final int[] EXAMPLE_SHAPE = {
      EXAMPLE_HEIGHT, EXAMPLE_WIDTH, EXAMPLE_NUM_CHANNELS
    };
    private static final float MEAN = 127.5f;
    private static final float STDDEV = 127.5f;

    @Mock Image imageMock;

    @Before
    public void setUp() {
      MockitoAnnotations.initMocks(this);
    }

    @Test
    public void defaultConstructorCreatesUint8TensorImage() {
      TensorImage image = new TensorImage();
      assertThat(image.getDataType()).isEqualTo(UINT8);
    }

    @Test
    public void createFromSucceedsWithUint8TensorImage() {
      TensorImage uint8Image = new TensorImage(UINT8);
      uint8Image.load(new int[] {1, 2, 3, 4, -5, 600}, new int[] {2, 1, 3});

      TensorImage floatImage = TensorImage.createFrom(uint8Image, FLOAT32);
      float[] pixels = floatImage.getTensorBuffer().getFloatArray();
      assertThat(pixels).isEqualTo(new float[] {1.0f, 2.0f, 3.0f, 4.0f, 0.0f, 255.0f});
    }

    @Test
    public void createFromSucceedsWithFloatTensorImage() {
      TensorImage floatImage = new TensorImage(FLOAT32);
      floatImage.load(new float[] {1, 2.495f, 3.5f, 4.5f, -5, 600}, new int[] {2, 1, 3});

      TensorImage uint8Image = TensorImage.createFrom(floatImage, UINT8);
      int[] pixels = uint8Image.getTensorBuffer().getIntArray();
      assertThat(pixels).isEqualTo(new int[] {1, 2, 3, 4, 0, 255});
    }

    @Test
    public void loadBitmapSucceedsWithUint8TensorImage() {
      Bitmap rgbBitmap = createRgbBitmap();
      TensorBuffer rgbTensorBuffer = createRgbTensorBuffer(UINT8, false, 0.0f);
      TensorImage uint8Image = new TensorImage(UINT8);

      uint8Image.load(rgbBitmap);
      assertThat(uint8Image.getBitmap().sameAs(rgbBitmap)).isTrue();
      assertEqualTensorBuffers(uint8Image.getTensorBuffer(), rgbTensorBuffer);
      assertThat(uint8Image.getDataType()).isEqualTo(UINT8);
    }

    @Test
    public void loadBitmapSucceedsWithFloatTensorImage() {
      Bitmap rgbBitmap = createRgbBitmap();
      TensorBuffer rgbTensorBuffer = createRgbTensorBuffer(FLOAT32, false, 0.0f);
      TensorImage floatImage = new TensorImage(FLOAT32);

      floatImage.load(rgbBitmap);
      assertThat(floatImage.getBitmap().sameAs(rgbBitmap)).isTrue();
      assertEqualTensorBuffers(floatImage.getTensorBuffer(), rgbTensorBuffer);
      assertThat(floatImage.getDataType()).isEqualTo(FLOAT32);
    }

    @Test
    public void loadFloatArrayWithUint8TensorImage() {
      TensorImage uint8Image = new TensorImage(UINT8);

      uint8Image.load(exampleFloatPixels, EXAMPLE_SHAPE);
      assertThat(uint8Image.getBitmap()).isNotNull();
      assertThat(uint8Image.getTensorBuffer().getFloatArray())
          .isEqualTo(
              new float
                  [exampleFloatPixels
                      .length]); // All zero because of normalization and casting when loading.
    }

    @Test
    public void loadFloatArrayWithFloatTensorImage() {
      TensorImage floatImage = new TensorImage(FLOAT32);

      floatImage.load(exampleFloatPixels, EXAMPLE_SHAPE);
      assertThat(floatImage.getTensorBuffer().getFloatArray()).isEqualTo(exampleFloatPixels);
    }

    @Test
    public void loadUint8ArrayWithUint8TensorImage() {
      TensorImage uint8Image = new TensorImage(UINT8);

      uint8Image.load(exampleUint8Pixels, EXAMPLE_SHAPE);
      assertThat(uint8Image.getBitmap().sameAs(exampleBitmap)).isTrue();
      assertThat(uint8Image.getTensorBuffer().getIntArray()).isEqualTo(exampleUint8Pixels);
    }

    @Test
    public void loadUint8ArrayWithFloatTensorImage() {
      TensorImage floatImage = new TensorImage(FLOAT32);

      floatImage.load(exampleUint8Pixels, EXAMPLE_SHAPE);
      assertThat(floatImage.getTensorBuffer().getIntArray()).isEqualTo(exampleUint8Pixels);
    }

    @Test
    public void loadTensorBufferWithUint8TensorImage() {
      TensorImage uint8Image = new TensorImage(UINT8);

      uint8Image.load(exampleBitmap);
      TensorBuffer buffer = uint8Image.getTensorBuffer();
      uint8Image.load(buffer);
      assertThat(uint8Image.getBitmap().sameAs(exampleBitmap)).isTrue();
    }

    @Test
    public void loadTensorBufferWithFloatTensorImage() {
      TensorImage floatImage = new TensorImage(FLOAT32);

      floatImage.load(exampleBitmap);
      TensorBuffer buffer = floatImage.getTensorBuffer();
      floatImage.load(buffer);
      assertThat(floatImage.getTensorBuffer().getIntArray()).isEqualTo(exampleUint8Pixels);
    }

    @Test
    public void loadAndGetMediaImageSucceedsWithYuv420888Format() {
      setUpImageMock(imageMock, ImageFormat.YUV_420_888);
      TensorImage tensorImage = new TensorImage(UINT8);

      tensorImage.load(imageMock);
      Image imageReturned = tensorImage.getMediaImage();

      assertThat(imageReturned).isEqualTo(imageMock);
    }

    @Test
    public void loadMediaImageFailsWithNonYuv420888Format() {
      setUpImageMock(imageMock, ImageFormat.YUV_422_888);
      TensorImage tensorImage = new TensorImage(UINT8);

      IllegalArgumentException exception =
          assertThrows(IllegalArgumentException.class, () -> tensorImage.load(imageMock));
      assertThat(exception).hasMessageThat().contains("Only supports loading YUV_420_888 Image.");
    }

    @Test
    public void getBitmapWithUint8TensorImage() {
      TensorImage uint8Image = new TensorImage(UINT8);

      uint8Image.load(exampleBitmap);
      assertThat(uint8Image.getBitmap().sameAs(exampleBitmap)).isTrue();
      // Also check zero copy is effective here (input and output are references of the same
      // object).
      assertThat(uint8Image.getBitmap()).isSameInstanceAs(exampleBitmap);
      // Also check we don't create new Bitmap only with reading operations.
      assertThat(uint8Image.getBuffer().limit())
          .isEqualTo(EXAMPLE_NUM_PIXELS * EXAMPLE_NUM_CHANNELS);
      assertThat(uint8Image.getBitmap()).isSameInstanceAs(exampleBitmap);

      uint8Image.load(exampleUint8Pixels, EXAMPLE_SHAPE);
      assertThat(uint8Image.getBitmap()).isNotSameInstanceAs(exampleBitmap);
    }

    @Test
    public void getBitmapWithFloatTensorImage() {
      TensorImage floatImage = new TensorImage(FLOAT32);

      floatImage.load(exampleBitmap);
      assertThat(floatImage.getBitmap()).isSameInstanceAs(exampleBitmap);
    }

    @Test
    public void getBitmapWithEmptyTensorImage() {
      TensorImage uint8Image = new TensorImage(UINT8);

      assertThrows(IllegalStateException.class, uint8Image::getBitmap);
    }

    @Test
    public void getMediaImageFailsWithBackedBitmap() {
      TensorImage tensorImage = TensorImage.fromBitmap(exampleBitmap);

      UnsupportedOperationException exception =
          assertThrows(UnsupportedOperationException.class, () -> tensorImage.getMediaImage());
      assertThat(exception)
          .hasMessageThat()
          .contains("Converting from Bitmap to android.media.Image is unsupported.");
    }

    @Test
    public void getMediaImageFailsWithBackedTensorBuffer() {
      TensorImage tensorImage = new TensorImage(UINT8);
      tensorImage.load(exampleFloatPixels, EXAMPLE_SHAPE);

      UnsupportedOperationException exception =
          assertThrows(UnsupportedOperationException.class, () -> tensorImage.getMediaImage());
      assertThat(exception)
          .hasMessageThat()
          .contains("Converting from TensorBuffer to android.media.Image is unsupported.");
    }

    @Test
    public void getShapeOfInternalBitmapShouldSuccess() {
      Bitmap bitmap = Bitmap.createBitmap(300, 400, Config.ARGB_8888);
      TensorImage image = TensorImage.fromBitmap(bitmap);

      int width = image.getWidth();
      int height = image.getHeight();

      assertThat(width).isEqualTo(300);
      assertThat(height).isEqualTo(400);
    }

    @Test
    public void getShapeOfInternalTensorBufferShouldSuccess() {
      TensorBuffer buffer = TensorBuffer.createFixedSize(new int[] {1, 400, 300, 3}, UINT8);
      TensorImage image = new TensorImage();
      image.load(buffer);

      int width = image.getWidth();
      int height = image.getHeight();

      assertThat(width).isEqualTo(300);
      assertThat(height).isEqualTo(400);
    }

    @Test
    public void getShapeOfNullImageShouldThrow() {
      TensorImage image = new TensorImage();

      assertThrows(IllegalStateException.class, image::getHeight);
    }

    @Test
    public void getShapeOfACorruptedBufferShouldThrowRatherThanCrash() {
      int[] data = new int[] {1, 2, 3, 4, 5, 6};
      TensorBuffer buffer = TensorBuffer.createDynamic(UINT8);
      buffer.loadArray(data, new int[] {1, 1, 2, 3});
      TensorImage image = new TensorImage();
      image.load(buffer);
      // Reload data but with an invalid shape, which leads to `buffer` corrupted.
      int[] newData = new int[] {1, 2, 3};
      buffer.loadArray(newData, new int[] {1, 1, 1, 3});

      assertThrows(IllegalArgumentException.class, image::getHeight);
    }

    @Test
    public void getColorSpaceTypeSucceedsWithBitmapARGB_8888() {
      Bitmap rgbBitmap = createRgbBitmap();
      TensorImage tensorImage = TensorImage.fromBitmap(rgbBitmap);

      assertThat(tensorImage.getColorSpaceType()).isEqualTo(ColorSpaceType.RGB);
    }

    @Test
    public void getColorSpaceTypeSucceedsWithRgbTensorBuffer() {
      TensorBuffer rgbBuffer = createRgbTensorBuffer(UINT8, false);
      TensorImage tensorImage = new TensorImage();
      tensorImage.load(rgbBuffer);

      assertThat(tensorImage.getColorSpaceType()).isEqualTo(ColorSpaceType.RGB);
    }

    @Test
    public void getColorSpaceTypeSucceedsWithGrayscaleTensorBuffer() {
      TensorBuffer grayBuffer = createGrayscaleTensorBuffer(UINT8, false);
      TensorImage tensorImage = new TensorImage();
      tensorImage.load(grayBuffer, ColorSpaceType.GRAYSCALE);

      assertThat(tensorImage.getColorSpaceType()).isEqualTo(ColorSpaceType.GRAYSCALE);
    }

    @Test
    public void getColorSpaceTypeSucceedsWithRepeatedLoading() {
      TensorBuffer grayBuffer = createGrayscaleTensorBuffer(UINT8, false);
      TensorBuffer rgbBuffer = createRgbTensorBuffer(UINT8, false);
      Bitmap rgbBitmap = createRgbBitmap();
      TensorImage tensorImage = new TensorImage();

      tensorImage.load(rgbBuffer);
      assertThat(tensorImage.getColorSpaceType()).isEqualTo(ColorSpaceType.RGB);
      tensorImage.load(grayBuffer, ColorSpaceType.GRAYSCALE);
      assertThat(tensorImage.getColorSpaceType()).isEqualTo(ColorSpaceType.GRAYSCALE);
      tensorImage.load(rgbBitmap);
      assertThat(tensorImage.getColorSpaceType()).isEqualTo(ColorSpaceType.RGB);
    }

    @Test
    public void getColorSpaceTypeFailsWhenNoImageHasBeenLoaded() {
      TensorImage tensorImage = new TensorImage();

      IllegalStateException exception =
          assertThrows(IllegalStateException.class, tensorImage::getColorSpaceType);
      assertThat(exception).hasMessageThat().contains("No image has been loaded yet.");
    }

    /**
     * Creates an example bit map, which is a 10x10 ARGB bitmap and pixels are set by: pixel[i] =
     * {A: 0, B: i + 2, G: i + 1, G: i}, where i is the flatten index
     */
    private static Bitmap createExampleBitmap() {
      int[] colors = new int[EXAMPLE_NUM_PIXELS];
      for (int i = 0; i < EXAMPLE_NUM_PIXELS; i++) {
        colors[i] = Color.rgb(i, i + 1, i + 2);
      }

      return Bitmap.createBitmap(colors, EXAMPLE_WIDTH, EXAMPLE_HEIGHT, Bitmap.Config.ARGB_8888);
    }

    private static float[] createExampleFloatPixels() {
      float[] pixels = new float[EXAMPLE_NUM_PIXELS * EXAMPLE_NUM_CHANNELS];
      for (int i = 0, j = 0; i < EXAMPLE_NUM_PIXELS; i++) {
        pixels[j++] = (i - MEAN) / STDDEV;
        pixels[j++] = (i + 1 - MEAN) / STDDEV;
        pixels[j++] = (i + 2 - MEAN) / STDDEV;
      }
      return pixels;
    }

    private static int[] createExampleUint8Pixels() {
      int[] pixels = new int[EXAMPLE_NUM_PIXELS * EXAMPLE_NUM_CHANNELS];
      for (int i = 0, j = 0; i < EXAMPLE_NUM_PIXELS; i++) {
        pixels[j++] = i;
        pixels[j++] = i + 1;
        pixels[j++] = i + 2;
      }
      return pixels;
    }
  }

  /** Parameterized tests for loading TensorBuffers with RGB and Grayscale images. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class LoadTensorBufferWithRgbAndGrayscale extends TensorImageTest {

    /**
     * Difference between the pair of float and uint8 values. It is used to test the data
     * conversion.
     */
    private static final float DELTA = 0.1f;

    /** The data type that used to create the TensorBuffer. */
    @Parameter(0)
    public DataType tensorBufferDataType;

    /** Indicates whether the shape is in the normalized form of (1, h, w, 3). */
    @Parameter(1)
    public boolean isNormalized;

    /** The color space type of the TensorBuffer. */
    @Parameter(2)
    public ColorSpaceType colorSpaceType;

    /** The data type that used to create the TensorImage. */
    @Parameter(3)
    public DataType tensorImageDataType;

    @Parameters(
        name =
            "tensorBufferDataType={0}; isNormalized={1}; colorSpaceType={2};"
                + " tensorImageDataType={3}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {FLOAT32, true, ColorSpaceType.RGB, FLOAT32},
            {FLOAT32, false, ColorSpaceType.RGB, UINT8},
            {UINT8, true, ColorSpaceType.RGB, FLOAT32},
            {UINT8, false, ColorSpaceType.RGB, UINT8},
          });
    }

    @Test
    public void loadAndGetBitmapSucceedsWithTensorBufferAndColorSpaceType() {
      TensorBuffer tensorBuffer =
          createTensorBuffer(tensorBufferDataType, isNormalized, colorSpaceType, DELTA);
      TensorImage tensorImage = new TensorImage(tensorImageDataType);

      tensorImage.load(tensorBuffer, colorSpaceType);
      Bitmap bitmap = tensorImage.getBitmap();

      Bitmap expectedBitmap = createBitmap(colorSpaceType);
      assertThat(bitmap.sameAs(expectedBitmap)).isTrue();
    }

    @Test
    public void loadAndGetTensorBufferSucceedsWithTensorBufferAndColorSpaceType() {
      TensorBuffer tensorBuffer =
          createTensorBuffer(tensorBufferDataType, isNormalized, colorSpaceType, DELTA);
      TensorImage tensorImage = new TensorImage(tensorImageDataType);

      tensorImage.load(tensorBuffer, colorSpaceType);
      TensorBuffer buffer = tensorImage.getTensorBuffer();

      // If tensorBufferDataType is UINT8, expectedTensorBuffer should not contain delta.
      float expectedResidual = tensorBufferDataType == UINT8 ? 0.f : DELTA;
      TensorBuffer expectedTensorBuffer =
          createTensorBuffer(tensorImageDataType, isNormalized, colorSpaceType, expectedResidual);
      assertEqualTensorBuffers(buffer, expectedTensorBuffer);
    }

    private static TensorBuffer createTensorBuffer(
        DataType dataType, boolean isNormalized, ColorSpaceType colorSpaceType, float delta) {
      switch (colorSpaceType) {
        case RGB:
          return createRgbTensorBuffer(dataType, isNormalized, delta);
        case GRAYSCALE:
          return createGrayscaleTensorBuffer(dataType, isNormalized, delta);
        default:
          break;
      }
      throw new IllegalArgumentException(
          "The ColorSpaceType, " + colorSpaceType + ", is unsupported.");
    }

    private static Bitmap createBitmap(ColorSpaceType colorSpaceType) {
      switch (colorSpaceType) {
        case RGB:
          return createRgbBitmap();
        case GRAYSCALE:
          return createGrayscaleBitmap();
        default:
          break;
      }
      throw new IllegalArgumentException(
          "The ColorSpaceType, " + colorSpaceType + ", is unsupported.");
    }
  }

  /** Parameterized tests for loading TensorBuffers with YUV images. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class LoadTensorBufferWithYUV extends TensorImageTest {

    private static final int HEIGHT = 2;
    private static final int WIDTH = 3;

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
          });
    }

    @Test
    public void loadTensorBufferWithColorSpaceShouldFail() {
      TensorImage tensorImage = new TensorImage();

      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () -> tensorImage.load(TensorBuffer.createDynamic(DataType.FLOAT32), colorSpaceType));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "Only ColorSpaceType.RGB and ColorSpaceType.GRAYSCALE are supported. Use"
                  + " `load(TensorBuffer, ImageProperties)` for other color space types.");
    }

    @Test
    public void loadTensorBufferAndGetBitmapShouldFail() {
      int[] data = new int[colorSpaceType.getNumElements(HEIGHT, WIDTH)];
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
      tensorBuffer.loadArray(data, new int[] {data.length});

      ImageProperties imageProperties =
          ImageProperties.builder()
              .setHeight(HEIGHT)
              .setWidth(WIDTH)
              .setColorSpaceType(colorSpaceType)
              .build();

      TensorImage tensorImage = new TensorImage(DataType.FLOAT32);
      tensorImage.load(tensorBuffer, imageProperties);

      UnsupportedOperationException exception =
          assertThrows(UnsupportedOperationException.class, () -> tensorImage.getBitmap());
      assertThat(exception)
          .hasMessageThat()
          .contains(
              "convertTensorBufferToBitmap() is unsupported for the color space type "
                  + colorSpaceType.name());
    }
  }

  /** Parameterized tests for loading TensorBuffers with ImageProperties. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class LoadTensorBufferWithImageProperties extends TensorImageTest {

    private static final int HEIGHT = 2;
    private static final int WIDTH = 3;
    private static final int WRONG_WIDTH = 10;

    @Parameter(0)
    public ColorSpaceType colorSpaceType;

    @Parameters(name = "colorSpaceType={0}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {ColorSpaceType.RGB},
            {ColorSpaceType.GRAYSCALE},
            {ColorSpaceType.NV12},
            {ColorSpaceType.NV21},
            {ColorSpaceType.YV12},
            {ColorSpaceType.YV21},
          });
    }

    @Test
    public void loadAndGetTensorBufferShouldSucceedWithCorrectProperties() {
      int[] data = new int[colorSpaceType.getNumElements(HEIGHT, WIDTH)];
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
      tensorBuffer.loadArray(data, new int[] {data.length});

      ImageProperties imageProperties =
          ImageProperties.builder()
              .setHeight(HEIGHT)
              .setWidth(WIDTH)
              .setColorSpaceType(colorSpaceType)
              .build();

      TensorImage tensorImage = new TensorImage(DataType.FLOAT32);
      tensorImage.load(tensorBuffer, imageProperties);

      assertEqualTensorBuffers(tensorImage.getTensorBuffer(), tensorBuffer);
    }

    @Test
    public void loadAndGetTensorBufferShouldSucceedWithLargerBuffer() {
      // Should allow buffer to be greater than the size specified by height and width.
      int moreElements = 1;
      int[] data = new int[colorSpaceType.getNumElements(HEIGHT, WIDTH) + moreElements];
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
      tensorBuffer.loadArray(data, new int[] {data.length});

      ImageProperties imageProperties =
          ImageProperties.builder()
              .setHeight(HEIGHT)
              .setWidth(WIDTH)
              .setColorSpaceType(colorSpaceType)
              .build();

      TensorImage tensorImage = new TensorImage(DataType.FLOAT32);
      tensorImage.load(tensorBuffer, imageProperties);

      assertEqualTensorBuffers(tensorImage.getTensorBuffer(), tensorBuffer);
    }

    @Test
    public void loadAndGetByteBufferShouldSucceedWithCorrectProperties() {
      ByteBuffer byteBuffer = ByteBuffer.allocate(colorSpaceType.getNumElements(HEIGHT, WIDTH));

      ImageProperties imageProperties =
          ImageProperties.builder()
              .setHeight(HEIGHT)
              .setWidth(WIDTH)
              .setColorSpaceType(colorSpaceType)
              .build();

      TensorImage tensorImage = new TensorImage(DataType.UINT8);
      tensorImage.load(byteBuffer, imageProperties);

      assertEqualByteBuffers(tensorImage.getBuffer(), byteBuffer);
    }

    @Test
    public void loadTensorBufferWithShouldFailWithWrongImageShape() {
      int[] data = new int[colorSpaceType.getNumElements(HEIGHT, WIDTH)];
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
      tensorBuffer.loadArray(data, new int[] {data.length});

      ImageProperties imageProperties =
          ImageProperties.builder()
              .setHeight(HEIGHT)
              .setWidth(WRONG_WIDTH)
              .setColorSpaceType(colorSpaceType)
              .build();

      TensorImage tensorImage = new TensorImage(DataType.FLOAT32);

      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () -> tensorImage.load(tensorBuffer, imageProperties));
      assertThat(exception)
          .hasMessageThat()
          .contains(
              String.format(
                  "The given number of elements (%d) does not match the image (%s) in %d x %d. The"
                      + " expected number of elements should be at least %d.",
                  data.length,
                  colorSpaceType.name(),
                  HEIGHT,
                  WRONG_WIDTH,
                  colorSpaceType.getNumElements(HEIGHT, WRONG_WIDTH)));
    }

    @Test
    public void getShapeOfInternalTensorBufferShouldSuccess() {
      int[] data = new int[colorSpaceType.getNumElements(HEIGHT, WIDTH)];
      TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.FLOAT32);
      tensorBuffer.loadArray(data, new int[] {data.length});

      ImageProperties imageProperties =
          ImageProperties.builder()
              .setHeight(HEIGHT)
              .setWidth(WIDTH)
              .setColorSpaceType(colorSpaceType)
              .build();

      TensorImage tensorImage = new TensorImage(DataType.FLOAT32);
      tensorImage.load(tensorBuffer, imageProperties);

      assertThat(tensorImage.getWidth()).isEqualTo(WIDTH);
      assertThat(tensorImage.getHeight()).isEqualTo(HEIGHT);
    }
  }

  /** Parameterized tests for loading TensorBuffer with invalid shapes. */
  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class LoadTensorBufferWithInvalidShapeTest extends TensorImageTest {

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
            {ColorSpaceType.RGB, new int[] {10, 20, 3, 4}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {10, 20, 5}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.RGB, new int[] {10, 20}, RGB_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {2, 10, 20}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {1, 10, 20, 3}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {10, 20, 4}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
            {ColorSpaceType.GRAYSCALE, new int[] {10}, GRAYSCALE_ASSERT_SHAPE_MESSAGE},
          });
    }

    @Test
    public void loadTensorBufferWithInvalidShape() {
      TensorBuffer tensorBuffer = TensorBuffer.createFixedSize(invalidShape, UINT8);
      TensorImage tensorImage = new TensorImage();

      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class, () -> tensorImage.load(tensorBuffer, colorSpaceType));
      assertThat(exception).hasMessageThat().contains(errorMessage + Arrays.toString(invalidShape));
    }
  }

  private static void assertEqualTensorBuffers(
      TensorBuffer tensorBuffer1, TensorBuffer tensorBuffer2) {
    assertEqualByteBuffers(tensorBuffer1.getBuffer(), tensorBuffer2.getBuffer());
    assertArrayEquals(tensorBuffer1.getShape(), tensorBuffer2.getShape());
  }

  private static void assertEqualByteBuffers(ByteBuffer buffer1, ByteBuffer buffer2) {
    buffer1.rewind();
    buffer2.rewind();
    assertThat(buffer1.equals(buffer2)).isTrue();
  }

  private static void setUpImageMock(Image imageMock, int imageFormat) {
    when(imageMock.getFormat()).thenReturn(imageFormat);
  }
}
