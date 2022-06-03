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

import com.google.auto.value.AutoValue;

import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.image.TensorImage;
import org.tensorflow.lite.task.core.BaseTaskApi;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.core.vision.ImageProcessingOptions;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

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
 *             any) is used to fill the class name, i.e. {@link ColoredLabel#getClassName} of the
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
public final class ImageSegmenter extends BaseTaskApi {
    private static final String IMAGE_SEGMENTER_NATIVE_LIB = "task_vision_jni";

    private final OutputType outputType;

    /**
     * Creates an {@link ImageSegmenter} instance from the default {@link ImageSegmenterOptions}.
     *
     * @param modelPath path of the segmentation model with metadata in the assets
     * @throws IOException if an I/O error occurs when loading the tflite model
     * @throws AssertionError if error occurs when creating {@link ImageSegmenter} from the native
     *     code
     */
    public static ImageSegmenter createFromFile(Context context, String modelPath)
            throws IOException {
        return createFromFileAndOptions(
                context, modelPath, ImageSegmenterOptions.builder().build());
    }

    /**
     * Creates an {@link ImageSegmenter} instance from {@link ImageSegmenterOptions}.
     *
     * @param modelPath path of the segmentation model with metadata in the assets
     * @throws IOException if an I/O error occurs when loading the tflite model
     * @throws AssertionError if error occurs when creating {@link ImageSegmenter} from the native
     *     code
     */
    public static ImageSegmenter createFromFileAndOptions(Context context, String modelPath,
            final ImageSegmenterOptions options) throws IOException {
        try (AssetFileDescriptor assetFileDescriptor = context.getAssets().openFd(modelPath)) {
            long nativeHandle = TaskJniUtils.createHandleFromLibrary(new EmptyHandleProvider() {
                @Override
                public long createHandle() {
                    return initJniWithModelFdAndOptions(
                            /*fileDescriptor=*/assetFileDescriptor.getParcelFileDescriptor()
                                    .getFd(),
                            /*fileDescriptorLength=*/assetFileDescriptor.getLength(),
                            /*fileDescriptorOffset=*/assetFileDescriptor.getStartOffset(),
                            options.getDisplayNamesLocale(), options.getOutputType().getValue());
                }
            }, IMAGE_SEGMENTER_NATIVE_LIB);
            return new ImageSegmenter(nativeHandle, options.getOutputType());
        }
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

        public abstract String getDisplayNamesLocale();

        public abstract OutputType getOutputType();

        public static Builder builder() {
            return new AutoValue_ImageSegmenter_ImageSegmenterOptions.Builder()
                    .setDisplayNamesLocale(DEFAULT_DISPLAY_NAME_LOCALE)
                    .setOutputType(DEFAULT_OUTPUT_TYPE);
        }

        /** Builder for {@link ImageSegmenterOptions}. */
        @AutoValue.Builder
        public abstract static class Builder {
            /**
             * Sets the locale to use for display names specified through the TFLite Model Metadata,
             * if any.
             *
             * <p>Defaults to English({@code "en"}). See the <a
             * href="https://github.com/tensorflow/tflite-support/blob/3ce83f0cfe2c68fecf83e019f2acc354aaba471f/tensorflow_lite_support/metadata/metadata_schema.fbs#L147">TFLite
             * Metadata schema file.</a> for the accepted pattern of locale.
             */
            public abstract Builder setDisplayNamesLocale(String displayNamesLocale);

            public abstract Builder setOutputType(OutputType outputType);

            public abstract ImageSegmenterOptions build();
        }
    }

    /**
     * Performs actual segmentation on the provided image.
     *
     * @param image a {@link TensorImage} object that represents an RGB image
     * @return results of performing image segmentation. Note that at the time, a single {@link
     *     Segmentation} element is expected to be returned. The result is stored in a {@link List}
     *     for later extension to e.g. instance segmentation models, which may return one
     * segmentation per object.
     * @throws AssertionError if error occurs when segmenting the image from the native code
     */
    public List<Segmentation> segment(TensorImage image) {
        return segment(image, ImageProcessingOptions.builder().build());
    }

    /**
     * Performs actual segmentation on the provided image with {@link ImageProcessingOptions}.
     *
     * @param image a {@link TensorImage} object that represents an RGB image
     * @param options {@link ImageSegmenter} only supports image rotation (through {@link
     *     ImageProcessingOptions#Builder#setOrientation}) currently. The orientation of an image
     *     defaults to {@link ImageProcessingOptions#Orientation#TOP_LEFT}.
     * @return results of performing image segmentation. Note that at the time, a single {@link
     *     Segmentation} element is expected to be returned. The result is stored in a {@link List}
     *     for later extension to e.g. instance segmentation models, which may return one
     * segmentation per object.
     * @throws AssertionError if error occurs when segmenting the image from the native code
     */
    public List<Segmentation> segment(TensorImage image, ImageProcessingOptions options) {
        checkNotClosed();

        // image_segmenter_jni.cc expects an uint8 image. Convert image of other types into uint8.
        TensorImage imageUint8 = image.getDataType() == DataType.UINT8
                ? image
                : TensorImage.createFrom(image, DataType.UINT8);
        List<byte[]> maskByteArrays = new ArrayList<>();
        List<ColoredLabel> coloredLabels = new ArrayList<>();
        int[] maskShape = new int[2];
        segmentNative(getNativeHandle(), imageUint8.getBuffer(), imageUint8.getWidth(),
                imageUint8.getHeight(), maskByteArrays, maskShape, coloredLabels,
                options.getOrientation().getValue());

        List<ByteBuffer> maskByteBuffers = new ArrayList<>();
        for (byte[] bytes : maskByteArrays) {
            ByteBuffer byteBuffer = ByteBuffer.wrap(bytes);
            // Change the byte order to little_endian, since the buffers were generated in jni.
            byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
            maskByteBuffers.add(byteBuffer);
        }

        return Arrays.asList(Segmentation.create(outputType,
                outputType.createMasksFromBuffer(maskByteBuffers, maskShape), coloredLabels));
    }

    private static native long initJniWithModelFdAndOptions(int fileDescriptor,
            long fileDescriptorLength, long fileDescriptorOffset, String displayNamesLocale,
            int outputType);

    /**
     * The native method to segment the image.
     *
     * <p>{@code maskBuffers}, {@code maskShape}, {@code coloredLabels} will be updated in the
     * native layer.
     */
    private static native void segmentNative(long nativeHandle, ByteBuffer image, int width,
            int height, List<byte[]> maskByteArrays, int[] maskShape,
            List<ColoredLabel> coloredLabels, int orientation);

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
