/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.task.vision.searcher;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.graphics.Rect;
import android.os.ParcelFileDescriptor;
import com.google.android.odml.image.MlImage;
import com.google.auto.value.AutoValue;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.util.List;
import org.tensorflow.lite.support.image.MlImageAdapter;
import org.tensorflow.lite.support.image.TensorImage;
import org.tensorflow.lite.task.core.BaseOptions;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.core.vision.ImageProcessingOptions;
import org.tensorflow.lite.task.processor.NearestNeighbor;
import org.tensorflow.lite.task.processor.SearcherOptions;
import org.tensorflow.lite.task.vision.core.BaseVisionTaskApi;
import org.tensorflow.lite.task.vision.core.BaseVisionTaskApi.InferenceProvider;

/**
 * Performs similarity search on images.
 *
 * <p>The API expects a TFLite model with optional, but strongly recommended, <a
 * href="https://www.tensorflow.org/lite/convert/metadata">TFLite Model Metadata.</a>.
 *
 * <ul>
 *   <li>Input image tensor ({@code kTfLiteUInt8}/{@code kTfLiteFloat32})
 *       <ul>
 *         <li>image input of size {@code [batch x height x width x channels]}.
 *         <li>batch inference is not supported ({@code batch} is required to be 1).
 *         <li>only RGB inputs are supported ({@code channels} is required to be 3).
 *         <li>if type is {@code kTfLiteFloat32}, NormalizationOptions are required to be attached
 *             to the metadata for input normalization.
 *       </ul>
 *   <li>Output tensor ({@code kTfLiteUInt8}/{@code kTfLiteFloat32})
 *       <ul>
 *         <li>{@code N} components corresponding to the {@code N} dimensions of the returned
 *             feature vector for this output layer.
 *         <li>Either 2 or 4 dimensions, i.e. {@code [1 x N]} or {@code [1 x 1 x 1 x N]}.
 *       </ul>
 * </ul>
 *
 * <p>TODO(b/180502532): add pointer to example model.
 *
 * <p>TODO(b/222671076): add factory create methods without options, such as `createFromFile`, once
 * the single file format (index file packed in the model) is supported.
 */
public final class ImageSearcher extends BaseVisionTaskApi {

  private static final String IMAGE_SEARCHER_NATIVE_LIB = "task_vision_jni";
  private static final int OPTIONAL_FD_LENGTH = -1;
  private static final int OPTIONAL_FD_OFFSET = -1;

  /**
   * Creates an {@link ImageSearcher} instance from {@link ImageSearcherOptions}.
   *
   * @param modelPath path of the search model with metadata in the assets
   * @throws IOException if an I/O error occurs when loading the tflite model or the index file
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageSearcher createFromFileAndOptions(
      Context context, String modelPath, final ImageSearcherOptions options) throws IOException {
    try (AssetFileDescriptor assetFileDescriptor = context.getAssets().openFd(modelPath)) {
      return createFromModelFdAndOptions(
          /*modelDescriptor=*/ assetFileDescriptor.getParcelFileDescriptor().getFd(),
          /*modelDescriptorLength=*/ assetFileDescriptor.getLength(),
          /*modelDescriptorOffset=*/ assetFileDescriptor.getStartOffset(),
          options);
    }
  }

  /**
   * Creates an {@link ImageSearcher} instance.
   *
   * @param modelFile the search model {@link File} instance
   * @throws IOException if an I/O error occurs when loading the tflite model or the index file
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageSearcher createFromFileAndOptions(
      File modelFile, final ImageSearcherOptions options) throws IOException {
    try (ParcelFileDescriptor descriptor =
        ParcelFileDescriptor.open(modelFile, ParcelFileDescriptor.MODE_READ_ONLY)) {
      return createFromModelFdAndOptions(
          /*modelDescriptor=*/ descriptor.getFd(),
          /*modelDescriptorLength=*/ OPTIONAL_FD_LENGTH,
          /*modelDescriptorOffset=*/ OPTIONAL_FD_OFFSET,
          options);
    }
  }

  /**
   * Creates an {@link ImageSearcher} instance with a model buffer and {@link ImageSearcherOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the search
   *     model
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   * @throws IOException if an I/O error occurs when loading the index file
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageSearcher createFromBufferAndOptions(
      final ByteBuffer modelBuffer, final ImageSearcherOptions options) throws IOException {
    if (!(modelBuffer.isDirect() || modelBuffer instanceof MappedByteBuffer)) {
      throw new IllegalArgumentException(
          "The model buffer should be either a direct ByteBuffer or a MappedByteBuffer.");
    }
    if (options.getSearcherOptions().getIndexFile() != null) {
      try (ParcelFileDescriptor indexDescriptor =
          ParcelFileDescriptor.open(
              options.getSearcherOptions().getIndexFile(), ParcelFileDescriptor.MODE_READ_ONLY)) {
        return createFromBufferAndOptionsImpl(modelBuffer, options, indexDescriptor.getFd());
      }
    } else {
      return createFromBufferAndOptionsImpl(modelBuffer, options, /*indexFd=*/ 0);
    }
  }

  public static ImageSearcher createFromBufferAndOptionsImpl(
      final ByteBuffer modelBuffer, final ImageSearcherOptions options, final int indexFd) {
    return new ImageSearcher(
        TaskJniUtils.createHandleFromLibrary(
            new EmptyHandleProvider() {
              @Override
              public long createHandle() {
                return initJniWithByteBuffer(
                    modelBuffer,
                    TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions()),
                    options.getSearcherOptions().getL2Normalize(),
                    options.getSearcherOptions().getQuantize(),
                    indexFd,
                    options.getSearcherOptions().getMaxResults());
              }
            },
            IMAGE_SEARCHER_NATIVE_LIB));
  }

  /**
   * Constructor to initialize the JNI with a pointer from C++.
   *
   * @param nativeHandle a pointer referencing memory allocated in C++
   */
  ImageSearcher(long nativeHandle) {
    super(nativeHandle);
  }

  /** Options for setting up an ImageSearcher. */
  @AutoValue
  public abstract static class ImageSearcherOptions {

    abstract BaseOptions getBaseOptions();

    abstract SearcherOptions getSearcherOptions();

    public static Builder builder() {
      return new AutoValue_ImageSearcher_ImageSearcherOptions.Builder()
          .setBaseOptions(BaseOptions.builder().build())
          .setSearcherOptions(SearcherOptions.builder().build());
    }

    /** Builder for {@link ImageSearcherOptions}. */
    @AutoValue.Builder
    public abstract static class Builder {
      /** Sets the general options to configure Task APIs, such as accelerators. */
      public abstract Builder setBaseOptions(BaseOptions baseOptions);

      /** Sets the options to configure Searcher API. */
      public abstract Builder setSearcherOptions(SearcherOptions searcherOptions);

      public abstract ImageSearcherOptions build();
    }
  }

  /**
   * Performs embedding extraction on the provided {@link TensorImage}, followed by nearest-neighbor
   * search in the index.
   *
   * <p>{@link ImageSearcher} supports the following {@link TensorImage} color space types:
   *
   * <ul>
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#RGB}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#NV12}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#NV21}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#YV12}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#YV21}
   * </ul>
   *
   * @param image a UINT8 {@link TensorImage} object that represents an RGB or YUV image
   * @throws IllegalArgumentException if the color space type of image is unsupported
   */
  public List<NearestNeighbor> search(TensorImage image) {
    return search(image, ImageProcessingOptions.builder().build());
  }

  /**
   * Performs embedding extraction on the provided {@link TensorImage} with {@link
   * ImageProcessingOptions}, followed by nearest-neighbor search in the index.
   *
   * <p>{@link ImageSearcher} supports the following options:
   *
   * <ul>
   *   <li>Region of interest (ROI) (through {@link ImageProcessingOptions.Builder#setRoi}). It
   *       defaults to the entire image.
   *   <li>image rotation (through {@link ImageProcessingOptions.Builder#setOrientation}). It
   *       defaults to {@link ImageProcessingOptions.Orientation#TOP_LEFT}.
   * </ul>
   *
   * <p>{@link ImageSearcher} supports the following {@link TensorImage} color space types:
   *
   * <ul>
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#RGB}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#NV12}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#NV21}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#YV12}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#YV21}
   * </ul>
   *
   * @param image a UINT8 {@link TensorImage} object that represents an RGB or YUV image
   * @throws IllegalArgumentException if the color space type of image is unsupported
   */
  public List<NearestNeighbor> search(TensorImage image, ImageProcessingOptions options) {
    return run(
        new InferenceProvider<List<NearestNeighbor>>() {
          @Override
          public List<NearestNeighbor> run(
              long frameBufferHandle, int width, int height, ImageProcessingOptions options) {
            return search(frameBufferHandle, width, height, options);
          }
        },
        image,
        options);
  }

  /**
   * Performs embedding extraction on the provided {@code MlImage}, followed by nearest-neighbor
   * search in the index.
   *
   * @param image an {@code MlImage} object that represents an image
   * @throws IllegalArgumentException if the storage type or format of the image is unsupported
   */
  public List<NearestNeighbor> search(MlImage image) {
    return search(image, ImageProcessingOptions.builder().build());
  }

  /**
   * Performs embedding extraction on the provided {@code MlImage} with {@link
   * ImageProcessingOptions}, followed by nearest-neighbor search in the index.
   *
   * <p>{@link ImageSearcher} supports the following options:
   *
   * <ul>
   *   <li>Region of interest (ROI) (through {@link ImageProcessingOptions.Builder#setRoi}). It
   *       defaults to the entire image.
   *   <li>image rotation (through {@link ImageProcessingOptions.Builder#setOrientation}). It
   *       defaults to {@link ImageProcessingOptions.Orientation#TOP_LEFT}. {@link
   *       MlImage#getRotation()} is not effective.
   * </ul>
   *
   * @param image a {@code MlImage} object that represents an image
   * @param options configures options including ROI and rotation
   * @throws IllegalArgumentException if the storage type or format of the image is unsupported
   */
  public List<NearestNeighbor> search(MlImage image, ImageProcessingOptions options) {
    image.getInternal().acquire();
    TensorImage tensorImage = MlImageAdapter.createTensorImageFrom(image);
    List<NearestNeighbor> result = search(tensorImage, options);
    image.close();
    return result;
  }

  private List<NearestNeighbor> search(
      long frameBufferHandle, int width, int height, ImageProcessingOptions options) {
    checkNotClosed();
    Rect roi = options.getRoi().isEmpty() ? new Rect(0, 0, width, height) : options.getRoi();
    return searchNative(
        getNativeHandle(),
        frameBufferHandle,
        new int[] {roi.left, roi.top, roi.width(), roi.height()});
  }

  private static ImageSearcher createFromModelFdAndOptions(
      final int modelDescriptor,
      final long modelDescriptorLength,
      final long modelDescriptorOffset,
      final ImageSearcherOptions options)
      throws IOException {
    if (options.getSearcherOptions().getIndexFile() != null) {
      // indexDescriptor must be alive before ImageSearcher is initialized completely in the native
      // layer.
      try (ParcelFileDescriptor indexDescriptor =
          ParcelFileDescriptor.open(
              options.getSearcherOptions().getIndexFile(), ParcelFileDescriptor.MODE_READ_ONLY)) {
        return createFromModelFdAndOptionsImpl(
            modelDescriptor,
            modelDescriptorLength,
            modelDescriptorOffset,
            options,
            indexDescriptor.getFd());
      }
    } else {
      // Index file is not configured. We'll check if the model contains one in the native layer.
      return createFromModelFdAndOptionsImpl(
          modelDescriptor, modelDescriptorLength, modelDescriptorOffset, options, /*indexFd=*/ 0);
    }
  }

  private static ImageSearcher createFromModelFdAndOptionsImpl(
      final int modelDescriptor,
      final long modelDescriptorLength,
      final long modelDescriptorOffset,
      final ImageSearcherOptions options,
      final int indexFd) {
    long nativeHandle =
        TaskJniUtils.createHandleFromLibrary(
            new EmptyHandleProvider() {
              @Override
              public long createHandle() {
                return initJniWithModelFdAndOptions(
                    modelDescriptor,
                    modelDescriptorLength,
                    modelDescriptorOffset,
                    TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions()),
                    options.getSearcherOptions().getL2Normalize(),
                    options.getSearcherOptions().getQuantize(),
                    indexFd,
                    options.getSearcherOptions().getMaxResults());
              }
            },
            IMAGE_SEARCHER_NATIVE_LIB);
    return new ImageSearcher(nativeHandle);
  }

  private static native long initJniWithModelFdAndOptions(
      int modelDescriptor,
      long modelDescriptorLength,
      long modelDescriptorOffset,
      long baseOptionsHandle,
      boolean l2Normalize,
      boolean quantize,
      int indexDescriptor,
      int maxResults);

  private static native long initJniWithByteBuffer(
      ByteBuffer modelBuffer,
      long baseOptionsHandle,
      boolean l2Normalize,
      boolean quantize,
      int indexFileDescriptor,
      int maxResults);

  /**
   * The native method to search an image based on the ROI specified.
   *
   * @param roi the ROI of the input image, an array representing the bounding box as {left, top,
   *     width, height}
   */
  private static native List<NearestNeighbor> searchNative(
      long nativeHandle, long frameBufferHandle, int[] roi);

  @Override
  protected void deinit(long nativeHandle) {
    deinitJni(nativeHandle);
  }

  /**
   * Native implementation to release memory pointed by the pointer.
   *
   * @param nativeHandle pointer to memory allocated
   */
  private native void deinitJni(long nativeHandle);
}
