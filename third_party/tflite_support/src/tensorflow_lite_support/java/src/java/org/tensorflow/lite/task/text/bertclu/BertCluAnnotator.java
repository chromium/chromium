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

package org.tensorflow.lite.task.text.bertclu;

import android.content.Context;
import androidx.annotation.Nullable;
import com.google.auto.value.AutoValue;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import org.tensorflow.lite.task.core.BaseOptions;
import org.tensorflow.lite.task.core.BaseTaskApi;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/**
 * API for BERT-based Conversational Language Understanding.
 *
 * <p>The API expects a Bert based TFLite model with metadata populated. The metadata should contain
 * the following information:
 *
 * <ul>
 *   <li>input_process_units for Wordpiece Tokenizer.
 *   <li>3 input tensors with names "ids", "mask" and "segment_ids".
 *   <li>6 output tensors with names "domain_task/names", "domain_task/scores", "intent_task/names",
 *       "intent_task/scores", "slot_task/names", and "slot_task/scores".
 * </ul>
 */
public class BertCluAnnotator extends BaseTaskApi {
  private static final String BERT_CLU_ANNOTATOR_NATIVE_LIBNAME = "task_text_jni";
  /**
   * Creates a {@link BertCluAnnotator} instance from a filepath.
   *
   * @param modelPath path of the annotator model
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertCluAnnotator createFromFile(Context context, String modelPath)
      throws IOException {
    return createFromBufferAndOptions(
        TaskJniUtils.loadMappedFile(context, modelPath), BertCluAnnotatorOptions.builder().build());
  }

  /**
   * Creates a {@link BertCluAnnotator} instance with a model buffer and {@link
   * BertCluAnnotatorOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the annotator
   *     model
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static BertCluAnnotator createFromBufferAndOptions(
      final ByteBuffer modelBuffer, final BertCluAnnotatorOptions options) {
    if (!(modelBuffer.isDirect() || modelBuffer instanceof MappedByteBuffer)) {
      throw new IllegalArgumentException(
          "The model buffer should be either a direct ByteBuffer or a MappedByteBuffer.");
    }

    return new BertCluAnnotator(
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
            BERT_CLU_ANNOTATOR_NATIVE_LIBNAME));
  }

  /**
   * Annotates the input utterances.
   *
   * @param cluRequest input dialogue encoded in a {@link CluRequest}
   * @return domain, intent, and slot annotations encoded in a {@link CluResponse}
   */
  public CluResponse annotate(CluRequest cluRequest) {
    return annotateNative(getNativeHandle(), cluRequest);
  }

  /** Options for setting up a {@link BertCluAnnotator}. */
  @AutoValue
  @UsedByReflection("bert_clu_annotator_jni.cc")
  public abstract static class BertCluAnnotatorOptions {
    private static final int DEFAULT_MAX_HISTORY_TURNS = 5;
    private static final float DEFAULT_DOMAIN_THRESHOLD = 0.5f;
    private static final float DEFAULT_INTENT_THRESHOLD = 0.5f;
    private static final float DEFAULT_CATEGORICAL_SLOT_THRESHOLD = 0.5f;
    private static final float DEFAULT_MENTIONED_SLOT_THRESHOLD = 0.5f;

    @UsedByReflection("bert_clu_annotator_jni.cc")
    abstract int getMaxHistoryTurns();

    @UsedByReflection("bert_clu_annotator_jni.cc")
    abstract float getDomainThreshold();

    @UsedByReflection("bert_clu_annotator_jni.cc")
    abstract float getIntentThreshold();

    @UsedByReflection("bert_clu_annotator_jni.cc")
    abstract float getCategoricalSlotThreshold();

    @UsedByReflection("bert_clu_annotator_jni.cc")
    abstract float getMentionedSlotThreshold();

    @Nullable
    abstract BaseOptions getBaseOptions();

    public static Builder builder() {
      return new AutoValue_BertCluAnnotator_BertCluAnnotatorOptions.Builder()
          .setMaxHistoryTurns(DEFAULT_MAX_HISTORY_TURNS)
          .setDomainThreshold(DEFAULT_DOMAIN_THRESHOLD)
          .setIntentThreshold(DEFAULT_INTENT_THRESHOLD)
          .setCategoricalSlotThreshold(DEFAULT_CATEGORICAL_SLOT_THRESHOLD)
          .setMentionedSlotThreshold(DEFAULT_MENTIONED_SLOT_THRESHOLD);
    }

    /** Builder for {@link BertCluAnnotatorOptions}. */
    @AutoValue.Builder
    public abstract static class Builder {
      /** Sets the general options to configure Task APIs, such as accelerators. */
      public abstract Builder setBaseOptions(@Nullable BaseOptions baseOptions);

      public abstract Builder setMaxHistoryTurns(int maxHistoryTurns);

      public abstract Builder setDomainThreshold(float domainThreshold);

      public abstract Builder setIntentThreshold(float intentThreshold);

      public abstract Builder setCategoricalSlotThreshold(float categoricalSlotThreshold);

      public abstract Builder setMentionedSlotThreshold(float mentionedSlotThreshold);

      public abstract BertCluAnnotatorOptions build();
    }
  }

  private BertCluAnnotator(long nativeHandle) {
    super(nativeHandle);
  }

  private static native long initJniWithByteBuffer(
      BertCluAnnotatorOptions options, ByteBuffer modelBuffer, long baseOptionsHandle);

  private static native CluResponse annotateNative(long nativeHandle, CluRequest cluRequest);

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
