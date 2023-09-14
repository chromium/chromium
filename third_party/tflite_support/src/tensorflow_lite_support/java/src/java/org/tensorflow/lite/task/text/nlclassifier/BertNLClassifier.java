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
 * Classifier API for NLClassification tasks with Bert models, categorizes string into different
 * classes. The API expects a Bert based TFLite model with metadata populated.
 *
 * <p>The metadata should contain the following information:
 *
 * <ul>
 *   <li>1 input_process_unit for Wordpiece/Sentencepiece Tokenizer.
 *   <li>3 input tensors with names "ids", "mask" and "segment_ids".
 *   <li>1 output tensor of type float32[1, 2], with a optionally attached label file. If a label
 *       file is attached, the file should be a plain text file with one label per line, the number
 *       of labels should match the number of categories the model outputs.
 * </ul>
 */
public class BertNLClassifier extends BaseTaskApi {

  private static final String BERT_NL_CLASSIFIER_NATIVE_LIBNAME = "task_text_jni";

  /** Options to configure BertNLClassifier. */
  @AutoValue
  @UsedByReflection("bert_nl_classifier_jni.cc")
  public abstract static class BertNLClassifierOptions {
    static final int DEFAULT_MAX_SEQ_LEN = 128;

    abstract int getMaxSeqLen();

    abstract BaseOptions getBaseOptions();

    public static Builder builder() {
      return new AutoValue_BertNLClassifier_BertNLClassifierOptions.Builder()
          .setMaxSeqLen(DEFAULT_MAX_SEQ_LEN)
          .setBaseOptions(BaseOptions.builder().build());
    }

    /** Builder for {@link BertNLClassifierOptions}. */
    @AutoValue.Builder
    public abstract static class Builder {

      /** Sets the general options to configure Task APIs, such as accelerators. */
      public abstract Builder setBaseOptions(BaseOptions baseOptions);

      /**
       * Set the maximum sequence length.
       *
       * @deprecated maximum sequence length is now read from the model (i.e. input tensor size)
       *     automatically
       */
      @Deprecated
      public abstract Builder setMaxSeqLen(int value);

      public abstract BertNLClassifierOptions build();
    }
  }

  /**
   * Creates {@link BertNLClassifier} from a model file with metadata and default {@link
   * BertNLClassifierOptions}.
   *
   * @param context Android context
   * @param modelPath Path to the classification model
   * @return a {@link BertNLClassifier} instance
   * @throws IOException If model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertNLClassifier createFromFile(final Context context, final String modelPath)
      throws IOException {
    return createFromBuffer(TaskJniUtils.loadMappedFile(context, modelPath));
  }

  /**
   * Creates {@link BertNLClassifier} from a {@link File} object with metadata and default {@link
   * BertNLClassifierOptions}.
   *
   * @param modelFile The classification model {@link File} instance
   * @return a {@link BertNLClassifier} instance
   * @throws IOException If model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertNLClassifier createFromFile(File modelFile) throws IOException {
    return createFromFileAndOptions(modelFile, BertNLClassifierOptions.builder().build());
  }

  /**
   * Creates {@link BertNLClassifier} from a model file with metadata and {@link
   * BertNLClassifierOptions}.
   *
   * @param context Android context.
   * @param modelPath Path to the classification model
   * @param options to configure the classifier
   * @return a {@link BertNLClassifier} instance
   * @throws IOException If model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertNLClassifier createFromFileAndOptions(
      final Context context, final String modelPath, BertNLClassifierOptions options)
      throws IOException {
    return createFromBufferAndOptions(TaskJniUtils.loadMappedFile(context, modelPath), options);
  }

  /**
   * Creates {@link BertNLClassifier} from a {@link File} object with metadata and {@link
   * BertNLClassifierOptions}.
   *
   * @param modelFile The classification model {@link File} instance
   * @param options to configure the classifier
   * @return a {@link BertNLClassifier} instance
   * @throws IOException If model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertNLClassifier createFromFileAndOptions(
      File modelFile, final BertNLClassifierOptions options) throws IOException {
    try (ParcelFileDescriptor descriptor =
        ParcelFileDescriptor.open(modelFile, ParcelFileDescriptor.MODE_READ_ONLY)) {
      return new BertNLClassifier(
          TaskJniUtils.createHandleFromLibrary(
              new EmptyHandleProvider() {
                @Override
                public long createHandle() {
                  return initJniWithFileDescriptor(
                      descriptor.getFd(),
                      options,
                      TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions()));
                }
              },
              BERT_NL_CLASSIFIER_NATIVE_LIBNAME));
    }
  }

  /**
   * Creates {@link BertNLClassifier} with a model buffer and default {@link
   * BertNLClassifierOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the model
   * @return a {@link BertNLClassifier} instance
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertNLClassifier createFromBuffer(final ByteBuffer modelBuffer) {
    return createFromBufferAndOptions(modelBuffer, BertNLClassifierOptions.builder().build());
  }

  /**
   * Creates {@link BertNLClassifier} with a model buffer and {@link BertNLClassifierOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the model
   * @param options to configure the classifier
   * @return a {@link BertNLClassifier} instance
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertNLClassifier createFromBufferAndOptions(
      final ByteBuffer modelBuffer, final BertNLClassifierOptions options) {
    if (!(modelBuffer.isDirect() || modelBuffer instanceof MappedByteBuffer)) {
      throw new IllegalArgumentException(
          "The model buffer should be either a direct ByteBuffer or a MappedByteBuffer.");
    }
    return new BertNLClassifier(
        TaskJniUtils.createHandleFromLibrary(
            new EmptyHandleProvider() {
              @Override
              public long createHandle() {
                return initJniWithByteBuffer(
                    modelBuffer,
                    options,
                    TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions()));
              }
            },
            BERT_NL_CLASSIFIER_NATIVE_LIBNAME));
  }

  /**
   * Performs classification on a string input, returns classified {@link Category}s.
   *
   * @param text input text to the model.
   * @return A list of Category results.
   */
  public List<Category> classify(String text) {
    return classifyNative(getNativeHandle(), text);
  }

  /**
   * Constructor to initialize the JNI with a pointer from C++.
   *
   * @param nativeHandle a pointer referencing memory allocated in C++.
   */
  private BertNLClassifier(long nativeHandle) {
    super(nativeHandle);
  }

  private static native long initJniWithByteBuffer(
      ByteBuffer modelBuffer, BertNLClassifierOptions options, long baseOptionsHandle);

  private static native long initJniWithFileDescriptor(
      int fd, BertNLClassifierOptions options, long baseOptionsHandle);

  private static native List<Category> classifyNative(long nativeHandle, String text);

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
