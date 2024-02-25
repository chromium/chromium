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

package org.tensorflow.lite.task.text.searcher;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.os.ParcelFileDescriptor;
import com.google.auto.value.AutoValue;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.util.List;
import org.tensorflow.lite.task.core.BaseOptions;
import org.tensorflow.lite.task.core.BaseTaskApi;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.processor.NearestNeighbor;
import org.tensorflow.lite.task.processor.SearcherOptions;

/**
 * Performs similarity search on text string.
 *
 * <p>The API expects a TFLite model with optional, but strongly recommended, <a
 * href="https://www.tensorflow.org/lite/convert/metadata">TFLite Model Metadata.</a>.
 *
 * <p>The API expects a TFLite model with metadata populated. The metadata should contain the
 * following information:
 *
 * <ul>
 *   <li>For Bert based TFLite model:
 *       <ul>
 *         <li>3 input tensors of type kTfLiteString with names "ids", "mask" and "segment_ids".
 *         <li>input_process_units for Wordpiece/Sentencepiece Tokenizer
 *         <li>exactly one output tensor of type kTfLiteFloat32
 *       </ul>
 *   <li>For Regex based TFLite model:
 *       <ul>
 *         <li>1 input tensor.
 *         <li>input_process_units for RegexTokenizer Tokenizer
 *         <li>exactly one output tensor of type kTfLiteFloat32
 *       </ul>
 *   <li>For Universal Sentence Encoder based TFLite model:
 *       <ul>
 *         <li>3 input tensors with names "inp_text", "res_context" and "res_text"
 *         <li>2 output tensors with names "query_encoding" and "response_encoding" of type
 *             kTfLiteFloat32
 *       </ul>
 * </ul>
 *
 * <p>TODO(b/180502532): add pointer to example model.
 *
 * <p>TODO(b/222671076): add factory create methods without options, such as `createFromFile`, once
 * the single file format (index file packed in the model) is supported.
 */
public final class TextSearcher extends BaseTaskApi {

  private static final String TEXT_SEARCHER_NATIVE_LIB = "task_text_jni";
  private static final int OPTIONAL_FD_LENGTH = -1;
  private static final int OPTIONAL_FD_OFFSET = -1;

  /**
   * Creates an {@link TextSearcher} instance from {@link TextSearcherOptions}.
   *
   * @param modelPath path of the search model with metadata in the assets
   * @throws IOException if an I/O error occurs when loading the tflite model or the index file
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static TextSearcher createFromFileAndOptions(
      Context context, String modelPath, final TextSearcherOptions options) throws IOException {
    try (AssetFileDescriptor assetFileDescriptor = context.getAssets().openFd(modelPath)) {
      return createFromModelFdAndOptions(
          /*modelDescriptor=*/ assetFileDescriptor.getParcelFileDescriptor().getFd(),
          /*modelDescriptorLength=*/ assetFileDescriptor.getLength(),
          /*modelDescriptorOffset=*/ assetFileDescriptor.getStartOffset(),
          options);
    }
  }

  /**
   * Creates an {@link TextSearcher} instance.
   *
   * @param modelFile the search model {@link File} instance
   * @throws IOException if an I/O error occurs when loading the tflite model or the index file
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static TextSearcher createFromFileAndOptions(
      File modelFile, final TextSearcherOptions options) throws IOException {
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
   * Creates an {@link TextSearcher} instance with a model buffer and {@link TextSearcherOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the search
   *     model
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   * @throws IOException if an I/O error occurs when loading the index file
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static TextSearcher createFromBufferAndOptions(
      final ByteBuffer modelBuffer, final TextSearcherOptions options) throws IOException {
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

  public static TextSearcher createFromBufferAndOptionsImpl(
      final ByteBuffer modelBuffer, final TextSearcherOptions options, final int indexFd) {
    return new TextSearcher(
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
            TEXT_SEARCHER_NATIVE_LIB));
  }

  /**
   * Constructor to initialize the JNI with a pointer from C++.
   *
   * @param nativeHandle a pointer referencing memory allocated in C++
   */
  TextSearcher(long nativeHandle) {
    super(nativeHandle);
  }

  /** Options for setting up an TextSearcher. */
  @AutoValue
  public abstract static class TextSearcherOptions {

    abstract BaseOptions getBaseOptions();

    abstract SearcherOptions getSearcherOptions();

    public static Builder builder() {
      return new AutoValue_TextSearcher_TextSearcherOptions.Builder()
          .setBaseOptions(BaseOptions.builder().build())
          .setSearcherOptions(SearcherOptions.builder().build());
    }

    /** Builder for {@link TextSearcherOptions}. */
    @AutoValue.Builder
    public abstract static class Builder {
      /** Sets the general options to configure Task APIs, such as accelerators. */
      public abstract Builder setBaseOptions(BaseOptions baseOptions);

      /** Sets the options to configure Searcher API. */
      public abstract Builder setSearcherOptions(SearcherOptions searcherOptions);

      public abstract TextSearcherOptions build();
    }
  }

  /**
   * Performs embedding extraction on the provided string input, followed by nearest-neighbor search
   * in the index.
   *
   * @param text input text query to the model
   */
  public List<NearestNeighbor> search(String text) {
    return searchNative(getNativeHandle(), text);
  }

  private static TextSearcher createFromModelFdAndOptions(
      final int modelDescriptor,
      final long modelDescriptorLength,
      final long modelDescriptorOffset,
      final TextSearcherOptions options)
      throws IOException {
    if (options.getSearcherOptions().getIndexFile() != null) {
      // indexDescriptor must be alive before TextSearcher is initialized completely in the native
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

  private static TextSearcher createFromModelFdAndOptionsImpl(
      final int modelDescriptor,
      final long modelDescriptorLength,
      final long modelDescriptorOffset,
      final TextSearcherOptions options,
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
            TEXT_SEARCHER_NATIVE_LIB);
    return new TextSearcher(nativeHandle);
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

  /** The native method to search an input text string. */
  private static native List<NearestNeighbor> searchNative(long nativeHandle, String text);

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
