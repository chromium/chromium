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
"""Bert CLU Annotator task."""

import dataclasses

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import clu_annotation_options_pb2
from tensorflow_lite_support.python.task.processor.proto import clu_pb2
from tensorflow_lite_support.python.task.text.pybinds import _pywrap_bert_clu_annotator

_CppBertCluAnnotator = _pywrap_bert_clu_annotator.BertCluAnnotator
_BaseOptions = base_options_module.BaseOptions
_BertCluAnnotationOptions = clu_annotation_options_pb2.BertCluAnnotationOptions


@dataclasses.dataclass
class BertCluAnnotatorOptions:
  """Options for the Bert CLU Annotator task.

  Attributes:
    base_options: Base options for the Bert CLU Annotator task.
    bert_clu_annotation_options: Bert CLU Annotator options for the Bert CLU
      Annotator task.
  """
  base_options: _BaseOptions
  bert_clu_annotation_options: _BertCluAnnotationOptions = dataclasses.field(
      default_factory=_BertCluAnnotationOptions
  )


class BertCluAnnotator(object):
  """Class that performs Bert CLU Annotation on text."""

  def __init__(self, options: BertCluAnnotatorOptions,
               cpp_annotator: _CppBertCluAnnotator) -> None:
    """Initializes the `BertCluAnnotator` object."""
    # Creates the object of C++ BertCluAnnotator class.
    self._options = options
    self._annotator = cpp_annotator

  @classmethod
  def create_from_file(cls, file_path: str) -> "BertCluAnnotator":
    """Creates the `BertCluAnnotator` object from a TensorFlow Lite model.

    Args:
      file_path: Path to the model.

    Returns:
      `BertCLUAnnotator` object that's created from the model file.
    Raises:
      ValueError: If failed to create `BertCluAnnotator` object from the
        provided file such as invalid file.
      RuntimeError: If other types of error occurred.
    """
    base_options = _BaseOptions(file_name=file_path)
    options = BertCluAnnotatorOptions(base_options=base_options)
    return cls.create_from_options(options)

  @classmethod
  def create_from_options(
      cls, options: BertCluAnnotatorOptions) -> "BertCluAnnotator":
    """Creates the `BertCluAnnotator` object from Bert CLU Annotation options.

    Args:
      options: Options for the Bert CLU annotation task.

    Returns:
      `BertCluAnnotator` object that's created from `options`.
    Raises:
      ValueError: If failed to create `BertCluAnnotator` object from
        `BertCluAnnotatorOptions` such as missing the model.
      RuntimeError: If other types of error occurred.
    """
    classifier = _CppBertCluAnnotator.create_from_options(
        options.base_options.to_pb2(),
        options.bert_clu_annotation_options.to_pb2())
    return cls(options, classifier)

  def annotate(self, request: clu_pb2.CluRequest) -> clu_pb2.CluResponse:
    """Performs actual Bert CLU Annotation on the provided CLU request.

    Args:
      request: The input to CLU.

    Returns:
      The output of CLU.

    Raises:
      ValueError: If any of the input arguments is invalid.
      RuntimeError: If failed to calculate the embedding vector.
    """
    annotation_result = self._annotator.annotate(request.to_pb2())
    return clu_pb2.CluResponse.create_from_pb2(annotation_result)

  @property
  def options(self) -> BertCluAnnotatorOptions:
    return self._options
