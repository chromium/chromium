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

import org.tensorflow.lite.task.core.BaseTaskApi;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.core.TaskJniUtils.MultipleBuffersHandleProvider;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;

/** Task API for BertQA models. */
public class BertQuestionAnswerer extends BaseTaskApi implements QuestionAnswerer {
    private static final String BERT_QUESTION_ANSWERER_NATIVE_LIBNAME = "task_text_jni";

    private BertQuestionAnswerer(long nativeHandle) {
        super(nativeHandle);
    }

    /**
     * Generic API to create the QuestionAnswerer for bert models with metadata populated. The API
     * expects a Bert based TFLite model with metadata containing the following information:
     *
     * <ul>
     *   <li>input_process_units for Wordpiece/Sentencepiece Tokenizer - Wordpiece Tokenizer can be
     *       used for a <a
     *       href="https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1">MobileBert</a>
     *       model, Sentencepiece Tokenizer Tokenizer can be used for an <a
     *       href="https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/1">Albert</a>
     *       model.
     *   <li>3 input tensors with names "ids", "mask" and "segment_ids".
     *   <li>2 output tensors with names "end_logits" and "start_logits".
     * </ul>
     *
     * @param context android context
     * @param pathToModel file path to the model with metadata. Note: The model should not be
     *     compressed
     * @return {@link BertQuestionAnswerer} instance
     * @throws IOException If model file fails to load.
     */
    public static BertQuestionAnswerer createFromFile(Context context, String pathToModel)
            throws IOException {
        return new BertQuestionAnswerer(TaskJniUtils.createHandleWithMultipleAssetFilesFromLibrary(
                context, new MultipleBuffersHandleProvider() {
                    @Override
                    public long createHandle(ByteBuffer... buffers) {
                        return BertQuestionAnswerer.initJniWithModelWithMetadataByteBuffers(
                                buffers);
                    }
                }, BERT_QUESTION_ANSWERER_NATIVE_LIBNAME, pathToModel));
    }

    /**
     * Generic API to create the QuestionAnswerer for bert models with metadata populated. The API
     * expects a Bert based TFLite model with metadata containing the following information:
     *
     * <ul>
     *   <li>input_process_units for Wordpiece/Sentencepiece Tokenizer - Wordpiece Tokenizer can be
     *       used for a <a
     *       href="https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1">MobileBert</a>
     *       model, Sentencepiece Tokenizer Tokenizer can be used for an <a
     *       href="https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/1">Albert</a>
     *       model.
     *   <li>3 input tensors with names "ids", "mask" and "segment_ids".
     *   <li>2 output tensors with names "end_logits" and "start_logits".
     * </ul>
     *
     * @param modelFile {@link File} object of the model
     * @return {@link BertQuestionAnswerer} instance
     * @throws IOException If model file fails to load.
     */
    public static BertQuestionAnswerer createFromFile(File modelFile) throws IOException {
        try (ParcelFileDescriptor descriptor =
                        ParcelFileDescriptor.open(modelFile, ParcelFileDescriptor.MODE_READ_ONLY)) {
            return new BertQuestionAnswerer(
                    TaskJniUtils.createHandleFromLibrary(new EmptyHandleProvider() {
                        @Override
                        public long createHandle() {
                            return initJniWithFileDescriptor(descriptor.getFd());
                        }
                    }, BERT_QUESTION_ANSWERER_NATIVE_LIBNAME));
        }
    }

    /**
     * Creates the API instance with a bert model and vocabulary file.
     *
     * <p>One suitable model is: https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1
     *
     * @param context android context
     * @param pathToModel file path to the bert model. Note: The model should not be compressed
     * @param pathToVocab file path to the vocabulary file. Note: The file should not be compressed
     * @return {@link BertQuestionAnswerer} instance
     * @throws IOException If model file fails to load.
     */
    public static BertQuestionAnswerer createBertQuestionAnswererFromFile(
            Context context, String pathToModel, String pathToVocab) throws IOException {
        return new BertQuestionAnswerer(TaskJniUtils.createHandleWithMultipleAssetFilesFromLibrary(
                context, new MultipleBuffersHandleProvider() {
                    @Override
                    public long createHandle(ByteBuffer... buffers) {
                        return BertQuestionAnswerer.initJniWithBertByteBuffers(buffers);
                    }
                }, BERT_QUESTION_ANSWERER_NATIVE_LIBNAME, pathToModel, pathToVocab));
    }

    /**
     * Creates the API instance with an albert model and sentence piece model file.
     *
     * <p>One suitable model is: https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/1
     *
     * @param context android context
     * @param pathToModel file path to the albert model. Note: The model should not be compressed
     * @param pathToSentencePieceModel file path to the sentence piece model file. Note: The model
     *     should not be compressed
     * @return {@link BertQuestionAnswerer} instance
     * @throws IOException If model file fails to load.
     */
    public static BertQuestionAnswerer createAlbertQuestionAnswererFromFile(Context context,
            String pathToModel, String pathToSentencePieceModel) throws IOException {
        return new BertQuestionAnswerer(TaskJniUtils.createHandleWithMultipleAssetFilesFromLibrary(
                context, new MultipleBuffersHandleProvider() {
                    @Override
                    public long createHandle(ByteBuffer... buffers) {
                        return BertQuestionAnswerer.initJniWithAlbertByteBuffers(buffers);
                    }
                }, BERT_QUESTION_ANSWERER_NATIVE_LIBNAME, pathToModel, pathToSentencePieceModel));
    }

    @Override
    public List<QaAnswer> answer(String context, String question) {
        checkNotClosed();
        return answerNative(getNativeHandle(), context, question);
    }

    // modelBuffers[0] is tflite model file buffer, and modelBuffers[1] is vocab file buffer.
    private static native long initJniWithBertByteBuffers(ByteBuffer... modelBuffers);

    // modelBuffers[0] is tflite model file buffer, and modelBuffers[1] is sentencepiece model file
    // buffer.
    private static native long initJniWithAlbertByteBuffers(ByteBuffer... modelBuffers);

    // modelBuffers[0] is tflite model file buffer with metadata to specify which tokenizer to use.
    private static native long initJniWithModelWithMetadataByteBuffers(ByteBuffer... modelBuffers);

    private static native long initJniWithFileDescriptor(int fd);

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
