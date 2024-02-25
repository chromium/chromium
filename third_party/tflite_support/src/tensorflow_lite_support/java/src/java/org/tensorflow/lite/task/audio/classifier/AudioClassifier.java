/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite.task.audio.classifier;

import static org.tensorflow.lite.support.common.internal.SupportPreconditions.checkState;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.ParcelFileDescriptor;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.MappedByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.tensorflow.lite.DataType;
import org.tensorflow.lite.support.audio.TensorAudio;
import org.tensorflow.lite.support.audio.TensorAudio.TensorAudioFormat;
import org.tensorflow.lite.support.tensorbuffer.TensorBuffer;
import org.tensorflow.lite.task.core.BaseOptions;
import org.tensorflow.lite.task.core.BaseTaskApi;
import org.tensorflow.lite.task.core.TaskJniUtils;
import org.tensorflow.lite.task.core.TaskJniUtils.EmptyHandleProvider;
import org.tensorflow.lite.task.core.TaskJniUtils.FdAndOptionsHandleProvider;
import org.tensorflow.lite.task.core.annotations.UsedByReflection;

/**
 * Performs classification on audio waveforms.
 *
 * <p>The API expects a TFLite model with <a
 * href="https://www.tensorflow.org/lite/convert/metadata">TFLite Model Metadata.</a>.
 *
 * <p>The API supports models with one audio input tensor and one classification output tensor. To
 * be more specific, here are the requirements.
 *
 * <ul>
 *   <li>Input audio tensor ({@code kTfLiteFloat32})
 *       <ul>
 *         <li>input audio buffer of size {@code [batch x samples]}.
 *         <li>batch inference is not supported ({@code batch} is required to be 1).
 *       </ul>
 *   <li>Output score tensor ({@code kTfLiteFloat32})
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
 * See <a href="https://tfhub.dev/google/lite-model/yamnet/classification/tflite/1">an example</a>
 * of such model, and <a
 * href="https://github.com/tensorflow/tflite-support/tree/master/tensorflow_lite_support/examples/task/audio/desktop">a
 * CLI demo tool</a> for easily trying out this API.
 */
public final class AudioClassifier extends BaseTaskApi {

  private static final String AUDIO_CLASSIFIER_NATIVE_LIB = "task_audio_jni";
  private static final int OPTIONAL_FD_LENGTH = -1;
  private static final int OPTIONAL_FD_OFFSET = -1;

  /**
   * Creates an {@link AudioClassifier} instance from the default {@link AudioClassifierOptions}.
   *
   * @param modelPath path of the classification model with metadata in the assets
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static AudioClassifier createFromFile(Context context, String modelPath)
      throws IOException {
    return createFromFileAndOptions(context, modelPath, AudioClassifierOptions.builder().build());
  }

  /**
   * Creates an {@link AudioClassifier} instance from the default {@link AudioClassifierOptions}.
   *
   * @param modelFile the classification model {@link File} instance
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static AudioClassifier createFromFile(File modelFile) throws IOException {
    return createFromFileAndOptions(modelFile, AudioClassifierOptions.builder().build());
  }

  /**
   * Creates an {@link AudioClassifier} instance with a model buffer and the default {@link
   * AudioClassifierOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the
   *     classification model
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   */
  public static AudioClassifier createFromBuffer(final ByteBuffer modelBuffer) {
    return createFromBufferAndOptions(modelBuffer, AudioClassifierOptions.builder().build());
  }

  /**
   * Creates an {@link AudioClassifier} instance from {@link AudioClassifierOptions}.
   *
   * @param modelPath path of the classification model with metadata in the assets
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static AudioClassifier createFromFileAndOptions(
      Context context, String modelPath, AudioClassifierOptions options) throws IOException {
    return new AudioClassifier(
        TaskJniUtils.createHandleFromFdAndOptions(
            context,
            new FdAndOptionsHandleProvider<AudioClassifierOptions>() {
              @Override
              public long createHandle(
                  int fileDescriptor,
                  long fileDescriptorLength,
                  long fileDescriptorOffset,
                  AudioClassifierOptions options) {
                return initJniWithModelFdAndOptions(
                    fileDescriptor,
                    fileDescriptorLength,
                    fileDescriptorOffset,
                    options,
                    TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions()));
              }
            },
            AUDIO_CLASSIFIER_NATIVE_LIB,
            modelPath,
            options));
  }

  /**
   * Creates an {@link AudioClassifier} instance.
   *
   * @param modelFile the classification model {@link File} instance
   * @throws IOException if an I/O error occurs when loading the tflite model
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   */
  public static AudioClassifier createFromFileAndOptions(
      File modelFile, final AudioClassifierOptions options) throws IOException {
    try (ParcelFileDescriptor descriptor =
        ParcelFileDescriptor.open(modelFile, ParcelFileDescriptor.MODE_READ_ONLY)) {
      return new AudioClassifier(
          TaskJniUtils.createHandleFromLibrary(
              new TaskJniUtils.EmptyHandleProvider() {
                @Override
                public long createHandle() {
                  return initJniWithModelFdAndOptions(
                      descriptor.getFd(),
                      /*fileDescriptorLength=*/ OPTIONAL_FD_LENGTH,
                      /*fileDescriptorOffset=*/ OPTIONAL_FD_OFFSET,
                      options,
                      TaskJniUtils.createProtoBaseOptionsHandle(options.getBaseOptions()));
                }
              },
              AUDIO_CLASSIFIER_NATIVE_LIB));
    }
  }

  /**
   * Creates an {@link AudioClassifier} instance with a model buffer and {@link
   * AudioClassifierOptions}.
   *
   * @param modelBuffer a direct {@link ByteBuffer} or a {@link MappedByteBuffer} of the
   *     classification model
   * @throws IllegalStateException if there is an internal error
   * @throws RuntimeException if there is an otherwise unspecified error
   * @throws IllegalArgumentException if the model buffer is not a direct {@link ByteBuffer} or a
   *     {@link MappedByteBuffer}
   */
  public static AudioClassifier createFromBufferAndOptions(
      final ByteBuffer modelBuffer, final AudioClassifierOptions options) {
    if (!(modelBuffer.isDirect() || modelBuffer instanceof MappedByteBuffer)) {
      throw new IllegalArgumentException(
          "The model buffer should be either a direct ByteBuffer or a MappedByteBuffer.");
    }
    return new AudioClassifier(
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
            AUDIO_CLASSIFIER_NATIVE_LIB));
  }

  /**
   * Constructor to initialize the JNI with a pointer from C++.
   *
   * @param nativeHandle a pointer referencing memory allocated in C++
   */
  private AudioClassifier(long nativeHandle) {
    super(nativeHandle);
  }

  /** Options for setting up an {@link AudioClassifier}. */
  @UsedByReflection("audio_classifier_jni.cc")
  public static class AudioClassifierOptions {
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

    public static Builder builder() {
      return new Builder();
    }

    /** A builder that helps to configure an instance of AudioClassifierOptions. */
    public static class Builder {
      private BaseOptions baseOptions = BaseOptions.builder().build();
      private String displayNamesLocale = "en";
      private int maxResults = -1;
      private float scoreThreshold;
      private boolean isScoreThresholdSet;
      private List<String> labelAllowList = new ArrayList<>();
      private List<String> labelDenyList = new ArrayList<>();

      private Builder() {}

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
       * @param maxResults if < 0, all results will be returned. If 0, an invalid argument error is
       *     returned. Defaults to -1.
       * @throws IllegalArgumentException if maxResults is 0
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

      public AudioClassifierOptions build() {
        return new AudioClassifierOptions(this);
      }
    }

    @UsedByReflection("audio_classifier_jni.cc")
    public String getDisplayNamesLocale() {
      return displayNamesLocale;
    }

    @UsedByReflection("audio_classifier_jni.cc")
    public int getMaxResults() {
      return maxResults;
    }

    @UsedByReflection("audio_classifier_jni.cc")
    public float getScoreThreshold() {
      return scoreThreshold;
    }

    @UsedByReflection("audio_classifier_jni.cc")
    public boolean getIsScoreThresholdSet() {
      return isScoreThresholdSet;
    }

    @UsedByReflection("audio_classifier_jni.cc")
    public List<String> getLabelAllowList() {
      return new ArrayList<>(labelAllowList);
    }

    @UsedByReflection("audio_classifier_jni.cc")
    public List<String> getLabelDenyList() {
      return new ArrayList<>(labelDenyList);
    }

    public BaseOptions getBaseOptions() {
      return baseOptions;
    }

    private AudioClassifierOptions(Builder builder) {
      displayNamesLocale = builder.displayNamesLocale;
      maxResults = builder.maxResults;
      scoreThreshold = builder.scoreThreshold;
      isScoreThresholdSet = builder.isScoreThresholdSet;
      labelAllowList = builder.labelAllowList;
      labelDenyList = builder.labelDenyList;
      baseOptions = builder.baseOptions;
    }
  }

  /**
   * Performs actual classification on the provided audio tensor.
   *
   * @param tensor a {@link TensorAudio} containing the input audio clip in float with values
   *     between [-1, 1). The {@code tensor} argument should have the same flat size as the TFLite
   *     model's input tensor. It's recommended to create {@code tensor} using {@code
   *     createInputTensorAudio} method.
   * @throws IllegalArgumentException if an argument is invalid
   * @throws IllegalStateException if error occurs when classifying the audio clip from the native
   *     code
   */
  public List<Classifications> classify(TensorAudio tensor) {
    TensorBuffer buffer = tensor.getTensorBuffer();
    TensorAudioFormat format = tensor.getFormat();
    checkState(
        buffer.getBuffer().hasArray(),
        "Input tensor buffer should be a non-direct buffer with a backed array (i.e. not readonly"
            + " buffer).");
    return classifyNative(
        getNativeHandle(),
        buffer.getBuffer().array(),
        format.getChannels(),
        format.getSampleRate());
  }

  /**
   * Creates a {@link TensorAudio} instance to store input audio samples.
   *
   * @return a {@link TensorAudio} with the same size as model input tensor
   * @throws IllegalArgumentException if the model is not compatible
   */
  public TensorAudio createInputTensorAudio() {
    TensorAudioFormat format = getRequiredTensorAudioFormat();

    long bufferSize = getRequiredInputBufferSize();
    long samples = bufferSize / format.getChannels();
    return TensorAudio.create(format, (int) samples);
  }

  /** Returns the required input buffer size in number of float elements. */
  public long getRequiredInputBufferSize() {
    return getRequiredInputBufferSizeNative(getNativeHandle());
  }

  /**
   * Creates an {@link android.media.AudioRecord} instance to record audio stream. The returned
   * AudioRecord instance is initialized and client needs to call {@link
   * android.media.AudioRecord#startRecording} method to start recording.
   *
   * @return an {@link android.media.AudioRecord} instance in {@link
   *     android.media.AudioRecord#STATE_INITIALIZED}
   * @throws IllegalArgumentException if the model required channel count is unsupported
   * @throws IllegalStateException if AudioRecord instance failed to initialize
   */
  public AudioRecord createAudioRecord() {
    TensorAudioFormat format = getRequiredTensorAudioFormat();
    int channelConfig = 0;

    switch (format.getChannels()) {
      case 1:
        channelConfig = AudioFormat.CHANNEL_IN_MONO;
        break;
      case 2:
        channelConfig = AudioFormat.CHANNEL_IN_STEREO;
        break;
      default:
        throw new IllegalArgumentException(
            String.format(
                "Number of channels required by the model is %d. getAudioRecord method only"
                    + " supports 1 or 2 audio channels.",
                format.getChannels()));
    }

    int bufferSizeInBytes =
        AudioRecord.getMinBufferSize(
            format.getSampleRate(), channelConfig, AudioFormat.ENCODING_PCM_FLOAT);
    if (bufferSizeInBytes == AudioRecord.ERROR
        || bufferSizeInBytes == AudioRecord.ERROR_BAD_VALUE) {
      throw new IllegalStateException(
          String.format("AudioRecord.getMinBufferSize failed. Returned: %d", bufferSizeInBytes));
    }
    // The buffer of AudioRecord should be strictly longer than what model requires so that clients
    // could run `TensorAudio::load(record)` together with `AudioClassifier::classify`.
    int bufferSizeMultiplier = 2;
    int modelRequiredBufferSize =
        (int) getRequiredInputBufferSize() * DataType.FLOAT32.byteSize() * bufferSizeMultiplier;
    if (bufferSizeInBytes < modelRequiredBufferSize) {
      bufferSizeInBytes = modelRequiredBufferSize;
    }
    AudioRecord audioRecord =
        new AudioRecord(
            // including MIC, UNPROCESSED, and CAMCORDER.
            MediaRecorder.AudioSource.VOICE_RECOGNITION,
            format.getSampleRate(),
            channelConfig,
            AudioFormat.ENCODING_PCM_FLOAT,
            bufferSizeInBytes);
    checkState(
        audioRecord.getState() == AudioRecord.STATE_INITIALIZED,
        "AudioRecord failed to initialize");
    return audioRecord;
  }

  /** Returns the {@link TensorAudioFormat} required by the model. */
  public TensorAudioFormat getRequiredTensorAudioFormat() {
    return TensorAudioFormat.builder()
        .setChannels(getRequiredChannels())
        .setSampleRate(getRequiredSampleRate())
        .build();
  }

  private int getRequiredChannels() {
    return getRequiredChannelsNative(getNativeHandle());
  }

  private int getRequiredSampleRate() {
    return getRequiredSampleRateNative(getNativeHandle());
  }

  // TODO(b/183343074): JNI method invocation is very expensive, taking about .2ms
  // each time. Consider combining the native getter methods into 1 and cache it in Java layer.
  private static native long getRequiredInputBufferSizeNative(long nativeHandle);

  private static native int getRequiredChannelsNative(long nativeHandle);

  private static native int getRequiredSampleRateNative(long nativeHandle);

  private static native List<Classifications> classifyNative(
      long nativeHandle, byte[] audioBuffer, int channels, int sampleRate);

  private static native long initJniWithModelFdAndOptions(
      int fileDescriptor,
      long fileDescriptorLength,
      long fileDescriptorOffset,
      AudioClassifierOptions options,
      long baseOptionsHandle);

  private static native long initJniWithByteBuffer(
      ByteBuffer modelBuffer, AudioClassifierOptions options, long baseOptionsHandle);

  /**
   * Releases memory pointed by {@code nativeHandle}, namely a C++ `AudioClassifier` instance.
   *
   * @param nativeHandle pointer to memory allocated
   */
  @Override
  protected void deinit(long nativeHandle) {
    deinitJni(nativeHandle);
  }

  /**
   * Native method to release memory pointed by {@code nativeHandle}, namely a C++ `AudioClassifier`
   * instance.
   *
   * @param nativeHandle pointer to memory allocated
   */
  private static native void deinitJni(long nativeHandle);
}
