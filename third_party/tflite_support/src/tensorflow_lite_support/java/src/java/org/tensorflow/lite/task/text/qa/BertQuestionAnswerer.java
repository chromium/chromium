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

package org.tensorflow.lite.task.text.qa;

import android.content.Context;
import android.os.ParcelFileDescriptor;
import com.google.auto.value.AutoValue;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;
import org.tensorflow.lite.task.core.BaseOptions;
import org.tensorflow.lite.task.core.BaseTaskApi;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.core.TaskJniUtils.FdAndOptionsHandleProvider;
import org.tensorflow.lite.task.core.TaskJniUtils.MultipleBuffersHandleProvider;

/**
 * Returns the most possible answers on a given question for QA models (BERT, Albert, etc.).
 *
 * <p>The API expects a Bert based TFLite model with metadata containing the following information:
 *
 * <ul>
 *   <li>input_process_units for Wordpiece/Sentencepiece Tokenizer - Wordpiece Tokenizer can be used
 *       for a <a
 *       href="https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1">MobileBert</a> model,
 *       Sentencepiece Tokenizer Tokenizer can be used for an <a
 *       href="https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/1">Albert</a> model.
 *   <li>3 input tensors with names "ids", "mask" and "segment_ids".
 *   <li>2 output tensors with names "end_logits" and "start_logits".
 * </ul>
 */
public class BertQuestionAnswerer extends BaseTaskApi implements QuestionAnswerer {
  private static final String BERT_QUESTION_ANSWERER_NATIVE_LIBNAME = "task_text_jni";
  private static final int OPTIONAL_FD_LENGTH = -1;
  private static final int OPTIONAL_FD_OFFSET = -1;

  /**
   * Creates a {@link BertQuestionAnswerer} instance from the default {@link
   * BertQuestionAnswererOptions}.
   *
   * @param context android context
   * @param modelPath file path to the model with metadata. Note: The model should not be compressed
   * @return a {@link BertQuestionAnswerer} instance
   * @throws IOException if model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertQuestionAnswerer createFromFile(Context context, String modelPath)
      throws IOException {
    return createFromFileAndOptions(
        context, modelPath, BertQuestionAnswererOptions.builder().build());
  }

  /**
   * Creates a {@link BertQuestionAnswerer} instance from the default {@link
   * BertQuestionAnswererOptions}.
   *
   * @param modelFile a {@link File} object of the model
   * @return a {@link BertQuestionAnswerer} instance
   * @throws IOException if model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertQuestionAnswerer createFromFile(File modelFile) throws IOException {
    return createFromFileAndOptions(modelFile, BertQuestionAnswererOptions.builder().build());
  }

  /**
   * Creates a {@link BertQuestionAnswerer} instance from {@link BertQuestionAnswererOptions}.
   *
   * @param context android context
   * @param modelPath file path to the model with metadata. Note: The model should not be compressed
   * @return a {@link BertQuestionAnswerer} instance
   * @throws IOException if model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertQuestionAnswerer createFromFileAndOptions(
      Context context, String modelPath, BertQuestionAnswererOptions options) throws IOException {
    return new BertQuestionAnswerer(
        TaskJniUtils.createHandleFromFdAndOptions(
            context,
            new FdAndOptionsHandleProvider<BertQuestionAnswererOptions>() {
              @Override
              public long createHandle(
                  int fileDescriptor,
                  long fileDescriptorLength,
                  long fileDescriptorOffset,
                  BertQuestionAnswererOptions options) {
                return initJniWithFileDescriptor(
                    fileDescriptor,
                    fileDescriptorLength,
                    fileDescriptorOffset,
                    TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions()));
              }
            },
            BERT_QUESTION_ANSWERER_NATIVE_LIBNAME,
            modelPath,
            options));
  }

  /**
   * Creates a {@link BertQuestionAnswerer} instance from {@link BertQuestionAnswererOptions}.
   *
   * @param modelFile a {@link File} object of the model
   * @return a {@link BertQuestionAnswerer} instance
   * @throws IOException if model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertQuestionAnswerer createFromFileAndOptions(
      File modelFile, final BertQuestionAnswererOptions options) throws IOException {
    try (ParcelFileDescriptor descriptor =
        ParcelFileDescriptor.open(modelFile, ParcelFileDescriptor.MODE_READ_ONLY)) {
      return new BertQuestionAnswerer(
          TaskJniUtils.createHandleFromLibrary(
              new EmptyHandleProvider() {
                @Override
                public long createHandle() {
                  return initJniWithFileDescriptor(
                      /*fileDescriptor=*/ descriptor.getFd(),
                      /*fileDescriptorLength=*/ OPTIONAL_FD_LENGTH,
                      /*fileDescriptorOffset=*/ OPTIONAL_FD_OFFSET,
                      TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions()));
                }
              },
              BERT_QUESTION_ANSWERER_NATIVE_LIBNAME));
    }
  }

  /**
   * Creates a {@link BertQuestionAnswerer} instance with a Bert model and a vocabulary file.
   *
   * <p>One suitable model is: https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1
   *
   * @param context android context
   * @param modelPath file path to the Bert model. Note: The model should not be compressed
   * @param vocabPath file path to the vocabulary file. Note: The file should not be compressed
   * @return a {@link BertQuestionAnswerer} instance
   * @throws IOException If model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertQuestionAnswerer createBertQuestionAnswererFromFile(
      Context context, String modelPath, String vocabPath) throws IOException {
    return new BertQuestionAnswerer(
        TaskJniUtils.createHandleWithMultipleAssetFilesFromLibrary(
            context,
            new MultipleBuffersHandleProvider() {
              @Override
              public long createHandle(ByteBuffer... buffers) {
                return initJniWithBertByteBuffers(buffers);
              }
            },
            BERT_QUESTION_ANSWERER_NATIVE_LIBNAME,
            modelPath,
            vocabPath));
  }

  /**
   * Creates a {@link BertQuestionAnswerer} instance with an Albert model and a sentence piece model
   * file.
   *
   * <p>One suitable model is: https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/1
   *
   * @param context android context
   * @param modelPath file path to the Albert model. Note: The model should not be compressed
   * @param sentencePieceModelPath file path to the sentence piece model file. Note: The model
   *     should not be compressed
   * @return a {@link BertQuestionAnswerer} instance
   * @throws IOException If model file fails to load
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertQuestionAnswerer createAlbertQuestionAnswererFromFile(
      Context context, String modelPath, String sentencePieceModelPath) throws IOException {
    return new BertQuestionAnswerer(
        TaskJniUtils.createHandleWithMultipleAssetFilesFromLibrary(
            context,
            new MultipleBuffersHandleProvider() {
              @Override
              public long createHandle(ByteBuffer... buffers) {
                return initJniWithAlbertByteBuffers(buffers);
              }
            },
            BERT_QUESTION_ANSWERER_NATIVE_LIBNAME,
            modelPath,
            sentencePieceModelPath));
  }

  /** Options for setting up a {@link BertQuestionAnswerer}. */
  @AutoValue
  public abstract static class BertQuestionAnswererOptions {
    abstract BaseOptions getBaseOptions();

    public static Builder builder() {
      return new AutoValue_BertQuestionAnswerer_BertQuestionAnswererOptions.Builder()
          .setBaseOptions(BaseOptions.builder().build());
    }

    /** Builder for {@link BertQuestionAnswererOptions}. */
    @AutoValue.Builder
    public abstract static class Builder {
      /** Sets the general options to configure Task APIs, such as accelerators. */
      public abstract Builder setBaseOptions(BaseOptions baseOptions);

      public abstract BertQuestionAnswererOptions build();
    }
  }

  @Override
  public List<QaAnswer> answer(String context, String question) {
    checkNotClosed();
    return answerNative(getNativeHandle(), context, question);
  }

  private BertQuestionAnswerer(long nativeHandle) {
    super(nativeHandle);
  }

  // modelBuffers[0] is tflite model file buffer, and modelBuffers[1] is vocab file buffer.
  private static native long initJniWithBertByteBuffers(ByteBuffer... modelBuffers);

  // modelBuffers[0] is tflite model file buffer, and modelBuffers[1] is sentencepiece model file
  // buffer.
  private static native long initJniWithAlbertByteBuffers(ByteBuffer... modelBuffers);

  private static native long initJniWithFileDescriptor(
      int fileDescriptor,
      long fileDescriptorLength,
      long fileDescriptorOffset,
      long baseOptionsHandle);

  private static native List<QaAnswer> answerNative(
      long nativeHandle, String context, String question);

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
