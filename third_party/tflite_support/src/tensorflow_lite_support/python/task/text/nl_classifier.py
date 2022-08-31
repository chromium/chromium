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
"""NL Classifier task."""

import dataclasses

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import classification_options_pb2
from tensorflow_lite_support.python.task.processor.proto import classifications_pb2
from tensorflow_lite_support.python.task.text.pybinds import _pywrap_nl_classifier

_CppNLClassifier = _pywrap_nl_classifier.NLClassifier
_BaseOptions = base_options_module.BaseOptions
_ClassificationOptions = classification_options_pb2.ClassificationOptions
_ClassificationResult = classifications_pb2.ClassificationResult


@dataclasses.dataclass
class NLClassifierOptions:
  """Options for the NL classifier task.

  Attributes:
    base_options: Base options for the NL classifier task.
  """
  base_options: _BaseOptions


class NLClassifier(object):
  """Class that performs NL classification on text."""

  def __init__(self, options: NLClassifierOptions,
               cpp_classifier: _CppNLClassifier) -> None:
    """Initializes the `NLClassifier` object."""
    # Creates the object of C++ NLClassifier class.
    self._options = options
    self._classifier = cpp_classifier

  @classmethod
  def create_from_file(cls, file_path: str) -> "NLClassifier":
    """Creates the `NLClassifier` object from a TensorFlow Lite model.

    Args:
      file_path: Path to the model.

    Returns:
      `NLClassifier` object that's created from the model file.
    Raises:
      ValueError: If failed to create `NLClassifier` object from the provided
        file such as invalid file.
      RuntimeError: If other types of error occurred.
    """
    base_options = _BaseOptions(file_name=file_path)
    options = NLClassifierOptions(base_options=base_options)
    return cls.create_from_options(options)

  @classmethod
  def create_from_options(cls, options: NLClassifierOptions) -> "NLClassifier":
    """Creates the `NLClassifier` object from NL classifier options.

    Args:
      options: Options for the NL classifier task.

    Returns:
      `NLClassifier` object that's created from `options`.
    Raises:
      ValueError: If failed to create `NLClassifier` object from
        `NLClassifierOptions` such as missing the model or if any of the
        classification options is invalid.
      RuntimeError: If other types of error occurred.
    """
    classifier = _CppNLClassifier.create_from_options(
        options.base_options.to_pb2())
    return cls(options, classifier)

  def classify(self, text: str) -> _ClassificationResult:
    """Performs actual NL classification on the provided text.

    Args:
      text: the input text, used to extract the feature vectors.

    Returns:
      The classification result.

    Raises:
      ValueError: If any of the input arguments is invalid.
      RuntimeError: If failed to perform the classification.
    """
    classification_result = self._classifier.classify(text)
    return _ClassificationResult.create_from_pb2(classification_result)

  @property
  def options(self) -> NLClassifierOptions:
    return self._options
