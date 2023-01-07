# Copyright 2022 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""TensorAudio class."""

import numpy as np

from tensorflow_lite_support.python.task.audio.core import audio_record
from tensorflow_lite_support.python.task.audio.core.pybinds import _pywrap_audio_buffer

_LoadAudioBufferFromFile = _pywrap_audio_buffer.LoadAudioBufferFromFile
AudioFormat = _pywrap_audio_buffer.AudioFormat


class TensorAudio(object):
  """A wrapper class to store the input audio."""

  def __init__(self, audio_format: AudioFormat, buffer_size: int) -> None:
    """Initializes the `TensorAudio` object.

    Args:
      audio_format: format of the audio.
      buffer_size: buffer size of the audio.
    """
    self._format = audio_format
    self._buffer_size = buffer_size
    self._buffer = np.zeros([self._buffer_size, self._format.channels],
                            dtype=np.float32)

  def clear(self):
    """Clear the internal buffer and fill it with zeros."""
    self._buffer.fill(0)

  @classmethod
  def create_from_wav_file(cls,
                           file_name: str,
                           sample_count: int,
                           offset: int = 0) -> "TensorAudio":
    """Creates `TensorAudio` object from the WAV file.

    Args:
      file_name: WAV file name.
      sample_count: The number of samples to read from the WAV file. This value
        should match with the input size of the TensorFlow Lite audio model that
        will consume the created TensorAudio object. If the WAV file contains
        more samples than sample_count, only the samples at the beginning of the
        WAV file will be loaded.
      offset: An optional offset for allowing the user to skip a certain number
        samples at the beginning.

    Returns:
      `TensorAudio` object.

    Raises:
      ValueError: If an input parameter, such as the audio file, is invalid.
      RuntimeError: If other types of error occurred.
    """
    if offset < 0:
      raise ValueError("offset cannot be negative")

    audio = _LoadAudioBufferFromFile(file_name, sample_count, offset,
                                     np.zeros([sample_count]))
    tensor = TensorAudio(audio.audio_format, audio.buffer_size)
    tensor.load_from_array(np.array(audio.float_buffer, copy=False))
    return tensor

  def load_from_audio_record(self, record: audio_record.AudioRecord) -> None:
    """Loads audio data from an AudioRecord instance.

    Args:
      record: An AudioRecord instance.

    Raises:
      ValueError: Raised if the audio record's config is invalid.
      RuntimeError: Raised if other types of error occurred.
    """
    if record.buffer_size < self._buffer_size:
      raise ValueError(
          "The audio record's buffer size cannot be smaller than the tensor "
          "audio's sample count.")

    if record.channels != self._format.channels:
      raise ValueError(f"The audio record's channel count doesn't match. "
                       f"Expects {self._format.channels} channel(s).")

    if record.sampling_rate != self._format.sample_rate:
      raise ValueError(f"The audio record's sampling rate doesn't match. "
                       f"Expects {self._format.sample_rate}Hz.")

    # Load audio data from the AudioRecord instance.
    data = record.read(self._buffer_size)
    self.load_from_array(data.astype(np.float32))

  def load_from_array(self,
                      src: np.ndarray,
                      offset: int = 0,
                      size: int = -1) -> None:
    """Loads the audio data from a NumPy array.

    Args:
      src: A NumPy source array contains the input audio.
      offset: An optional offset for loading a slice of the `src` array to the
        buffer.
      size: An optional size parameter denoting the number of samples to load
        from the `src` array.

    Raises:
      ValueError: If the input array has an incorrect shape or if
        `offset` + `size` exceeds the length of the `src` array.
    """
    if src.shape[1] != self._format.channels:
      raise ValueError(f"Input audio contains an invalid number of channels. "
                       f"Expect {self._format.channels}.")

    if size < 0:
      size = len(src)

    if offset + size > len(src):
      raise ValueError(
          f"Index out of range. offset {offset} + size {size} should be <= "
          f"src's length: {len(src)}")

    if len(src) >= len(self._buffer):
      # If the internal buffer is shorter than the load target (src), copy
      # values from the end of the src array to the internal buffer.
      new_offset = offset + size - len(self._buffer)
      new_size = len(self._buffer)
      self._buffer = src[new_offset:new_offset + new_size].copy()
    else:
      # Shift the internal buffer backward and add the incoming data to the end
      # of the buffer.
      shift = size
      self._buffer = np.roll(self._buffer, -shift, axis=0)
      self._buffer[-shift:, :] = src[offset:offset + size].copy()

  @property
  def format(self) -> AudioFormat:
    """Gets the audio format of the audio."""
    return self._format

  @property
  def buffer_size(self) -> int:
    """Gets the sample count of the audio."""
    return self._buffer_size

  @property
  def buffer(self) -> np.ndarray:
    """Gets the internal buffer."""
    return self._buffer
