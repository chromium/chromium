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

import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkArgument;

import android.graphics.Bitmap;
import android.media.Image;
import java.nio.ByteBuffer;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;

/**
 * TensorImage is the wrapper class for Image object. When using image processing utils in
 * TFLite.support library, it's common to convert image objects in variant types to TensorImage at
 * first.
 *
 * <p>At present, only RGB images are supported, and the A channel is always ignored.
 *
 * <p>Details of data storage: a {@link TensorImage} object may have 2 potential sources of truth: a
 * {@link android.graphics.Bitmap} or a {@link TensorBuffer}. {@link TensorImage} maintains the
 * state and only converts one to the other when needed. A typical use case of {@link TensorImage}
 * is to first load a {@link android.graphics.Bitmap} image, then process it using {@link
 * ImageProcessor}, and finally get the underlying {@link ByteBuffer} of the {@link TensorBuffer}
 * and feed it into the TFLite interpreter.
 *
 * <p>IMPORTANT: to achieve the best performance, {@link TensorImage} avoids copying data whenever
 * it's possible. Therefore, it doesn't own its data. Callers should not modify data objects those
 * are passed to {@link TensorImage#load(Bitmap)} or {@link TensorImage#load(TensorBuffer,
 * ColorSpaceType)}.
 *
 * <p>IMPORTANT: all methods are not proved thread-safe.
 *
 * @see ImageProcessor which is often used for transforming a {@code TensorImage}.
 */
// TODO(b/138907116): Support loading images from TensorBuffer with properties.
// TODO(b/138905544): Support directly loading RGBBytes, YUVBytes and other types if necessary.
public class TensorImage {

  private final DataType dataType;
  private ImageContainer container = null;

  /**
   * Initializes a {@link TensorImage} object.
   *
   * <p>Note: the data type of this {@link TensorImage} is {@link DataType#UINT8}. Use {@link
   * #TensorImage(DataType)} if other data types are preferred.
   */
  public TensorImage() {
    this(DataType.UINT8);
  }

  /**
   * Initializes a {@link TensorImage} object with the specified data type.
   *
   * <p>When getting a {@link TensorBuffer} or a {@link ByteBuffer} from this {@link TensorImage},
   * such as using {@link #getTensorBuffer} and {@link #getBuffer}, the data values will be
   * converted to the specified data type.
   *
   * <p>Note: the shape of a {@link TensorImage} is not fixed. It can be adjusted to the shape of
   * the image being loaded to this {@link TensorImage}.
   *
   * @param dataType the expected data type of the resulting {@link TensorBuffer}. The type is
   *     always fixed during the lifetime of the {@link TensorImage}. To convert the data type, use
   *     {@link #createFrom(TensorImage, DataType)} to create a copy and convert data type at the
   *     same time.
   * @throws IllegalArgumentException if {@code dataType} is neither {@link DataType#UINT8} nor
   *     {@link DataType#FLOAT32}
   */
  public TensorImage(DataType dataType) {
    checkArgument(
        dataType == DataType.UINT8 || dataType == DataType.FLOAT32,
        "Illegal data type for TensorImage: Only FLOAT32 and UINT8 are accepted");
    this.dataType = dataType;
  }

  /**
   * Initializes a {@link TensorImage} object of {@link DataType#UINT8} with a {@link
   * android.graphics.Bitmap} .
   *
   * @see #load(Bitmap) for reusing the object when it's expensive to create objects frequently,
   *     because every call of {@code fromBitmap} creates a new {@link TensorImage}.
   */
  public static TensorImage fromBitmap(Bitmap bitmap) {
    TensorImage image = new TensorImage();
    image.load(bitmap);
    return image;
  }

  /**
   * Creates a deep-copy of a given {@link TensorImage} with the desired data type.
   *
   * @param src the {@link TensorImage} to copy from
   * @param dataType the expected data type of newly created {@link TensorImage}
   * @return a {@link TensorImage} whose data is copied from {@code src} and data type is {@code
   *     dataType}
   */
  public static TensorImage createFrom(TensorImage src, DataType dataType) {
    TensorImage dst = new TensorImage(dataType);
    dst.container = src.container.clone();
    return dst;
  }

  /**
   * Loads a {@link android.graphics.Bitmap} image object into this {@link TensorImage}.
   *
   * <p>Note: if the {@link TensorImage} has data type other than {@link DataType#UINT8}, numeric
   * casting and clamping will be applied when calling {@link #getTensorBuffer} and {@link
   * #getBuffer}, where the {@link android.graphics.Bitmap} will be converted into a {@link
   * TensorBuffer}.
   *
   * <p>Important: when loading a bitmap, DO NOT MODIFY the bitmap from the caller side anymore. The
   * {@link TensorImage} object will rely on the bitmap. It will probably modify the bitmap as well.
   * In this method, we perform a zero-copy approach for that bitmap, by simply holding its
   * reference. Use {@code bitmap.copy(bitmap.getConfig(), true)} to create a copy if necessary.
   *
   * <p>Note: to get the best performance, please load images in the same shape to avoid memory
   * re-allocation.
   *
   * @throws IllegalArgumentException if {@code bitmap} is not in ARGB_8888
   */
  public void load(Bitmap bitmap) {
    container = BitmapContainer.create(bitmap);
  }

  /**
   * Loads a float array as RGB pixels into this {@link TensorImage}, representing the pixels
   * inside.
   *
   * <p>Note: if the {@link TensorImage} has a data type other than {@link DataType#FLOAT32},
   * numeric casting and clamping will be applied when calling {@link #getTensorBuffer} and {@link
   * #getBuffer}.
   *
   * @param pixels the RGB pixels representing the image
   * @param shape the shape of the image, should either in form (h, w, 3), or in form (1, h, w, 3)
   * @throws IllegalArgumentException if the shape is neither (h, w, 3) nor (1, h, w, 3)
   */
  public void load(float[] pixels, int[] shape) {
    TensorBuffer buffer = TensorBuffer.createDynamic(getDataType());
    buffer.loadArray(pixels, shape);
    load(buffer);
  }

  /**
   * Loads an int array as RGB pixels into this {@link TensorImage}, representing the pixels inside.
   *
   * <p>Note: numeric casting and clamping will be applied to convert the values into the data type
   * of this {@link TensorImage} when calling {@link #getTensorBuffer} and {@link #getBuffer}.
   *
   * @param pixels the RGB pixels representing the image
   * @param shape the shape of the image, should either in form (h, w, 3), or in form (1, h, w, 3)
   * @throws IllegalArgumentException if the shape is neither (h, w, 3) nor (1, h, w, 3)
   */
  public void load(int[] pixels, int[] shape) {
    TensorBuffer buffer = TensorBuffer.createDynamic(getDataType());
    buffer.loadArray(pixels, shape);
    load(buffer);
  }

  /**
   * Loads a {@link TensorBuffer} containing pixel values. The color layout should be RGB.
   *
   * <p>Note: if the data type of {@code buffer} does not match that of this {@link TensorImage},
   * numeric casting and clamping will be applied when calling {@link #getTensorBuffer} and {@link
   * #getBuffer}.
   *
   * @param buffer the {@link TensorBuffer} to be loaded. Its shape should be either (h, w, 3) or
   *     (1, h, w, 3)
   * @throws IllegalArgumentException if the shape is neither (h, w, 3) nor (1, h, w, 3)
   */
  public void load(TensorBuffer buffer) {
    load(buffer, ColorSpaceType.RGB);
  }

  /**
   * Loads a {@link TensorBuffer} containing pixel values with the specific {@link ColorSpaceType}.
   *
   * <p>Only supports {@link ColorSpaceType#RGB} and {@link ColorSpaceType#GRAYSCALE}. Use {@link
   * #load(TensorBuffer, ImageProperties)} for other color space types.
   *
   * <p>Note: if the data type of {@code buffer} does not match that of this {@link TensorImage},
   * numeric casting and clamping will be applied when calling {@link #getTensorBuffer} and {@link
   * #getBuffer}.
   *
   * @param buffer the {@link TensorBuffer} to be loaded. Its shape should be either (h, w, 3) or
   *     (1, h, w, 3) for RGB images, and either (h, w) or (1, h, w) for GRAYSCALE images
   * @throws IllegalArgumentException if the shape of buffer does not match the color space type, or
   *     if the color space type is not supported
   */
  public void load(TensorBuffer buffer, ColorSpaceType colorSpaceType) {
    checkArgument(
        colorSpaceType == ColorSpaceType.RGB || colorSpaceType == ColorSpaceType.GRAYSCALE,
        "Only ColorSpaceType.RGB and ColorSpaceType.GRAYSCALE are supported. Use"
            + " `load(TensorBuffer, ImageProperties)` for other color space types.");

    container = TensorBufferContainer.create(buffer, colorSpaceType);
  }

  /**
   * Loads a {@link TensorBuffer} containing pixel values with the specific {@link ImageProperties}.
   *
   * <p>The shape of the {@link TensorBuffer} will not be used to determine image height and width.
   * Set image properties through {@link ImageProperties}.
   *
   * <p>Note: if the data type of {@code buffer} does not match that of this {@link TensorImage},
   * numeric casting and clamping will be applied when calling {@link #getTensorBuffer} and {@link
   * #getBuffer}.
   *
   * @throws IllegalArgumentException if buffer size is less than the image size indicated by image
   *     height, width, and color space type in {@link ImageProperties}
   */
  public void load(TensorBuffer buffer, ImageProperties imageProperties) {
    container = TensorBufferContainer.create(buffer, imageProperties);
  }

  /**
   * Loads a {@link ByteBuffer} containing pixel values with the specific {@link ImageProperties}.
   *
   * <p>Note: if the data type of {@code buffer} does not match that of this {@link TensorImage},
   * numeric casting and clamping will be applied when calling {@link #getTensorBuffer} and {@link
   * #getBuffer}.
   *
   * @throws IllegalArgumentException if buffer size is less than the image size indicated by image
   *     height, width, and color space type in {@link ImageProperties}
   */
  public void load(ByteBuffer buffer, ImageProperties imageProperties) {
    TensorBuffer tensorBuffer = TensorBuffer.createDynamic(DataType.UINT8);
    tensorBuffer.loadBuffer(buffer, new int[] {buffer.limit()});
    container = TensorBufferContainer.create(tensorBuffer, imageProperties);
  }

  /**
   * Loads an {@link android.media.Image} object into this {@link TensorImage}.
   *
   * <p>The main usage of this method is to load an {@link android.media.Image} object as model
   * input to the <a href="TFLite Task
   * Library">https://www.tensorflow.org/lite/inference_with_metadata/task_library/overview</a>.
   * {@link TensorImage} backed by {@link android.media.Image} is not supported by {@link
   * ImageProcessor}.
   *
   * <p>* @throws IllegalArgumentException if the {@link android.graphics.ImageFormat} of {@code
   * image} is not YUV_420_888
   */
  public void load(Image image) {
    container = MediaImageContainer.create(image);
  }

  /**
   * Returns a {@link android.graphics.Bitmap} representation of this {@link TensorImage}.
   *
   * <p>Numeric casting and clamping will be applied if the stored data is not uint8.
   *
   * <p>Note that, the reliable way to get pixels from an {@code ALPHA_8} Bitmap is to use {@code
   * copyPixelsToBuffer}. Bitmap methods such as, `setPixels()` and `getPixels` do not work.
   *
   * <p>Important: it's only a reference. DO NOT MODIFY. We don't create a copy here for performance
   * concern, but if modification is necessary, please make a copy.
   *
   * @return a reference to a {@link android.graphics.Bitmap} in {@code ARGB_8888} config ("A"
   *     channel is always opaque) or in {@code ALPHA_8}, depending on the {@link ColorSpaceType} of
   *     this {@link TensorBuffer}.
   * @throws IllegalStateException if the {@link TensorImage} never loads data
   */
  public Bitmap getBitmap() {
    if (container == null) {
      throw new IllegalStateException("No image has been loaded yet.");
    }

    return container.getBitmap();
  }

  /**
   * Returns a {@link ByteBuffer} representation of this {@link TensorImage} with the expected data
   * type.
   *
   * <p>Numeric casting and clamping will be applied if the stored data is different from the data
   * type of the {@link TensorImage}.
   *
   * <p>Important: it's only a reference. DO NOT MODIFY. We don't create a copy here for performance
   * concern, but if modification is necessary, please make a copy.
   *
   * <p>It's essentially a short cut for {@code getTensorBuffer().getBuffer()}.
   *
   * @return a reference to a {@link ByteBuffer} which holds the image data
   * @throws IllegalStateException if the {@link TensorImage} never loads data
   */
  public ByteBuffer getBuffer() {
    return getTensorBuffer().getBuffer();
  }

  /**
   * Returns a {@link TensorBuffer} representation of this {@link TensorImage} with the expected
   * data type.
   *
   * <p>Numeric casting and clamping will be applied if the stored data is different from the data
   * type of the {@link TensorImage}.
   *
   * <p>Important: it's only a reference. DO NOT MODIFY. We don't create a copy here for performance
   * concern, but if modification is necessary, please make a copy.
   *
   * @return a reference to a {@link TensorBuffer} which holds the image data
   * @throws IllegalStateException if the {@link TensorImage} never loads data
   */
  public TensorBuffer getTensorBuffer() {
    if (container == null) {
      throw new IllegalStateException("No image has been loaded yet.");
    }

    return container.getTensorBuffer(dataType);
  }

  /**
   * Returns an {@link android.media.Image} representation of this {@link TensorImage}.
   *
   * <p>This method only works when the {@link TensorImage} is backed by an {@link
   * android.media.Image}, meaning you need to first load an {@link android.media.Image} through
   * {@link #load(Image)}.
   *
   * <p>Important: it's only a reference. DO NOT MODIFY. We don't create a copy here for performance
   * concern, but if modification is necessary, please make a copy.
   *
   * @return a reference to a {@link android.graphics.Bitmap} in {@code ARGB_8888} config ("A"
   *     channel is always opaque) or in {@code ALPHA_8}, depending on the {@link ColorSpaceType} of
   *     this {@link TensorBuffer}.
   * @throws IllegalStateException if the {@link TensorImage} never loads data
   */
  public Image getMediaImage() {
    if (container == null) {
      throw new IllegalStateException("No image has been loaded yet.");
    }

    return container.getMediaImage();
  }

  /**
   * Gets the data type of this {@link TensorImage}.
   *
   * @return a data type. Currently only {@link DataType#UINT8} and {@link DataType#FLOAT32} are
   *     supported.
   */
  public DataType getDataType() {
    return dataType;
  }

  /**
   * Gets the color space type of this {@link TensorImage}.
   *
   * @throws IllegalStateException if the {@link TensorImage} never loads data
   */
  public ColorSpaceType getColorSpaceType() {
    if (container == null) {
      throw new IllegalStateException("No image has been loaded yet.");
    }

    return container.getColorSpaceType();
  }

  /**
   * Gets the image width.
   *
   * @throws IllegalStateException if the {@link TensorImage} never loads data
   * @throws IllegalArgumentException if the underlying data is corrupted
   */
  public int getWidth() {
    if (container == null) {
      throw new IllegalStateException("No image has been loaded yet.");
    }

    return container.getWidth();
  }

  /**
   * Gets the image height.
   *
   * @throws IllegalStateException if the {@link TensorImage} never loads data
   * @throws IllegalArgumentException if the underlying data is corrupted
   */
  public int getHeight() {
    if (container == null) {
      throw new IllegalStateException("No image has been loaded yet.");
    }

    return container.getHeight();
  }
}
