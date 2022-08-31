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
"""Bert NL Classifier task."""

import dataclasses

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import classifications_pb2
from tensorflow_lite_support.python.task.text.pybinds import _pywrap_bert_nl_classifier

_CppBertNLClassifier = _pywrap_bert_nl_classifier.BertNLClassifier
_BaseOptions = base_options_module.BaseOptions


@dataclasses.dataclass
class BertNLClassifierOptions:
  """Options for the Bert NL classifier task.

  Attributes:
    base_options: Base options for the Bert NL classifier task.
  """
  base_options: _BaseOptions


class BertNLClassifier(object):
  """Class that performs Bert NL classification on text."""

  def __init__(self, options: BertNLClassifierOptions,
               cpp_classifier: _CppBertNLClassifier) -> None:
    """Initializes the `BertNLClassifier` object."""
    # Creates the object of C++ BertNLClassifier class.
    self._options = options
    self._classifier = cpp_classifier

  @classmethod
  def create_from_file(cls, file_path: str) -> "BertNLClassifier":
    """Creates the `BertNLClassifier` object from a TensorFlow Lite model.

    Args:
      file_path: Path to the model.

    Returns:
      `BertNLClassifier` object that's created from the model file.
    Raises:
      ValueError: If failed to create `BertNLClassifier` object from the
        provided file such as invalid file.
      RuntimeError: If other types of error occurred.
    """
    base_options = _BaseOptions(file_name=file_path)
    options = BertNLClassifierOptions(base_options=base_options)
    return cls.create_from_options(options)

  @classmethod
  def create_from_options(
      cls, options: BertNLClassifierOptions) -> "BertNLClassifier":
    """Creates the `BertNLClassifier` object from Bert NL classifier options.

    Args:
      options: Options for the Bert NL classifier task.

    Returns:
      `BertNLClassifier` object that's created from `options`.
    Raises:
      ValueError: If failed to create `BertNLClassifier` object from
        `BertNLClassifierOptions` such as missing the model.
      RuntimeError: If other types of error occurred.
    """
    classifier = _CppBertNLClassifier.create_from_options(
        options.base_options.to_pb2())
    return cls(options, classifier)

  def classify(self, text: str) -> classifications_pb2.ClassificationResult:
    """Performs actual Bert NL classification on the provided text.

    Args:
      text: the input text, used to extract the feature vectors.

    Returns:
      classification result.

    Raises:
      ValueError: If any of the input arguments is invalid.
      RuntimeError: If failed to calculate the embedding vector.
    """
    classification_result = self._classifier.classify(text)
    return classifications_pb2.ClassificationResult.create_from_pb2(
        classification_result)

  @property
  def options(self) -> BertNLClassifierOptions:
    return self._options
