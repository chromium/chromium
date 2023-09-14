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

package org.tensorflow.lite.task.vision.core;

import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkArgument;
import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkNotNull;

import android.graphics.ImageFormat;
import android.media.Image;
import android.media.Image.Plane;
import com.google.auto.value.AutoValue;
import java.nio.ByteBuffer;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.image.ColorSpaceType;
import org.tensorflow.lite.support.image.TensorImage;
import org.tensorflow.lite.task.core.BaseTaskApi;
import org.tensorflow.lite.task.core.vision.ImageProcessingOptions;

/** Base class for Task Vision APIs. */
public abstract class BaseVisionTaskApi extends BaseTaskApi {

  /** Syntax sugar to run vision tasks with FrameBuffer and image processing options. */
  public interface InferenceProvider<T> {
    T run(long frameBufferHandle, int width, int height, ImageProcessingOptions options);
  }

  protected BaseVisionTaskApi(long nativeHandle) {
    super(nativeHandle);
  }

  /** Runs inference with {@link TensorImage} and {@link ImageProcessingOptions}. */
  protected <T> T run(
      InferenceProvider<T> provider, TensorImage image, ImageProcessingOptions options) {
    FrameBufferData frameBufferData = createFrameBuffer(image, options.getOrientation().getValue());
    T results =
        provider.run(
            frameBufferData.getFrameBufferHandle(), image.getWidth(), image.getHeight(), options);
    deleteFrameBuffer(
        frameBufferData.getFrameBufferHandle(),
        frameBufferData.getByteArrayHandle(),
        frameBufferData.getByteArray());
    return results;
  }

  private static FrameBufferData createFrameBuffer(TensorImage image, int orientation) {
    ColorSpaceType colorSpaceType = image.getColorSpaceType();
    switch (colorSpaceType) {
      case RGB:
      case NV12:
      case NV21:
      case YV12:
      case YV21:
        // All these types can be converted to ByteBuffer inside TensorImage. Creating FrameBuffer
        // base on the image ByteBuffer.
        return createFrameBufferFromByteBuffer(image, orientation);
      case YUV_420_888:
        // YUV_420_888 is a specific type for android.media.Image.
        return createFrameBufferFromMediaImage(image, orientation);
      default:
        throw new IllegalArgumentException(
            "Color space type, " + colorSpaceType.name() + ", is unsupported.");
    }
  }

  /**
   * Creates FrameBuffer from the {@link android.media.Image} stored in the given {@link
   * TensorImage}.
   */
  private static FrameBufferData createFrameBufferFromMediaImage(
      TensorImage image, int orientation) {
    Image mediaImage = image.getMediaImage();

    checkArgument(
        mediaImage.getFormat() == ImageFormat.YUV_420_888,
        "Only supports loading YUV_420_888 Image.");

    Plane[] planes = mediaImage.getPlanes();
    checkArgument(
        planes.length == 3,
        String.format("The input image should have 3 planes, but got %d plane(s).", planes.length));

    // Verify and rewind planes.
    for (Plane plane : planes) {
      ByteBuffer buffer = plane.getBuffer();
      checkNotNull(buffer, "The image buffer is corrupted and the plane is null.");
      // From the public documentation, plane.getBuffer() should always return a direct ByteBuffer.
      // See https://developer.android.com/reference/android/media/Image.Plane#getBuffer()
      checkArgument(
          buffer.isDirect(),
          "The image plane buffer is not a direct ByteBuffer, and is not supported.");
      buffer.rewind();
    }

    return FrameBufferData.create(
        createFrameBufferFromPlanes(
            planes[0].getBuffer(),
            planes[1].getBuffer(),
            planes[2].getBuffer(),
            mediaImage.getWidth(),
            mediaImage.getHeight(),
            planes[0].getRowStride(),
            // row_stride and pixel_stride should be identical for U/V planes.
            planes[1].getRowStride(),
            planes[1].getPixelStride(),
            orientation),
        // FrameBuffer created with direct ByteBuffer does not require memory freeing.
        /*byteArrayHandle=*/ 0,
        /*byteArray=*/ new byte[0]);
  }

  /** Creates FrameBuffer from the {@link ByteBuffer} stored in the given {@link TensorImage}. */
  private static FrameBufferData createFrameBufferFromByteBuffer(
      TensorImage image, int orientation) {
    // base_vision_api_jni.cc expects an uint8 image. Convert image of other types into uint8.
    TensorImage imageUint8 =
        image.getDataType() == DataType.UINT8
            ? image
            : TensorImage.createFrom(image, DataType.UINT8);

    ByteBuffer byteBuffer = imageUint8.getBuffer();
    byteBuffer.rewind();
    ColorSpaceType colorSpaceType = image.getColorSpaceType();
    if (byteBuffer.isDirect()) {
      return FrameBufferData.create(
          createFrameBufferFromByteBuffer(
              byteBuffer,
              imageUint8.getWidth(),
              imageUint8.getHeight(),
              orientation,
              colorSpaceType.getValue()),
          // FrameBuffer created with direct ByteBuffer does not require memory freeing.
          /*byteArrayHandle=*/ 0,
          /*byteArray=*/ new byte[0]);
    } else {
      // If the byte array is copied in jni (during GetByteArrayElements), need to free
      // the copied array once inference is done.
      long[] byteArrayHandle = new long[1];
      byte[] byteArray = getBytesFromByteBuffer(byteBuffer);
      return FrameBufferData.create(
          createFrameBufferFromBytes(
              byteArray,
              imageUint8.getWidth(),
              imageUint8.getHeight(),
              orientation,
              colorSpaceType.getValue(),
              byteArrayHandle),
          byteArrayHandle[0],
          byteArray);
    }
  }

  /** Holds the FrameBuffer and the underlying data pointers in C++. */
  @AutoValue
  abstract static class FrameBufferData {

    /**
     * Initializes a {@link FrameBufferData} object.
     *
     * @param frameBufferHandle the native handle to the FrameBuffer object.
     * @param byteArrayHandle the native handle to the data array that backs up the FrameBuffer
     *     object. If the FrameBuffer is created on a byte array, this byte array need to be freed
     *     after inference is done. If the FrameBuffer is created on a direct ByteBuffer, no byte
     *     array needs to be freed, and byteArrayHandle will be 0.
     * @param byteArray the byte array that is used to create the c++ byte array object, which is
     *     needed when releasing byteArrayHandle. If the FrameBuffer is created on a direct
     *     ByteBuffer (no byte array needs to be freed), pass in an empty array for {@code
     *     byteArray}.
     */
    public static FrameBufferData create(
        long frameBufferHandle, long byteArrayHandle, byte[] byteArray) {
      return new AutoValue_BaseVisionTaskApi_FrameBufferData(
          frameBufferHandle, byteArrayHandle, byteArray);
    }

    abstract long getFrameBufferHandle();

    abstract long getByteArrayHandle();

    // Package private method for transferring data.
    @SuppressWarnings("mutable")
    abstract byte[] getByteArray();
  }

  private static native long createFrameBufferFromByteBuffer(
      ByteBuffer image, int width, int height, int orientation, int colorSpaceType);

  private static native long createFrameBufferFromBytes(
      byte[] image,
      int width,
      int height,
      int orientation,
      int colorSpaceType,
      long[] byteArrayHandle);

  private static native long createFrameBufferFromPlanes(
      ByteBuffer yBuffer,
      ByteBuffer uBuffer,
      ByteBuffer vBuffer,
      int width,
      int height,
      int yRowStride,
      int uvRowStride,
      int uvPixelStride,
      int orientation);

  private static native void deleteFrameBuffer(
      long frameBufferHandle, long byteArrayHandle, byte[] byteArray);

  private static byte[] getBytesFromByteBuffer(ByteBuffer byteBuffer) {
    // If the ByteBuffer has a back up array, use it directly without copy.
    if (byteBuffer.hasArray() && byteBuffer.arrayOffset() == 0) {
      return byteBuffer.array();
    }
    // Copy out the data otherwise.
    byteBuffer.rewind();
    byte[] bytes = new byte[byteBuffer.limit()];
    byteBuffer.get(bytes, 0, bytes.length);
    return bytes;
  }
}
