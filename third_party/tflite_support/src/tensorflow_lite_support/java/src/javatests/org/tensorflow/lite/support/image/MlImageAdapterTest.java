/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.media.Image;
import com.google.android.odml.image.BitmapMlImageBuilder;
import com.google.android.odml.image.ByteBufferMlImageBuilder;
import com.google.android.odml.image.MediaMlImageBuilder;
import com.google.android.odml.image.MlImage;
import com.google.android.odml.image.MlImage.ImageFormat;
import java.io.IOException;
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

/** Test for {@link MlImageAdapter}. */
@RunWith(Suite.class)
@SuiteClasses({
  MlImageAdapterTest.CreateTensorImageFromSupportedByteBufferMlImage.class,
  MlImageAdapterTest.CreateTensorImageFromUnsupportedByteBufferMlImage.class,
  MlImageAdapterTest.General.class,
})
public class MlImageAdapterTest {

  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class CreateTensorImageFromSupportedByteBufferMlImage
      extends MlImageAdapterTest {

    @Parameter(0)
    @ImageFormat
    public int imageFormat;

    @Parameter(1)
    public ColorSpaceType colorSpaceType;

    @Parameters(name = "imageFormat={0}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {MlImage.IMAGE_FORMAT_RGB, ColorSpaceType.RGB},
            {MlImage.IMAGE_FORMAT_ALPHA, ColorSpaceType.GRAYSCALE},
            {MlImage.IMAGE_FORMAT_NV21, ColorSpaceType.NV21},
            {MlImage.IMAGE_FORMAT_NV12, ColorSpaceType.NV12},
            {MlImage.IMAGE_FORMAT_YV12, ColorSpaceType.YV12},
            {MlImage.IMAGE_FORMAT_YV21, ColorSpaceType.YV21},
          });
    }

    @Test
    public void createTensorImageFrom_supportedByteBufferMlImage_succeeds() throws IOException {
      ByteBuffer buffer = ByteBuffer.allocateDirect(6).asReadOnlyBuffer();
      buffer.rewind();
      MlImage image = new ByteBufferMlImageBuilder(buffer, 1, 2, imageFormat).build();

      TensorImage tensorImage = MlImageAdapter.createTensorImageFrom(image);

      assertThat(tensorImage.getWidth()).isEqualTo(1);
      assertThat(tensorImage.getHeight()).isEqualTo(2);
      assertThat(tensorImage.getColorSpaceType()).isEqualTo(colorSpaceType);
      assertThat(tensorImage.getBuffer().position()).isEqualTo(0);
      assertThat(tensorImage.getBuffer()).isEquivalentAccordingToCompareTo(buffer);
    }
  }

  @RunWith(ParameterizedRobolectricTestRunner.class)
  public static final class CreateTensorImageFromUnsupportedByteBufferMlImage
      extends MlImageAdapterTest {
    @Parameter(0)
    @ImageFormat
    public int imageFormat;

    @Parameters(name = "imageFormat={0}")
    public static Collection<Object[]> data() {
      return Arrays.asList(
          new Object[][] {
            {MlImage.IMAGE_FORMAT_RGBA},
            {MlImage.IMAGE_FORMAT_JPEG},
            {MlImage.IMAGE_FORMAT_YUV_420_888},
            {MlImage.IMAGE_FORMAT_UNKNOWN},
          });
    }

    @Test
    public void createTensorImageFrom_unsupportedByteBufferMlImage_throws() throws IOException {
      ByteBuffer buffer = ByteBuffer.allocateDirect(6).asReadOnlyBuffer();
      buffer.rewind();
      MlImage image = new ByteBufferMlImageBuilder(buffer, 1, 2, imageFormat).build();

      assertThrows(
          IllegalArgumentException.class, () -> MlImageAdapter.createTensorImageFrom(image));
    }
  }

  @RunWith(RobolectricTestRunner.class)
  public static final class General extends MlImageAdapterTest {

    @Mock Image mediaImageMock;

    @Before
    public void setUp() {
      MockitoAnnotations.openMocks(this);
    }

    @Test
    public void createTensorImageFrom_bitmapMlImage_succeeds() throws IOException {
      Bitmap bitmap =
          Bitmap.createBitmap(new int[] {0xff000100, 0xff000001}, 1, 2, Bitmap.Config.ARGB_8888);
      MlImage image = new BitmapMlImageBuilder(bitmap).build();
      ByteBuffer expectedBuffer = ByteBuffer.allocateDirect(6);
      for (byte b : new byte[] {0, 1, 0, 0, 0, 1}) {
        expectedBuffer.put(b);
      }
      expectedBuffer.rewind();

      TensorImage tensorImage = MlImageAdapter.createTensorImageFrom(image);

      assertThat(tensorImage.getWidth()).isEqualTo(1);
      assertThat(tensorImage.getHeight()).isEqualTo(2);
      assertThat(tensorImage.getBuffer().position()).isEqualTo(0);
      assertThat(tensorImage.getBuffer()).isEquivalentAccordingToCompareTo(expectedBuffer);
    }

    @Test
    public void createTensorImageFrom_yuv420888MediaImageMlImage_succeeds() throws IOException {
      setUpMediaImageMock(mediaImageMock, android.graphics.ImageFormat.YUV_420_888, 1, 2);
      MlImage image = new MediaMlImageBuilder(mediaImageMock).build();

      TensorImage tensorImage = MlImageAdapter.createTensorImageFrom(image);

      assertThat(tensorImage.getWidth()).isEqualTo(1);
      assertThat(tensorImage.getHeight()).isEqualTo(2);
      assertThat(tensorImage.getColorSpaceType()).isEqualTo(ColorSpaceType.YUV_420_888);
    }

    @Test
    public void createTensorImageFrom_nonYuv420888MediaImageMlImage_throws() throws IOException {
      setUpMediaImageMock(mediaImageMock, android.graphics.ImageFormat.YUV_422_888, 1, 2);
      MlImage image = new MediaMlImageBuilder(mediaImageMock).build();

      assertThrows(
          IllegalArgumentException.class, () -> MlImageAdapter.createTensorImageFrom(image));
    }

    private static void setUpMediaImageMock(
        Image mediaImageMock, int imageFormat, int width, int height) {
      when(mediaImageMock.getFormat()).thenReturn(imageFormat);
      when(mediaImageMock.getWidth()).thenReturn(width);
      when(mediaImageMock.getHeight()).thenReturn(height);
    }
  }
}
