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

package org.tensorflow.lite.task.vision.classifier;

import android.content.Context;
import android.graphics.Rect;
import android.os.ParcelFileDescriptor;
import com.google.android.odml.image.MlImage;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.tensorflow.lite.support.image.MlImageAdapter;
import org.tensorflow.lite.support.image.TensorImage;
import org.tensorflow.lite.task.core.BaseOptions;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.core.TaskJniUtils.FdAndOptionsHandleProvider;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;
import org.tensorflow.lite.task.core.vision.ImageProcessingOptions;
import org.tensorflow.lite.task.vision.core.BaseVisionTaskApi;
import org.tensorflow.lite.task.vision.core.BaseVisionTaskApi.InferenceProvider;

/**
 * Performs classification on images.
 *
 * <p>The API expects a TFLite model with optional, but strongly recommended, <a
 * href="https://www.tensorflow.org/lite/convert/metadata">TFLite Model Metadata.</a>.
 *
 * <p>The API supports models with one image input tensor and one classification output tensor. To
 * be more specific, here are the requirements.
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
 *   <li>Output score tensor ({@code kTfLiteUInt8}/{@code kTfLiteFloat32})
 *       <ul>
 *         <li>with {@code N} classes of either 2 or 4 dimensions, such as {@code [1 x N]} or {@code
 *             [1 x 1 x 1 x N]}
 *         <li>the label file is required to be packed to the metadata. See the <a
 *             href="https://www.tensorflow.org/lite/convert/metadata#label_output">example of
 *             creating metadata for an image classifier</a>. If no label files are packed, it will
 *             use index as label in the result.
 *       </ul>
 * </ul>
 *
 * <p>An example of such model can be found on <a
 * href="https://tfhub.dev/bohemian-visual-recognition-alliance/lite-model/models/mushroom-identification_v1/1">TensorFlow
 * Hub.</a>.
 */
public final class ImageClassifier extends BaseVisionTaskApi {

  private static final String IMAGE_CLASSIFIER_NATIVE_LIB = "task_vision_jni";
  private static final int OPTIONAL_FD_LENGTH = -1;
  private static final int OPTIONAL_FD_OFFSET = -1;

  /**
   * Creates an {@link ImageClassifier} instance from the default {@link ImageClassifierOptions}.
   *
   * @param modelPath path of the classification model with metadata in the assets
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageClassifier createFromFile(Context context, String modelPath)
      throws IOException {
    return createFromFileAndOptions(context, modelPath, ImageClassifierOptions.builder().build());
  }

  /**
   * Creates an {@link ImageClassifier} instance from the default {@link ImageClassifierOptions}.
   *
   * @param modelFile the classification model {@link File} instance
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageClassifier createFromFile(File modelFile) throws IOException {
    return createFromFileAndOptions(modelFile, ImageClassifierOptions.builder().build());
  }

  /**
   * Creates an {@link ImageClassifier} instance with a model buffer and the default {@link
   * ImageClassifierOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the
   *     classification model
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageClassifier createFromBuffer(final ByteBuffer modelBuffer) {
    return createFromBufferAndOptions(modelBuffer, ImageClassifierOptions.builder().build());
  }

  /**
   * Creates an {@link ImageClassifier} instance from {@link ImageClassifierOptions}.
   *
   * @param modelPath path of the classification model with metadata in the assets
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageClassifier createFromFileAndOptions(
      Context context, String modelPath, ImageClassifierOptions options) throws IOException {
    return new ImageClassifier(
        TaskJniUtils.createHandleFromFdAndOptions(
            context,
            new FdAndOptionsHandleProvider<ImageClassifierOptions>() {
              @Override
              public long createHandle(
                  int fileDescriptor,
                  long fileDescriptorLength,
                  long fileDescriptorOffset,
                  ImageClassifierOptions options) {
                return initJniWithModelFdAndOptions(
                    fileDescriptor,
                    fileDescriptorLength,
                    fileDescriptorOffset,
                    options,
                    TaskJniUtils.createProtoBaseOptionsHandleWithLegacyNumThreads(
                        options.getBaseOptions(), options.getNumThreads()));
              }
            },
            IMAGE_CLASSIFIER_NATIVE_LIB,
            modelPath,
            options));
  }

  /**
   * Creates an {@link ImageClassifier} instance.
   *
   * @param modelFile the classification model {@link File} instance
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageClassifier createFromFileAndOptions(
      File modelFile, final ImageClassifierOptions options) throws IOException {
    try (ParcelFileDescriptor descriptor =
        ParcelFileDescriptor.open(modelFile, ParcelFileDescriptor.MODE_READ_ONLY)) {
      return new ImageClassifier(
          TaskJniUtils.createHandleFromLibrary(
              new TaskJniUtils.EmptyHandleProvider() {
                @Override
                public long createHandle() {
                  return initJniWithModelFdAndOptions(
                      descriptor.getFd(),
                      /*fileDescriptorLength=*/ OPTIONAL_FD_LENGTH,
                      /*fileDescriptorOffset=*/ OPTIONAL_FD_OFFSET,
                      options,
                      TaskJniUtils.createProtoBaseOptionsHandleWithLegacyNumThreads(
                          options.getBaseOptions(), options.getNumThreads()));
                }
              },
              IMAGE_CLASSIFIER_NATIVE_LIB));
    }
  }

  /**
   * Creates an {@link ImageClassifier} instance with a model buffer and {@link
   * ImageClassifierOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the
   *     classification model
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static ImageClassifier createFromBufferAndOptions(
      final ByteBuffer modelBuffer, final ImageClassifierOptions options) {
    if (!(modelBuffer.isDirect() || modelBuffer instanceof MappedByteBuffer)) {
      throw new IllegalArgumentException(
          "The model buffer should be either a direct ByteBuffer or a MappedByteBuffer.");
    }
    return new ImageClassifier(
        TaskJniUtils.createHandleFromLibrary(
            new EmptyHandleProvider() {
              @Override
              public long createHandle() {
                return initJniWithByteBuffer(
                    modelBuffer,
                    options,
                    TaskJniUtils.createProtoBaseOptionsHandleWithLegacyNumThreads(
                        options.getBaseOptions(), options.getNumThreads()));
              }
            },
            IMAGE_CLASSIFIER_NATIVE_LIB));
  }

  /**
   * Constructor to initialize the JNI with a pointer from C++.
   *
   * @param nativeHandle a pointer referencing memory allocated in C++
   */
  ImageClassifier(long nativeHandle) {
    super(nativeHandle);
  }

  /** Options for setting up an ImageClassifier. */
  @UsedByReflection("image_classifier_jni.cc")
  public static class ImageClassifierOptions {
    // Not using AutoValue for this class because scoreThreshold cannot have default value
    // (otherwise, the default value would override the one in the model metadata) and `Optional` is
    // not an option here, because
    // 1. java.util.Optional require Java 8 while we need to support Java 7.
    // 2. The Guava library (com.google.common.base.Optional) is avoided in this project. See the
    // comments for labelAllowList.
    private final BaseOptions baseOptions;
    private final String displayNamesLocale;
    private final int maxResults;
    private final float scoreThreshold;
    private final boolean isScoreThresholdSet;
    // As an open source project, we've been trying avoiding depending on common java libraries,
    // such as Guava, because it may introduce conflicts with clients who also happen to use those
    // libraries. Therefore, instead of using ImmutableList here, we convert the List into
    // unmodifiableList in setLabelAllowList() and setLabelDenyList() to make it less
    // vulnerable.
    private final List<String> labelAllowList;
    private final List<String> labelDenyList;
    private final int numThreads;

    public static Builder builder() {
      return new Builder();
    }

    /** A builder that helps to configure an instance of ImageClassifierOptions. */
    public static class Builder {
      private BaseOptions baseOptions = BaseOptions.builder().build();
      private String displayNamesLocale = "en";
      private int maxResults = -1;
      private float scoreThreshold;
      private boolean isScoreThresholdSet = false;
      private List<String> labelAllowList = new ArrayList<>();
      private List<String> labelDenyList = new ArrayList<>();
      private int numThreads = -1;

      Builder() {}

      /** Sets the general options to configure Task APIs, such as accelerators. */
      public Builder setBaseOptions(BaseOptions baseOptions) {
        this.baseOptions = baseOptions;
        return this;
      }

      /**
       * Sets the locale to use for display names specified through the TFLite Model Metadata, if
       * any.
       *
       * <p>Defaults to English({@code "en"}). See the <a
       * href="https://github.com/tensorflow/tflite-support/blob/3ce83f0cfe2c68fecf83e019f2acc354aaba471f/tensorflow_lite_support/metadata/metadata_schema.fbs#L147">TFLite
       * Metadata schema file.</a> for the accepted pattern of locale.
       */
      public Builder setDisplayNamesLocale(String displayNamesLocale) {
        this.displayNamesLocale = displayNamesLocale;
        return this;
      }

      /**
       * Sets the maximum number of top scored results to return.
       *
       * <p>If < 0, all results will be returned. If 0, an invalid argument error is returned.
       * Defaults to -1.
       *
       * @throws IllegalArgumentException if maxResults is 0.
       */
      public Builder setMaxResults(int maxResults) {
        if (maxResults == 0) {
          throw new IllegalArgumentException("maxResults cannot be 0.");
        }
        this.maxResults = maxResults;
        return this;
      }

      /**
       * Sets the score threshold.
       *
       * <p>It overrides the one provided in the model metadata (if any). Results below this value
       * are rejected.
       */
      public Builder setScoreThreshold(float scoreThreshold) {
        this.scoreThreshold = scoreThreshold;
        isScoreThresholdSet = true;
        return this;
      }

      /**
       * Sets the optional allowlist of labels.
       *
       * <p>If non-empty, classifications whose label is not in this set will be filtered out.
       * Duplicate or unknown labels are ignored. Mutually exclusive with labelDenyList.
       */
      public Builder setLabelAllowList(List<String> labelAllowList) {
        this.labelAllowList = Collections.unmodifiableList(new ArrayList<>(labelAllowList));
        return this;
      }

      /**
       * Sets the optional denylist of labels.
       *
       * <p>If non-empty, classifications whose label is in this set will be filtered out. Duplicate
       * or unknown labels are ignored. Mutually exclusive with labelAllowList.
       */
      public Builder setLabelDenyList(List<String> labelDenyList) {
        this.labelDenyList = Collections.unmodifiableList(new ArrayList<>(labelDenyList));
        return this;
      }

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
      public Builder setNumThreads(int numThreads) {
        this.numThreads = numThreads;
        return this;
      }

      public ImageClassifierOptions build() {
        return new ImageClassifierOptions(this);
      }
    }

    @UsedByReflection("image_classifier_jni.cc")
    public String getDisplayNamesLocale() {
      return displayNamesLocale;
    }

    @UsedByReflection("image_classifier_jni.cc")
    public int getMaxResults() {
      return maxResults;
    }

    @UsedByReflection("image_classifier_jni.cc")
    public float getScoreThreshold() {
      return scoreThreshold;
    }

    @UsedByReflection("image_classifier_jni.cc")
    public boolean getIsScoreThresholdSet() {
      return isScoreThresholdSet;
    }

    @UsedByReflection("image_classifier_jni.cc")
    public List<String> getLabelAllowList() {
      return new ArrayList<>(labelAllowList);
    }

    @UsedByReflection("image_classifier_jni.cc")
    public List<String> getLabelDenyList() {
      return new ArrayList<>(labelDenyList);
    }

    @UsedByReflection("image_classifier_jni.cc")
    public int getNumThreads() {
      return numThreads;
    }

    public BaseOptions getBaseOptions() {
      return baseOptions;
    }

    ImageClassifierOptions(Builder builder) {
      displayNamesLocale = builder.displayNamesLocale;
      maxResults = builder.maxResults;
      scoreThreshold = builder.scoreThreshold;
      isScoreThresholdSet = builder.isScoreThresholdSet;
      labelAllowList = builder.labelAllowList;
      labelDenyList = builder.labelDenyList;
      numThreads = builder.numThreads;
      baseOptions = builder.baseOptions;
    }
  }

  /**
   * Performs actual classification on the provided {@link TensorImage}.
   *
   * <p>{@link ImageClassifier} supports the following {@link TensorImage} color space types:
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
  public List<Classifications> classify(TensorImage image) {
    return classify(image, ImageProcessingOptions.builder().build());
  }

  /**
   * Performs actual classification on the provided {@link TensorImage} with {@link
   * ImageProcessingOptions}.
   *
   * <p>{@link ImageClassifier} supports the following options:
   *
   * <ul>
   *   <li>Region of interest (ROI) (through {@link ImageProcessingOptions.Builder#setRoi}). It
   *       defaults to the entire image.
   *   <li>image rotation (through {@link ImageProcessingOptions.Builder#setOrientation}). It
   *       defaults to {@link ImageProcessingOptions.Orientation#TOP_LEFT}.
   * </ul>
   *
   * <p>{@link ImageClassifier} supports the following {@link TensorImage} color space types:
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
  public List<Classifications> classify(TensorImage image, ImageProcessingOptions options) {
    return run(
        new InferenceProvider<List<Classifications>>() {
          @Override
          public List<Classifications> run(
              long frameBufferHandle, int width, int height, ImageProcessingOptions options) {
            return classify(frameBufferHandle, width, height, options);
          }
        },
        image,
        options);
  }

  /**
   * Performs actual classification on the provided {@code MlImage}.
   *
   * @param image an {@code MlImage} object that represents an image
   * @throws IllegalArgumentException if the storage type or format of the image is unsupported
   */
  public List<Classifications> classify(MlImage image) {
    return classify(image, ImageProcessingOptions.builder().build());
  }

  /**
   * Performs actual classification on the provided {@code MlImage} with {@link
   * ImageProcessingOptions}.
   *
   * <p>{@link ImageClassifier} supports the following options:
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
  public List<Classifications> classify(MlImage image, ImageProcessingOptions options) {
    image.getInternal().acquire();
    TensorImage tensorImage = MlImageAdapter.createTensorImageFrom(image);
    List<Classifications> result = classify(tensorImage, options);
    image.close();
    return result;
  }

  private List<Classifications> classify(
      long frameBufferHandle, int width, int height, ImageProcessingOptions options) {
    checkNotClosed();

    Rect roi = options.getRoi().isEmpty() ? new Rect(0, 0, width, height) : options.getRoi();

    return classifyNative(
        getNativeHandle(),
        frameBufferHandle,
        new int[] {roi.left, roi.top, roi.width(), roi.height()});
  }

  private static native long initJniWithModelFdAndOptions(
      int fileDescriptor,
      long fileDescriptorLength,
      long fileDescriptorOffset,
      ImageClassifierOptions options,
      long baseOptionsHandle);

  private static native long initJniWithByteBuffer(
      ByteBuffer modelBuffer, ImageClassifierOptions options, long baseOptionsHandle);

  /**
   * The native method to classify an image with the ROI and orientation.
   *
   * @param roi the ROI of the input image, an array representing the bounding box as {left, top,
   *     width, height}
   */
  private static native List<Classifications> classifyNative(
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
