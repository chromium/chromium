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

import com.google.android.odml.image.BitmapExtractor;
import com.google.android.odml.image.ByteBufferExtractor;
import com.google.android.odml.image.MediaImageExtractor;
import com.google.android.odml.image.MlImage;
import com.google.android.odml.image.MlImage.ImageFormat;
import com.google.auto.value.AutoValue;
import java.nio.ByteBuffer;

/** Converts {@code MlImage} to {@link TensorImage} and vice versa. */
public class MlImageAdapter {

  /** Proxies an {@link ImageFormat} and its equivalent {@link ColorSpaceType}. */
  @AutoValue
  abstract static class ImageFormatProxy {

    abstract ColorSpaceType getColorSpaceType();

    @ImageFormat
    abstract int getImageFormat();

    static ImageFormatProxy createFromImageFormat(@ImageFormat int format) {
      switch (format) {
        case MlImage.IMAGE_FORMAT_RGB:
          return new AutoValue_MlImageAdapter_ImageFormatProxy(ColorSpaceType.RGB, format);
        case MlImage.IMAGE_FORMAT_NV12:
          return new AutoValue_MlImageAdapter_ImageFormatProxy(ColorSpaceType.NV12, format);
        case MlImage.IMAGE_FORMAT_NV21:
          return new AutoValue_MlImageAdapter_ImageFormatProxy(ColorSpaceType.NV21, format);
        case MlImage.IMAGE_FORMAT_YV12:
          return new AutoValue_MlImageAdapter_ImageFormatProxy(ColorSpaceType.YV12, format);
        case MlImage.IMAGE_FORMAT_YV21:
          return new AutoValue_MlImageAdapter_ImageFormatProxy(ColorSpaceType.YV21, format);
        case MlImage.IMAGE_FORMAT_YUV_420_888:
          return new AutoValue_MlImageAdapter_ImageFormatProxy(ColorSpaceType.YUV_420_888, format);
        case MlImage.IMAGE_FORMAT_ALPHA:
          return new AutoValue_MlImageAdapter_ImageFormatProxy(ColorSpaceType.GRAYSCALE, format);
        case MlImage.IMAGE_FORMAT_RGBA:
        case MlImage.IMAGE_FORMAT_JPEG:
        case MlImage.IMAGE_FORMAT_UNKNOWN:
          throw new IllegalArgumentException(
              "Cannot create ColorSpaceType from MlImage format: " + format);
        default:
          throw new AssertionError("Illegal @ImageFormat: " + format);
      }
    }
  }

  /**
   * Creates a {@link TensorImage} from an {@link MlImage}.
   *
   * <p>IMPORTANT: The returned {@link TensorImage} shares storage with {@code mlImage}, so do not
   * modify the contained object in the {@link TensorImage}, as {@code MlImage} expects its
   * contained data are immutable. Also, callers should use {@code MlImage#getInternal()#acquire()}
   * and {@code MlImage#release()} to avoid the {@code mlImage} being released unexpectedly.
   *
   * @throws IllegalArgumentException if the {@code mlImage} is built from an unsupported container.
   */
  public static TensorImage createTensorImageFrom(MlImage mlImage) {
    // TODO(b/190670174): Choose the best storage from multiple containers.
    com.google.android.odml.image.ImageProperties mlImageProperties =
        mlImage.getContainedImageProperties().get(0);
    switch (mlImageProperties.getStorageType()) {
      case MlImage.STORAGE_TYPE_BITMAP:
        return TensorImage.fromBitmap(BitmapExtractor.extract(mlImage));
      case MlImage.STORAGE_TYPE_MEDIA_IMAGE:
        TensorImage mediaTensorImage = new TensorImage();
        mediaTensorImage.load(MediaImageExtractor.extract(mlImage));
        return mediaTensorImage;
      case MlImage.STORAGE_TYPE_BYTEBUFFER:
        ByteBuffer buffer = ByteBufferExtractor.extract(mlImage);
        ImageFormatProxy formatProxy =
            ImageFormatProxy.createFromImageFormat(mlImageProperties.getImageFormat());
        TensorImage byteBufferTensorImage = new TensorImage();
        ImageProperties properties =
            ImageProperties.builder()
                .setColorSpaceType(formatProxy.getColorSpaceType())
                .setHeight(mlImage.getHeight())
                .setWidth(mlImage.getWidth())
                .build();
        byteBufferTensorImage.load(buffer, properties);
        return byteBufferTensorImage;
      default:
        throw new IllegalArgumentException(
            "Illegal storage type: " + mlImageProperties.getStorageType());
    }
  }

  /** Creatas a {@link ColorSpaceType} from {@code MlImage.ImageFormat}. */
  public static ColorSpaceType createColorSpaceTypeFrom(@ImageFormat int imageFormat) {
    return ImageFormatProxy.createFromImageFormat(imageFormat).getColorSpaceType();
  }

  private MlImageAdapter() {}
}
