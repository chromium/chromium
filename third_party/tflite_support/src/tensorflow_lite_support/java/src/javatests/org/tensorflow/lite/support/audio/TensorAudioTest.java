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

package org.tensorflow.lite.support.audio;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.media.AudioFormat;
import android.media.AudioRecord;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Suite;
import org.junit.runners.Suite.SuiteClasses;
import org.robolectric.RobolectricTestRunner;
import org.tensorflow.lite.support.audio.TensorAudio.TensorAudioFormat;

/** Test for {@link TensorAudio}. */
@RunWith(Suite.class)
@SuiteClasses({
  TensorAudioTest.General.class,
})
public class TensorAudioTest {

  /** General tests of TensorAudio. */
  @RunWith(RobolectricTestRunner.class)
  public static final class General extends TensorAudioTest {
    @Test
    public void createSucceedsWithTensorAudioFormat() throws Exception {
      TensorAudio tensor =
          TensorAudio.create(
              TensorAudioFormat.builder().setChannels(1).setSampleRate(2).build(), 100);
      assertThat(tensor.getFormat().getChannels()).isEqualTo(1);
      assertThat(tensor.getFormat().getSampleRate()).isEqualTo(2);
      assertThat(tensor.getTensorBuffer().getFlatSize()).isEqualTo(100);
    }

    @Test
    public void createSucceedsWithTensorAudioFormatWithMultipleChannels() throws Exception {
      TensorAudio tensor =
          TensorAudio.create(
              TensorAudioFormat.builder().setChannels(5).setSampleRate(2).build(), 100);
      assertThat(tensor.getFormat().getChannels()).isEqualTo(5);
      assertThat(tensor.getFormat().getSampleRate()).isEqualTo(2);
      assertThat(tensor.getTensorBuffer().getFlatSize()).isEqualTo(500);
    }

    @Test
    public void createSucceededsWithDefaultArguments() throws Exception {
      TensorAudio tensor =
          TensorAudio.create(TensorAudioFormat.builder().setSampleRate(20).build(), 1000);
      // Number of channels defaults to 1.
      assertThat(tensor.getFormat().getChannels()).isEqualTo(1);
      assertThat(tensor.getFormat().getSampleRate()).isEqualTo(20);
      assertThat(tensor.getTensorBuffer().getFlatSize()).isEqualTo(1000);
    }

    @Test
    public void createSucceedsWithAudioFormat() throws Exception {
      AudioFormat format =
          new AudioFormat.Builder()
              .setChannelMask(AudioFormat.CHANNEL_IN_STEREO)
              .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
              .setSampleRate(16000)
              .build();
      TensorAudio tensor = TensorAudio.create(format, 100);
      // STEREO has 2 channels
      assertThat(tensor.getFormat().getChannels()).isEqualTo(2);
      assertThat(tensor.getFormat().getSampleRate()).isEqualTo(16000);
      // flatSize = channelCount * sampleCount
      assertThat(tensor.getTensorBuffer().getFlatSize()).isEqualTo(200);
    }

    @Test
    public void createFailedWithInvalidSampleRate() throws Exception {
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () -> TensorAudio.create(TensorAudioFormat.builder().setSampleRate(0).build(), 100));
      // Sample rate 0 is not allowed
      assertThat(exception).hasMessageThat().ignoringCase().contains("sample rate");
    }

    @Test
    public void createFailedWithInvalidChannels() throws Exception {
      IllegalArgumentException exception =
          assertThrows(
              IllegalArgumentException.class,
              () ->
                  TensorAudio.create(
                      TensorAudioFormat.builder().setSampleRate(1).setChannels(-1).build(), 100));
      // Negative channels is not allowed
      assertThat(exception).hasMessageThat().ignoringCase().contains("channels");
    }

    @Test
    public void loadSucceedsFromArray() throws Exception {
      TensorAudioFormat format =
          TensorAudioFormat.builder().setChannels(2).setSampleRate(2).build();
      TensorAudio tensor = TensorAudio.create(format, 2);
      assertThat(tensor.getTensorBuffer().getFloatArray()).isEqualTo(new float[4]);

      tensor.load(new float[] {2.f, 0});
      assertThat(tensor.getTensorBuffer().getFloatArray())
          .usingTolerance(0.001f)
          .containsExactly(new float[] {0, 0, 2.f, 0});

      tensor.load(new float[] {2.f, 3.f}, 0, 2);
      assertThat(tensor.getTensorBuffer().getFloatArray())
          .usingTolerance(0.001f)
          .containsExactly(new float[] {2.f, 0, 2.f, 3.f});

      tensor.load(new float[] {2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f}, 1, 6);
      // The sequence is longer than the ring buffer size so it's expected to keep only the last 4
      // numbers (index 3 to 6) of the load target sub-sequence (index 1 to 6).
      assertThat(tensor.getTensorBuffer().getFloatArray())
          .usingTolerance(0.001f)
          .containsExactly(new float[] {5.f, 6.f, 7.f, 8.f});

      tensor.load(new short[] {Short.MAX_VALUE, Short.MIN_VALUE});
      assertThat(tensor.getTensorBuffer().getFloatArray())
          .usingTolerance(0.001f)
          .containsExactly(new float[] {7.f, 8.f, 1.f, -1.f});

      tensor.load(new short[] {1000, 2000, 3000, 0, 1000, Short.MIN_VALUE, 4000, 5000, 6000}, 3, 6);
      // The sequence is longer than the ring buffer size so it's expected to keep only the last 4
      // numbers.
      assertThat(tensor.getTensorBuffer().getFloatArray())
          .usingTolerance(0.001f)
          .containsExactly(
              new float[] {
                -1.f, 4000.f / Short.MAX_VALUE, 5000.f / Short.MAX_VALUE, 6000.f / Short.MAX_VALUE
              });
    }

    @Test
    public void loadFailsWithIndexOutOfRange() throws Exception {
      TensorAudioFormat format = TensorAudioFormat.builder().setSampleRate(2).build();
      TensorAudio tensor = TensorAudio.create(format, 5);

      assertThrows(IllegalArgumentException.class, () -> tensor.load(new short[100], 99, 2));

      assertThrows(IllegalArgumentException.class, () -> tensor.load(new float[100], 99, 2));
    }

    @Test
    public void loadFailsWithIncompatibleInputSize() throws Exception {
      TensorAudioFormat format =
          TensorAudioFormat.builder().setChannels(3).setSampleRate(2).build();
      TensorAudio tensor = TensorAudio.create(format, 5);

      assertThrows(IllegalArgumentException.class, () -> tensor.load(new float[1]));

      assertThrows(IllegalArgumentException.class, () -> tensor.load(new short[2]));

      assertThrows(IllegalArgumentException.class, () -> tensor.load(new float[2], 1, 1));

      assertThrows(IllegalArgumentException.class, () -> tensor.load(new short[5], 2, 4));
    }

    @Test
    public void loadAudioRecordSucceeds() throws Exception {
      TensorAudio tensor =
          TensorAudio.create(TensorAudioFormat.builder().setSampleRate(16000).build(), 4);
      tensor.load(new float[] {1, 2, 3, 4, 5});
      assertThat(tensor.getTensorBuffer().getFloatArray())
          .isEqualTo(new float[] {2.f, 3.f, 4.f, 5.f});

      AudioRecord record = mock(AudioRecord.class);
      when(record.getBufferSizeInFrames()).thenReturn(5);
      when(record.getChannelCount()).thenReturn(1);
      when(record.getAudioFormat()).thenReturn(AudioFormat.ENCODING_PCM_FLOAT);
      when(record.getFormat())
          .thenReturn(
              new AudioFormat.Builder()
                  .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                  .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                  .setSampleRate(16000)
                  .build());
      // Unused
      when(record.read(any(short[].class), anyInt(), anyInt(), eq(AudioRecord.READ_NON_BLOCKING)))
          .thenReturn(AudioRecord.ERROR_INVALID_OPERATION);
      // Used
      when(record.read(any(float[].class), anyInt(), anyInt(), eq(AudioRecord.READ_NON_BLOCKING)))
          .thenReturn(1);
      assertThat(tensor.load(record)).isEqualTo(1);
      assertThat(tensor.getTensorBuffer().getFloatArray())
          .isEqualTo(new float[] {3.f, 4.f, 5.f, 0});

      record = mock(AudioRecord.class);
      when(record.getBufferSizeInFrames()).thenReturn(5);
      when(record.getChannelCount()).thenReturn(1);
      when(record.getAudioFormat()).thenReturn(AudioFormat.ENCODING_PCM_16BIT);
      when(record.getFormat())
          .thenReturn(
              new AudioFormat.Builder()
                  .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                  .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                  .setSampleRate(16000)
                  .build());
      // Used
      when(record.read(any(short[].class), anyInt(), anyInt(), eq(AudioRecord.READ_NON_BLOCKING)))
          .thenReturn(2);
      // Unused
      when(record.read(any(float[].class), anyInt(), anyInt(), eq(AudioRecord.READ_NON_BLOCKING)))
          .thenReturn(AudioRecord.ERROR_INVALID_OPERATION);
      assertThat(tensor.load(record)).isEqualTo(2);
      assertThat(tensor.getTensorBuffer().getFloatArray()).isEqualTo(new float[] {5.f, 0, 0, 0});
    }

    @Test
    public void loadAudioRecordFailsWithErrorState() throws Exception {
      TensorAudio tensor =
          TensorAudio.create(TensorAudioFormat.builder().setSampleRate(16000).build(), 4);
      tensor.load(new float[] {1, 2, 3, 4, 5});
      assertThat(tensor.getTensorBuffer().getFloatArray())
          .isEqualTo(new float[] {2.f, 3.f, 4.f, 5.f});

      AudioRecord record = mock(AudioRecord.class);
      when(record.getAudioFormat()).thenReturn(AudioFormat.ENCODING_PCM_FLOAT);
      when(record.getFormat())
          .thenReturn(
              new AudioFormat.Builder()
                  .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                  .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                  .setSampleRate(16000)
                  .build());
      // Unused
      when(record.read(any(short[].class), anyInt(), anyInt(), eq(AudioRecord.READ_NON_BLOCKING)))
          .thenReturn(AudioRecord.ERROR_INVALID_OPERATION);
      // Used
      when(record.read(any(float[].class), anyInt(), anyInt(), eq(AudioRecord.READ_NON_BLOCKING)))
          .thenReturn(AudioRecord.ERROR_DEAD_OBJECT);
      IllegalStateException exception =
          assertThrows(IllegalStateException.class, () -> tensor.load(record));
      assertThat(exception).hasMessageThat().contains("ERROR_DEAD_OBJECT");
    }

    @Test
    public void loadAudioRecordFailsWithUnsupportedAudioEncoding() throws Exception {
      TensorAudio tensor =
          TensorAudio.create(TensorAudioFormat.builder().setSampleRate(16000).build(), 4);
      AudioRecord record = mock(AudioRecord.class);
      when(record.getFormat())
          .thenReturn(
              new AudioFormat.Builder()
                  .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                  .setEncoding(AudioFormat.ENCODING_PCM_8BIT) // Not supported
                  .setSampleRate(16000)
                  .build());
      when(record.getAudioFormat()).thenReturn(AudioFormat.ENCODING_PCM_8BIT);

      IllegalArgumentException exception =
          assertThrows(IllegalArgumentException.class, () -> tensor.load(record));
      assertThat(exception).hasMessageThat().ignoringCase().contains("unsupported encoding");
    }

    @Test
    public void loadAudioRecordFailsWithIncompatibleAudioFormat() throws Exception {
      TensorAudio tensor =
          TensorAudio.create(TensorAudioFormat.builder().setSampleRate(16000).build(), 4);
      AudioRecord record = mock(AudioRecord.class);
      when(record.getFormat())
          .thenReturn(
              new AudioFormat.Builder()
                  .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                  .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                  .setSampleRate(44100) // Mismatch
                  .build());

      IllegalArgumentException exception =
          assertThrows(IllegalArgumentException.class, () -> tensor.load(record));
      assertThat(exception).hasMessageThat().ignoringCase().contains("Incompatible audio format");
    }
  }
}
