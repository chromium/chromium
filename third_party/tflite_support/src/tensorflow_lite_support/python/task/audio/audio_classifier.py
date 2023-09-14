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
"""Audio classifier task."""

import dataclasses

from tensorflow_lite_support.python.task.audio.core import audio_record
from tensorflow_lite_support.python.task.audio.core import tensor_audio
from tensorflow_lite_support.python.task.audio.core.pybinds import _pywrap_audio_buffer
from tensorflow_lite_support.python.task.audio.pybinds import _pywrap_audio_classifier
from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import classification_options_pb2
from tensorflow_lite_support.python.task.processor.proto import classifications_pb2

_CppAudioFormat = _pywrap_audio_buffer.AudioFormat
_CppAudioBuffer = _pywrap_audio_buffer.AudioBuffer
_CppAudioClassifier = _pywrap_audio_classifier.AudioClassifier
_ClassificationOptions = classification_options_pb2.ClassificationOptions
_BaseOptions = base_options_module.BaseOptions


@dataclasses.dataclass
class AudioClassifierOptions:
  """Options for the audio classifier task.

  Attributes:
    base_options: Base options for the audio classifier task.
    classification_options: Classification options for the audio classifier
      task.
  """
  base_options: _BaseOptions
  classification_options: _ClassificationOptions = dataclasses.field(
      default_factory=_ClassificationOptions
  )


class AudioClassifier(object):
  """Class that performs classification on audio."""

  def __init__(self, options: AudioClassifierOptions,
               classifier: _CppAudioClassifier) -> None:
    """Initializes the `AudioClassifier` object."""
    # Creates the object of C++ AudioClassifier class.
    self._options = options
    self._classifier = classifier

  @classmethod
  def create_from_file(cls, file_path: str) -> "AudioClassifier":
    """Creates the `AudioClassifier` object from a TensorFlow Lite model.

    Args:
      file_path: Path to the model.

    Returns:
      `AudioClassifier` object that's created from `options`.

    Raises:
      ValueError: If failed to create `AudioClassifier` object from the provided
        file such as invalid file.
      RuntimeError: If other types of error occurred.
    """
    base_options = _BaseOptions(file_name=file_path)
    options = AudioClassifierOptions(base_options=base_options)
    return cls.create_from_options(options)

  @classmethod
  def create_from_options(cls,
                          options: AudioClassifierOptions) -> "AudioClassifier":
    """Creates the `AudioClassifier` object from audio classifier options.

    Args:
      options: Options for the audio classifier task.

    Returns:
      `AudioClassifier` object that's created from `options`.

    Raises:
      ValueError: If failed to create `AudioClassifier` object from
        `AudioClassifierOptions` such as missing the model.
      RuntimeError: If other types of error occurred.
    """
    classifier = _CppAudioClassifier.create_from_options(
        options.base_options.to_pb2(), options.classification_options.to_pb2())
    return cls(options, classifier)

  def create_input_tensor_audio(self) -> tensor_audio.TensorAudio:
    """Creates a TensorAudio instance to store the audio input.

    Returns:
      A TensorAudio instance.
    """
    return tensor_audio.TensorAudio(
        audio_format=self.required_audio_format,
        buffer_size=self.required_input_buffer_size)

  def create_audio_record(self) -> audio_record.AudioRecord:
    """Creates an AudioRecord instance to record audio.

    Returns:
      An AudioRecord instance.
    """
    return audio_record.AudioRecord(self.required_audio_format.channels,
                                    self.required_audio_format.sample_rate,
                                    self.required_input_buffer_size)

  def classify(
      self,
      audio: tensor_audio.TensorAudio,
  ) -> classifications_pb2.ClassificationResult:
    """Performs classification on the provided TensorAudio.

    Args:
      audio: Tensor audio, used to extract the feature vectors.

    Returns:
      classification result.

    Raises:
      ValueError: If any of the input arguments is invalid.
      RuntimeError: If failed to run audio classification.
    """
    classification_result = self._classifier.classify(
        _CppAudioBuffer(audio.buffer, audio.buffer_size, audio.format))
    return classifications_pb2.ClassificationResult.create_from_pb2(
        classification_result)

  @property
  def required_input_buffer_size(self) -> int:
    """Gets the required input buffer size for the model."""
    return self._classifier.get_required_input_buffer_size()

  @property
  def required_audio_format(self) -> _CppAudioFormat:
    """Gets the required audio format for the model.

    Raises:
      RuntimeError: If failed to get the required audio format.
    """
    return self._classifier.get_required_audio_format()
