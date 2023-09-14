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

package org.tensorflow.lite.task.vision.segmenter;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.os.ParcelFileDescriptor;
import com.google.android.odml.image.MlImage;
import com.google.auto.value.AutoValue;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import org.tensorflow.lite.support.image.MlImageAdapter;
import org.tensorflow.lite.support.image.TensorImage;
import org.tensorflow.lite.task.core.BaseOptions;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.core.vision.ImageProcessingOptions;
import org.tensorflow.lite.task.vision.core.BaseVisionTaskApi;
import org.tensorflow.lite.task.vision.core.BaseVisionTaskApi.InferenceProvider;

/**
 * Performs segmentation on images.
 *
 * <p>The API expects a TFLite model with <a
 * href="https://www.tensorflow.org/lite/convert/metadata">TFLite Model Metadata.</a>.
 *
 * <p>The API supports models with one image input tensor and one output tensor. To be more
 * specific, here are the requirements.
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
 *   <li>Output image tensor ({@code kTfLiteUInt8}/{@code kTfLiteFloat32})
 *       <ul>
 *         <li>tensor of size {@code [batch x mask_height x mask_width x num_classes]}, where {@code
 *             batch} is required to be 1, {@code mask_width} and {@code mask_height} are the
 *             dimensions of the segmentation masks produced by the model, and {@code num_classes}
 *             is the number of classes supported by the model.
 *         <li>optional (but recommended) label map(s) can be attached as AssociatedFile-s with type
 *             TENSOR_AXIS_LABELS, containing one label per line. The first such AssociatedFile (if
 *             any) is used to fill the class name, i.e. {@link ColoredLabel#getlabel} of the
 *             results. The display name, i.e. {@link ColoredLabel#getDisplayName}, is filled from
 *             the AssociatedFile (if any) whose locale matches the `display_names_locale` field of
 *             the `ImageSegmenterOptions` used at creation time ("en" by default, i.e. English). If
 *             none of these are available, only the `index` field of the results will be filled.
 *       </ul>
 * </ul>
 *
 * <p>An example of such model can be found on <a
 * href="https://tfhub.dev/tensorflow/lite-model/deeplabv3/1/metadata/1">TensorFlow Hub.</a>.
 */
public final class ImageSegmenter extends BaseVisionTaskApi {

  private static final String IMAGE_SEGMENTER_NATIVE_LIB = "task_vision_jni";
  private static final int OPTIONAL_FD_LENGTH = -1;
  private static final int OPTIONAL_FD_OFFSET = -1;

  private final OutputType outputType;

  /**
   * Creates an {@link ImageSegmenter} instance from the default {@link ImageSegmenterOptions}.
   *
   * @param modelPath path of the segmentation model with metadata in the assets
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageSegmenter createFromFile(Context context, String modelPath)
      throws IOException {
    return createFromFileAndOptions(context, modelPath, ImageSegmenterOptions.builder().build());
  }

  /**
   * Creates an {@link ImageSegmenter} instance from the default {@link ImageSegmenterOptions}.
   *
   * @param modelFile the segmentation model {@link File} instance
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageSegmenter createFromFile(File modelFile) throws IOException {
    return createFromFileAndOptions(modelFile, ImageSegmenterOptions.builder().build());
  }

  /**
   * Creates an {@link ImageSegmenter} instance with a model buffer and the default {@link
   * ImageSegmenterOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the
   *     segmentation model
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   */
  public static ImageSegmenter createFromBuffer(final ByteBuffer modelBuffer) {
    return createFromBufferAndOptions(modelBuffer, ImageSegmenterOptions.builder().build());
  }

  /**
   * Creates an {@link ImageSegmenter} instance from {@link ImageSegmenterOptions}.
   *
   * @param modelPath path of the segmentation model with metadata in the assets
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageSegmenter createFromFileAndOptions(
      Context context, String modelPath, final ImageSegmenterOptions options) throws IOException {
    try (AssetFileDescriptor assetFileDescriptor = context.getAssets().openFd(modelPath)) {
      return createFromModelFdAndOptions(
          /*fileDescriptor=*/ assetFileDescriptor.getParcelFileDescriptor().getFd(),
          /*fileDescriptorLength=*/ assetFileDescriptor.getLength(),
          /*fileDescriptorOffset=*/ assetFileDescriptor.getStartOffset(),
          options);
    }
  }

  /**
   * Creates an {@link ImageSegmenter} instance from {@link ImageSegmenterOptions}.
   *
   * @param modelFile the segmentation model {@link File} instance
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageSegmenter createFromFileAndOptions(
      File modelFile, final ImageSegmenterOptions options) throws IOException {
    try (ParcelFileDescriptor descriptor =
        ParcelFileDescriptor.open(modelFile, ParcelFileDescriptor.MODE_READ_ONLY)) {
      return createFromModelFdAndOptions(
          /*fileDescriptor=*/ descriptor.getFd(),
          /*fileDescriptorLength=*/ OPTIONAL_FD_LENGTH,
          /*fileDescriptorOffset=*/ OPTIONAL_FD_OFFSET,
          options);
    }
  }

  /**
   * Creates an {@link ImageSegmenter} instance with a model buffer and {@link
   * ImageSegmenterOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the
   *     segmentation model
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   */
  public static ImageSegmenter createFromBufferAndOptions(
      final ByteBuffer modelBuffer, final ImageSegmenterOptions options) {
    if (!(modelBuffer.isDirect() || modelBuffer instanceof MappedByteBuffer)) {
      throw new IllegalArgumentException(
          "The model buffer should be either a direct ByteBuffer or a MappedByteBuffer.");
    }
    return new ImageSegmenter(
        TaskJniUtils.createHandleFromLibrary(
            new EmptyHandleProvider() {
              @Override
              public long createHandle() {
                return initJniWithByteBuffer(
                    modelBuffer,
                    options.getDisplayNamesLocale(),
                    options.getOutputType().getValue(),
                    TaskJniUtils.createProtoBaseOptionsHandleWithLegacyNumThreads(
                        options.getBaseOptions(), options.getNumThreads()));
              }
            },
            IMAGE_SEGMENTER_NATIVE_LIB),
        options.getOutputType());
  }

  /**
   * Constructor to initialize the JNI with a pointer from C++.
   *
   * @param nativeHandle a pointer referencing memory allocated in C++
   */
  private ImageSegmenter(long nativeHandle, OutputType outputType) {
    super(nativeHandle);
    this.outputType = outputType;
  }

  /** Options for setting up an {@link ImageSegmenter}. */
  @AutoValue
  public abstract static class ImageSegmenterOptions {
    private static final String DEFAULT_DISPLAY_NAME_LOCALE = "en";
    private static final OutputType DEFAULT_OUTPUT_TYPE = OutputType.CATEGORY_MASK;
    private static final int NUM_THREADS = -1;

    public abstract BaseOptions getBaseOptions();

    public abstract String getDisplayNamesLocale();

    public abstract OutputType getOutputType();

    public abstract int getNumThreads();

    public static Builder builder() {
      return new AutoValue_ImageSegmenter_ImageSegmenterOptions.Builder()
          .setDisplayNamesLocale(DEFAULT_DISPLAY_NAME_LOCALE)
          .setOutputType(DEFAULT_OUTPUT_TYPE)
          .setNumThreads(NUM_THREADS)
          .setBaseOptions(BaseOptions.builder().build());
    }

    /** Builder for {@link ImageSegmenterOptions}. */
    @AutoValue.Builder
    public abstract static class Builder {

      /** Sets the general options to configure Task APIs, such as accelerators. */
      public abstract Builder setBaseOptions(BaseOptions baseOptions);

      /**
       * Sets the locale to use for display names specified through the TFLite Model Metadata, if
       * any.
       *
       * <p>Defaults to English({@code "en"}). See the <a
       * href="https://github.com/tensorflow/tflite-support/blob/3ce83f0cfe2c68fecf83e019f2acc354aaba471f/tensorflow_lite_support/metadata/metadata_schema.fbs#L147">TFLite
       * Metadata schema file.</a> for the accepted pattern of locale.
       */
      public abstract Builder setDisplayNamesLocale(String displayNamesLocale);

      public abstract Builder setOutputType(OutputType outputType);

      /**
       * Sets the number of threads to be used for TFLite ops that support multi-threading when
       * running inference with CPU. Defaults to -1.
       *
       * <p>numThreads should be greater than 0 or equal to -1. Setting numThreads to -1 has the
       * effect to let TFLite runtime set the value.
       *
       * @deprecated use {@link BaseOptions} to configure number of threads instead. This method
       *     will override the number of threads configured from {@link BaseOptions}.
       */
      @Deprecated
      public abstract Builder setNumThreads(int numThreads);

      public abstract ImageSegmenterOptions build();
    }
  }

  /**
   * Performs actual segmentation on the provided image.
   *
   * <p>{@link ImageSegmenter} supports the following {@link TensorImage} color space types:
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
   * @return results of performing image segmentation. Note that at the time, a single {@link
   *     Segmentation} element is expected to be returned. The result is stored in a {@link List}
   *     for later extension to e.g. instance segmentation models, which may return one segmentation
   *     per object.
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   * @throws IllegalArgumentException if the color space type of image is unsupported
   */
  public List<Segmentation> segment(TensorImage image) {
    return segment(image, ImageProcessingOptions.builder().build());
  }

  /**
   * Performs actual segmentation on the provided image with {@link ImageProcessingOptions}.
   *
   * <p>{@link ImageSegmenter} supports the following {@link TensorImage} color space types:
   *
   * <ul>
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#RGB}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#NV12}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#NV21}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#YV12}
   *   <li>{@link org.tensorflow.lite.support.image.ColorSpaceType#YV21}
   * </ul>
   *
   * <p>{@link ImageSegmenter} supports the following options:
   *
   * <ul>
   *   <li>image rotation (through {@link ImageProcessingOptions.Builder#setOrientation}). It
   *       defaults to {@link ImageProcessingOptions.Orientation#TOP_LEFT}
   * </ul>
   *
   * @param image a UINT8 {@link TensorImage} object that represents an RGB or YUV image
   * @param options the options configure how to preprocess the image
   * @return results of performing image segmentation. Note that at the time, a single {@link
   *     Segmentation} element is expected to be returned. The result is stored in a {@link List}
   *     for later extension to e.g. instance segmentation models, which may return one segmentation
   *     per object.
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   * @throws IllegalArgumentException if the color space type of image is unsupported
   */
  public List<Segmentation> segment(TensorImage image, ImageProcessingOptions options) {
    return run(
        new InferenceProvider<List<Segmentation>>() {
          @Override
          public List<Segmentation> run(
              long frameBufferHandle, int width, int height, ImageProcessingOptions options) {
            return segment(frameBufferHandle, options);
          }
        },
        image,
        options);
  }

  /**
   * Performs actual segmentation on the provided {@code MlImage}.
   *
   * @param image an {@code MlImage} to segment.
   * @return results of performing image segmentation. Note that at the time, a single {@link
   *     Segmentation} element is expected to be returned. The result is stored in a {@link List}
   *     for later extension to e.g. instance segmentation models, which may return one segmentation
   *     per object.
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   * @throws IllegalArgumentException if the storage type or format of the image is unsupported
   */
  public List<Segmentation> segment(MlImage image) {
    return segment(image, ImageProcessingOptions.builder().build());
  }

  /**
   * Performs actual segmentation on the provided {@code MlImage} with {@link
   * ImageProcessingOptions}.
   *
   * <p>{@link ImageSegmenter} supports the following options:
   *
   * <ul>
   *   <li>image rotation (through {@link ImageProcessingOptions.Builder#setOrientation}). It
   *       defaults to {@link ImageProcessingOptions.Orientation#TOP_LEFT}. {@link
   *       MlImage#getRotation()} is not effective.
   * </ul>
   *
   * @param image an {@code MlImage} to segment.
   * @param options the options configure how to preprocess the image.
   * @return results of performing image segmentation. Note that at the time, a single {@link
   *     Segmentation} element is expected to be returned. The result is stored in a {@link List}
   *     for later extension to e.g. instance segmentation models, which may return one segmentation
   *     per object.
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   * @throws IllegalArgumentException if the color space type of image is unsupported
   */
  public List<Segmentation> segment(MlImage image, ImageProcessingOptions options) {
    image.getInternal().acquire();
    TensorImage tensorImage = MlImageAdapter.createTensorImageFrom(image);
    List<Segmentation> result = segment(tensorImage, options);
    image.close();
    return result;
  }

  public List<Segmentation> segment(long frameBufferHandle, ImageProcessingOptions options) {
    checkNotClosed();

    List<byte[]> maskByteArrays = new ArrayList<>();
    List<ColoredLabel> coloredLabels = new ArrayList<>();
    int[] maskShape = new int[2];
    segmentNative(getNativeHandle(), frameBufferHandle, maskByteArrays, maskShape, coloredLabels);

    List<ByteBuffer> maskByteBuffers = new ArrayList<>();
    for (byte[] bytes : maskByteArrays) {
      ByteBuffer byteBuffer = ByteBuffer.wrap(bytes);
      // Change the byte order to little_endian, since the buffers were generated in jni.
      byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
      maskByteBuffers.add(byteBuffer);
    }

    return Arrays.asList(
        Segmentation.create(
            outputType,
            outputType.createMasksFromBuffer(maskByteBuffers, maskShape),
            coloredLabels));
  }

  private static ImageSegmenter createFromModelFdAndOptions(
      final int fileDescriptor,
      final long fileDescriptorLength,
      final long fileDescriptorOffset,
      final ImageSegmenterOptions options) {
    long nativeHandle =
        TaskJniUtils.createHandleFromLibrary(
            new EmptyHandleProvider() {
              @Override
              public long createHandle() {
                return initJniWithModelFdAndOptions(
                    fileDescriptor,
                    fileDescriptorLength,
                    fileDescriptorOffset,
                    options.getDisplayNamesLocale(),
                    options.getOutputType().getValue(),
                    TaskJniUtils.createProtoBaseOptionsHandleWithLegacyNumThreads(
                        options.getBaseOptions(), options.getNumThreads()));
              }
            },
            IMAGE_SEGMENTER_NATIVE_LIB);
    return new ImageSegmenter(nativeHandle, options.getOutputType());
  }

  private static native long initJniWithModelFdAndOptions(
      int fileDescriptor,
      long fileDescriptorLength,
      long fileDescriptorOffset,
      String displayNamesLocale,
      int outputType,
      long baseOptionsHandle);

  private static native long initJniWithByteBuffer(
      ByteBuffer modelBuffer, String displayNamesLocale, int outputType, long baseOptionsHandle);

  /**
   * The native method to segment the image.
   *
   * <p>{@code maskBuffers}, {@code maskShape}, {@code coloredLabels} will be updated in the native
   * layer.
   */
  private static native void segmentNative(
      long nativeHandle,
      long frameBufferHandle,
      List<byte[]> maskByteArrays,
      int[] maskShape,
      List<ColoredLabel> coloredLabels);

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
