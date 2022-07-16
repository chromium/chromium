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

import org.tensorflow.lite.DataType;
import org.tensorflow.lite.annotations.UsedByReflection;
import org.tensorflow.lite.support.image.TensorImage;
import org.tensorflow.lite.task.core.BaseTaskApi;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.FdAndOptionsHandleProvider;
import org.tensorflow.lite.task.core.vision.ImageProcessingOptions;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

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
 *       </ul>
 * </ul>
 *
 * <p>An example of such model can be found on <a
 * href="https://tfhub.dev/bohemian-visual-recognition-alliance/lite-model/models/mushroom-identification_v1/1">TensorFlow
 * Hub.</a>.
 */
public final class ImageClassifier extends BaseTaskApi {
    private static final String IMAGE_CLASSIFIER_NATIVE_LIB = "task_vision_jni";

    /**
     * Creates an {@link ImageClassifier} instance from the default {@link ImageClassifierOptions}.
     *
     * @param modelPath path of the classification model with metadata in the assets
     * @throws IOException if an I/O error occurs when loading the tflite model
     * @throws AssertionError if error occurs when creating {@link ImageClassifier} from the native
     *     code
     */
    public static ImageClassifier createFromFile(Context context, String modelPath)
            throws IOException {
        return createFromFileAndOptions(
                context, modelPath, ImageClassifierOptions.builder().build());
    }

    /**
     * Creates an {@link ImageClassifier} instance from {@link ImageClassifierOptions}.
     *
     * @param modelPath path of the classification model with metadata in the assets
     * @throws IOException if an I/O error occurs when loading the tflite model
     * @throws AssertionError if error occurs when creating {@link ImageClassifier} from the native
     *     code
     */
    public static ImageClassifier createFromFileAndOptions(
            Context context, String modelPath, ImageClassifierOptions options) throws IOException {
        return new ImageClassifier(TaskJniUtils.createHandleFromFdAndOptions(
                context, new FdAndOptionsHandleProvider<ImageClassifierOptions>() {
                    @Override
                    public long createHandle(int fileDescriptor, long fileDescriptorLength,
                            long fileDescriptorOffset, ImageClassifierOptions options) {
                        return initJniWithModelFdAndOptions(fileDescriptor, fileDescriptorLength,
                                fileDescriptorOffset, options);
                    }
                }, IMAGE_CLASSIFIER_NATIVE_LIB, modelPath, options));
    }

    /**
     * Constructor to initialize the JNI with a pointer from C++.
     *
     * @param nativeHandle a pointer referencing memory allocated in C++
     */
    private ImageClassifier(long nativeHandle) {
        super(nativeHandle);
    }

    /** Options for setting up an ImageClassifier. */
    @UsedByReflection("image_classifier_jni.cc")
    public static class ImageClassifierOptions {
        // Not using AutoValue for this class because scoreThreshold cannot have default value
        // (otherwise, the default value would override the one in the model metadata) and
        // `Optional` is not an option here, because
        // 1. java.util.Optional require Java 8 while we need to support Java 7.
        // 2. The Guava library (com.google.common.base.Optional) is avoided in this project. See
        // the comments for labelAllowList.
        private final String displayNamesLocale;
        private final int maxResults;
        private final float scoreThreshold;
        private final boolean isScoreThresholdSet;
        // As an open source project, we've been trying avoiding depending on common java libraries,
        // such as Guava, because it may introduce conflicts with clients who also happen to use
        // those libraries. Therefore, instead of using ImmutableList here, we convert the List into
        // unmodifiableList in setLabelAllowList() and setLabelDenyList() to make it less
        // vulnerable.
        private final List<String> labelAllowList;
        private final List<String> labelDenyList;

        public static Builder builder() {
            return new Builder();
        }

        /** A builder that helps to configure an instance of ImageClassifierOptions. */
        public static class Builder {
            private String displayNamesLocale = "en";
            private int maxResults = -1;
            private float scoreThreshold;
            private boolean isScoreThresholdSet = false;
            private List<String> labelAllowList = new ArrayList<>();
            private List<String> labelDenyList = new ArrayList<>();

            private Builder() {}

            /**
             * Sets the locale to use for display names specified through the TFLite Model Metadata,
             * if any.
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
             * Sets the score threshold in [0,1).
             *
             * <p>It overrides the one provided in the model metadata (if any). Results below this
             * value are rejected.
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
             * <p>If non-empty, classifications whose label is in this set will be filtered out.
             * Duplicate or unknown labels are ignored. Mutually exclusive with labelAllowList.
             */
            public Builder setLabelDenyList(List<String> labelDenyList) {
                this.labelDenyList = Collections.unmodifiableList(new ArrayList<>(labelDenyList));
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

        private ImageClassifierOptions(Builder builder) {
            displayNamesLocale = builder.displayNamesLocale;
            maxResults = builder.maxResults;
            scoreThreshold = builder.scoreThreshold;
            isScoreThresholdSet = builder.isScoreThresholdSet;
            labelAllowList = builder.labelAllowList;
            labelDenyList = builder.labelDenyList;
        }
    }

    /**
     * Performs actual classification on the provided image.
     *
     * @param image a {@link TensorImage} object that represents an RGB image
     * @throws AssertionError if error occurs when classifying the image from the native code
     */
    public List<Classifications> classify(TensorImage image) {
        return classify(image, ImageProcessingOptions.builder().build());
    }

    /**
     * Performs actual classification on the provided image with {@link ImageProcessingOptions}.
     *
     * <p>{@link ImageClassifier} supports the following options:
     *
     * <ul>
     *   <li>Region of interest (ROI) (through {@link ImageProcessingOptions#Builder#setRoi}). It
     *       defaults to the entire image.
     *   <li>image rotation (through {@link ImageProcessingOptions#Builder#setOrientation}). It
     *       defaults to {@link ImageProcessingOptions#Orientation#TOP_LEFT}.
     * </ul>
     *
     * @param image a {@link TensorImage} object that represents an RGB image
     * @throws AssertionError if error occurs when classifying the image from the native code
     */
    public List<Classifications> classify(TensorImage image, ImageProcessingOptions options) {
        checkNotClosed();

        // image_classifier_jni.cc expects an uint8 image. Convert image of other types into uint8.
        TensorImage imageUint8 = image.getDataType() == DataType.UINT8
                ? image
                : TensorImage.createFrom(image, DataType.UINT8);

        Rect roi = options.getRoi().isEmpty()
                ? new Rect(0, 0, imageUint8.getWidth(), imageUint8.getHeight())
                : options.getRoi();

        return classifyNative(getNativeHandle(), imageUint8.getBuffer(), imageUint8.getWidth(),
                imageUint8.getHeight(), new int[] {roi.left, roi.top, roi.width(), roi.height()},
                options.getOrientation().getValue());
    }

    private static native long initJniWithModelFdAndOptions(int fileDescriptor,
            long fileDescriptorLength, long fileDescriptorOffset, ImageClassifierOptions options);

    /**
     * The native method to classify an image with the ROI and orientation.
     *
     * @param roi the ROI of the input image, an array representing the bounding box as {left, top,
     *     width, height}
     * @param orientation the integer value corresponding to {@link
     *     ImageProcessingOptions#Orientation}
     */
    private static native List<Classifications> classifyNative(
            long nativeHandle, ByteBuffer image, int width, int height, int[] roi, int orientation);

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
