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

package org.tensorflow.lite.task.text.nlclassifier;

import android.content.Context;
import android.os.ParcelFileDescriptor;
import androidx.annotation.Nullable;
import com.google.auto.value.AutoValue;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.util.List;
import org.tensorflow.lite.support.label.Category;
import org.tensorflow.lite.task.core.BaseOptions;
import org.tensorflow.lite.task.core.BaseTaskApi;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/**
 * Classifier API for natural language classification tasks, categorizes string into different
 * classes.
 *
 * <p>The API expects a TFLite model with the following input/output tensor:
 *
 * <ul>
 *   <li>Input tensor (kTfLiteString)
 *       <ul>
 *         <li>input of the model, accepts a string.
 *       </ul>
 *   <li>Output score tensor
 *       (kTfLiteUInt8/kTfLiteInt8/kTfLiteInt16/kTfLiteFloat32/kTfLiteFloat64/kTfLiteBool)
 *       <ul>
 *         <li>output scores for each class, if type is one of the Int types, dequantize it, if it
 *             is Bool type, convert the values to 0.0 and 1.0 respectively.
 *         <li>can have an optional associated file in metadata for labels, the file should be a
 *             plain text file with one label per line, the number of labels should match the number
 *             of categories the model outputs. Output label tensor: optional (kTfLiteString) -
 *             output classname for each class, should be of the same length with scores. If this
 *             tensor is not present, the API uses score indices as classnames. - will be ignored if
 *             output score tensor already has an associated label file.
 *       </ul>
 *   <li>Optional Output label tensor (kTfLiteString/kTfLiteInt32)
 *       <ul>
 *         <li>output classname for each class, should be of the same length with scores. If this
 *             tensor is not present, the API uses score indices as classnames.
 *         <li>will be ignored if output score tensor already has an associated labe file.
 *       </ul>
 * </ul>
 *
 * <p>By default the API tries to find the input/output tensors with default configurations in
 * {@link NLClassifierOptions}, with tensor name prioritized over tensor index. The option is
 * configurable for different TFLite models.
 */
public class NLClassifier extends BaseTaskApi {

  /** Options to identify input and output tensors of the model. */
  @AutoValue
  @UsedByReflection("nl_classifier_jni.cc")
  public abstract static class NLClassifierOptions {
    private static final int DEFAULT_INPUT_TENSOR_INDEX = 0;
    private static final int DEFAULT_OUTPUT_SCORE_TENSOR_INDEX = 0;
    // By default there is no output label tensor. The label file can be attached
    // to the output score tensor metadata.
    private static final int DEFAULT_OUTPUT_LABEL_TENSOR_INDEX = -1;
    private static final String DEFAULT_INPUT_TENSOR_NAME = "INPUT";
    private static final String DEFAULT_OUTPUT_SCORE_TENSOR_NAME = "OUTPUT_SCORE";
    private static final String DEFAULT_OUTPUT_LABEL_TENSOR_NAME = "OUTPUT_LABEL";

    @UsedByReflection("nl_classifier_jni.cc")
    abstract int getInputTensorIndex();

    @UsedByReflection("nl_classifier_jni.cc")
    abstract int getOutputScoreTensorIndex();

    @UsedByReflection("nl_classifier_jni.cc")
    abstract int getOutputLabelTensorIndex();

    @UsedByReflection("nl_classifier_jni.cc")
    abstract String getInputTensorName();

    @UsedByReflection("nl_classifier_jni.cc")
    abstract String getOutputScoreTensorName();

    @UsedByReflection("nl_classifier_jni.cc")
    abstract String getOutputLabelTensorName();

    @Nullable
    abstract BaseOptions getBaseOptions();

    public static Builder builder() {
      return new AutoValue_NLClassifier_NLClassifierOptions.Builder()
          .setInputTensorIndex(DEFAULT_INPUT_TENSOR_INDEX)
          .setOutputScoreTensorIndex(DEFAULT_OUTPUT_SCORE_TENSOR_INDEX)
          .setOutputLabelTensorIndex(DEFAULT_OUTPUT_LABEL_TENSOR_INDEX)
          .setInputTensorName(DEFAULT_INPUT_TENSOR_NAME)
          .setOutputScoreTensorName(DEFAULT_OUTPUT_SCORE_TENSOR_NAME)
          .setOutputLabelTensorName(DEFAULT_OUTPUT_LABEL_TENSOR_NAME);
    }

    /** Builder for {@link NLClassifierOptions}. */
    @AutoValue.Builder
    public abstract static class Builder {
      /** Sets the general options to configure Task APIs, such as accelerators. */
      public abstract Builder setBaseOptions(@Nullable BaseOptions baseOptions);

      /**
       * Configure the input/output tensors for NLClassifier:
       *
       * <p>- No special configuration is needed if the model has only one input tensor and one
       * output tensor.
       *
       * <p>- When the model has multiple input or output tensors, use the following configurations
       * to specifiy the desired tensors: <br>
       * -- tensor names: {@code inputTensorName}, {@code outputScoreTensorName}, {@code
       * outputLabelTensorName}<br>
       * -- tensor indices: {@code inputTensorIndex}, {@code outputScoreTensorIndex}, {@code
       * outputLabelTensorIndex} <br>
       * Tensor names has higher priorities than tensor indices in locating the tensors. It means
       * the tensors will be first located according to tensor names. If not found, then the tensors
       * will be located according to tensor indices.
       *
       * <p>- Failing to match the input text tensor or output score tensor with neither tensor
       * names nor tensor indices will trigger a runtime error. However, failing to locate the
       * output label tensor will not trigger an error because the label tensor is optional.
       */

      /**
       * Set the name of the input text tensor, if the model has multiple inputs. Only the input
       * tensor specified will be used for inference; other input tensors will be ignored. Dafualt
       * to {@code "INPUT"}.
       *
       * <p>See the section, Configure the input/output tensors for NLClassifier, for more details.
       */
      public abstract Builder setInputTensorName(String inputTensorName);

      /**
       * Set the name of the output score tensor, if the model has multiple outputs. Dafualt to
       * {@code "OUTPUT_SCORE"}.
       *
       * <p>See the section, Configure the input/output tensors for NLClassifier, for more details.
       */
      public abstract Builder setOutputScoreTensorName(String outputScoreTensorName);

      /**
       * Set the name of the output label tensor, if the model has multiple outputs. Dafualt to
       * {@code "OUTPUT_LABEL"}.
       *
       * <p>See the section, Configure the input/output tensors for NLClassifier, for more details.
       *
       * <p>By default, label file should be packed with the output score tensor through Model
       * Metadata. See the <a
       * href="https://www.tensorflow.org/lite/convert/metadata_writer_tutorial#natural_language_classifiers">MetadataWriter
       * for NLClassifier</a>. NLClassifier reads and parses labels from the label file
       * automatically. However, some models may output a specific label tensor instead. In this
       * case, NLClassifier reads labels from the output label tensor.
       */
      public abstract Builder setOutputLabelTensorName(String outputLabelTensorName);

      /**
       * Set the index of the input text tensor among all input tensors, if the model has multiple
       * inputs. Only the input tensor specified will be used for inference; other input tensors
       * will be ignored. Dafualt to 0.
       *
       * <p>See the section, Configure the input/output tensors for NLClassifier, for more details.
       */
      public abstract Builder setInputTensorIndex(int inputTensorIndex);

      /**
       * Set the index of the output score tensor among all output tensors, if the model has
       * multiple outputs. Dafualt to 0.
       *
       * <p>See the section, Configure the input/output tensors for NLClassifier, for more details.
       */
      public abstract Builder setOutputScoreTensorIndex(int outputScoreTensorIndex);

      /**
       * Set the index of the optional output label tensor among all output tensors, if the model
       * has multiple outputs.
       *
       * <p>See the document above {@code outputLabelTensorName} for more information about what the
       * output label tensor is.
       *
       * <p>See the section, Configure the input/output tensors for NLClassifier, for more details.
       *
       * <p>{@code outputLabelTensorIndex} dafualts to -1, meaning to disable the output label
       * tensor.
       */
      public abstract Builder setOutputLabelTensorIndex(int outputLabelTensorIndex);

      public abstract NLClassifierOptions build();
    }
  }

  private static final String NL_CLASSIFIER_NATIVE_LIBNAME = "task_text_jni";

  /**
   * Creates {@link NLClassifier} from default {@link NLClassifierOptions}.
   *
   * @param context Android context
   * @param modelPath path to the classification model relative to asset dir
   * @return an {@link NLClassifier} instance
   * @throws IOException if model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static NLClassifier createFromFile(Context context, String modelPath) throws IOException {
    return createFromFileAndOptions(context, modelPath, NLClassifierOptions.builder().build());
  }

  /**
   * Creates {@link NLClassifier} from default {@link NLClassifierOptions}.
   *
   * @param modelFile the classification model {@link File} instance
   * @return an {@link NLClassifier} instance
   * @throws IOException if model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static NLClassifier createFromFile(File modelFile) throws IOException {
    return createFromFileAndOptions(modelFile, NLClassifierOptions.builder().build());
  }

  /**
   * Creates {@link NLClassifier} from {@link NLClassifierOptions}.
   *
   * @param context Android context
   * @param modelPath path to the classification model relative to asset dir
   * @param options configurations for the model.
   * @return an {@link NLClassifier} instance
   * @throws IOException if model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static NLClassifier createFromFileAndOptions(
      Context context, String modelPath, NLClassifierOptions options) throws IOException {
    return createFromBufferAndOptions(TaskJniUtils.loadMappedFile(context, modelPath), options);
  }

  /**
   * Creates {@link NLClassifier} from {@link NLClassifierOptions}.
   *
   * @param modelFile the classification model {@link File} instance
   * @param options configurations for the model
   * @return an {@link NLClassifier} instance
   * @throws IOException if model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static NLClassifier createFromFileAndOptions(
      File modelFile, final NLClassifierOptions options) throws IOException {
    try (ParcelFileDescriptor descriptor =
        ParcelFileDescriptor.open(modelFile, ParcelFileDescriptor.MODE_READ_ONLY)) {
      return new NLClassifier(
          TaskJniUtils.createHandleFromLibrary(
              new EmptyHandleProvider() {
                @Override
                public long createHandle() {
                  long baseOptionsHandle =
                      options.getBaseOptions() == null
                          ? 0 // pass an invalid native handle
                          : TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions());
                  return initJniWithFileDescriptor(options, descriptor.getFd(), baseOptionsHandle);
                }
              },
              NL_CLASSIFIER_NATIVE_LIBNAME));
    }
  }

  /**
   * Creates {@link NLClassifier} with a model {@link ByteBuffer} and {@link NLClassifierOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the
   *     classification model
   * @param options configurations for the model
   * @return {@link NLClassifier} instance
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   */
  public static NLClassifier createFromBufferAndOptions(
      final ByteBuffer modelBuffer, final NLClassifierOptions options) {
    if (!(modelBuffer.isDirect() || modelBuffer instanceof MappedByteBuffer)) {
      throw new IllegalArgumentException(
          "The model buffer should be either a direct ByteBuffer or a MappedByteBuffer.");
    }

    return new NLClassifier(
        TaskJniUtils.createHandleFromLibrary(
            new EmptyHandleProvider() {
              @Override
              public long createHandle() {
                long baseOptionsHandle =
                    options.getBaseOptions() == null
                        ? 0 // pass an invalid native handle
                        : TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions());
                return initJniWithByteBuffer(options, modelBuffer, baseOptionsHandle);
              }
            },
            NL_CLASSIFIER_NATIVE_LIBNAME));
  }

  /**
   * Performs classification on a string input, returns classified {@link Category}s.
   *
   * @param text input text to the model
   * @return a list of Category results
   */
  public List<Category> classify(String text) {
    return classifyNative(getNativeHandle(), text);
  }

  /**
   * Constructor to initialize the JNI with a pointer from C++.
   *
   * @param nativeHandle a pointer referencing memory allocated in C++.
   */
  protected NLClassifier(long nativeHandle) {
    super(nativeHandle);
  }

  @Override
  protected void deinit(long nativeHandle) {
    deinitJni(nativeHandle);
  }

  private static native long initJniWithByteBuffer(
      NLClassifierOptions options, ByteBuffer modelBuffer, long baseOptionsHandle);

  private static native long initJniWithFileDescriptor(
      NLClassifierOptions options, int fd, long baseOptionsHandle);

  private static native List<Category> classifyNative(long nativeHandle, String text);

  /**
   * Native implementation to release memory pointed by the pointer.
   *
   * @param nativeHandle pointer to memory allocated
   */
  private native void deinitJni(long nativeHandle);
}
