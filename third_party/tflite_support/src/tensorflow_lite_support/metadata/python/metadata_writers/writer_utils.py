# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
# ==============================================================================
"""Helper methods for writing metadata into TFLite models."""

import array
import functools
from typing import List, Union, Optional

from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata import schema_py_generated as _schema_fb


def compute_flat_size(tensor_shape: Optional["array.array[int]"]) -> int:
  """Computes the flat size (number of elements) of tensor shape.

  Args:
    tensor_shape: an array of the tensor shape values.

  Returns:
    The flat size of the tensor shape. Return 0 if tensor_shape is None.
  """
  if not tensor_shape:
    return 0
  return functools.reduce(lambda x, y: x * y, tensor_shape)


def get_input_tensor_names(model_buffer: bytearray) -> List[str]:
  """Gets a list of the input tensor names."""
  subgraph = _get_subgraph(model_buffer)
  tensor_names = []
  for i in range(subgraph.InputsLength()):
    index = subgraph.Inputs(i)
    tensor_names.append(subgraph.Tensors(index).Name().decode("utf-8"))
  return tensor_names


def get_output_tensor_names(model_buffer: bytearray) -> List[str]:
  """Gets a list of the output tensor names."""
  subgraph = _get_subgraph(model_buffer)
  tensor_names = []
  for i in range(subgraph.OutputsLength()):
    index = subgraph.Outputs(i)
    tensor_names.append(subgraph.Tensors(index).Name().decode("utf-8"))
  return tensor_names


def get_input_tensor_types(
    model_buffer: bytearray) -> List[_schema_fb.TensorType]:
  """Gets a list of the input tensor types."""
  subgraph = _get_subgraph(model_buffer)
  tensor_types = []
  for i in range(subgraph.InputsLength()):
    index = subgraph.Inputs(i)
    tensor_types.append(subgraph.Tensors(index).Type())
  return tensor_types


def get_output_tensor_types(
    model_buffer: bytearray) -> List[_schema_fb.TensorType]:
  """Gets a list of the output tensor types."""
  subgraph = _get_subgraph(model_buffer)
  tensor_types = []
  for i in range(subgraph.OutputsLength()):
    index = subgraph.Outputs(i)
    tensor_types.append(subgraph.Tensors(index).Type())
  return tensor_types


def get_input_tensor_shape(model_buffer: bytearray,
                           tensor_index: int) -> array.array:
  """Gets the shape of the specified input tensor."""
  subgraph = _get_subgraph(model_buffer)
  return subgraph.Tensors(subgraph.Inputs(tensor_index)).ShapeAsNumpy()


def load_file(file_path: str, mode: str = "rb") -> Union[str, bytes]:
  """Loads file from the file path.

  Args:
    file_path: valid file path string.
    mode: a string specifies the model in which the file is opened. Use "rt" for
      reading in text mode; use "rb" for reading in binary mode.

  Returns:
    The loaded file in str or bytes.
  """
  with open(file_path, mode) as file:
    return file.read()


def save_file(file_bytes: Union[bytes, bytearray],
              save_to_path: str,
              mode: str = "wb"):
  """Loads file from the file path.

  Args:
    file_bytes: the bytes to be saved to file.
    save_to_path: valid file path string.
    mode: a string specifies the model in which the file is opened. Use "wt" for
      writing in text mode; use "wb" for writing in binary mode.

  Returns:
    The loaded file in str or bytes.
  """
  with open(save_to_path, mode) as file:
    file.write(file_bytes)


def get_tokenizer_associated_files(
    tokenizer_options: Union[None, _metadata_fb.BertTokenizerOptionsT,
                             _metadata_fb.SentencePieceTokenizerOptionsT,
                             _metadata_fb.RegexTokenizerOptionsT]
) -> List[Optional[str]]:
  """Gets a list of associated files packed in the tokenzier_options.

  Args:
    tokenizer_options: a tokenizer metadata object. Support the following
      tokenizer types:
      1. BertTokenizerOptions:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L436
      2. SentencePieceTokenizerOptions:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L473
      3. RegexTokenizerOptions:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L475

  Returns:
    A list of associated files included in tokenizer_options.
  """

  if not tokenizer_options:
    return []

  def _get_file_path(files: _metadata_fb.AssociatedFileT) -> List[str]:
    if not files:
      return []
    return [file.name for file in files]

  if isinstance(tokenizer_options, (_metadata_fb.BertTokenizerOptionsT,
                                    _metadata_fb.RegexTokenizerOptionsT)):
    return _get_file_path(tokenizer_options.vocabFile)
  elif isinstance(tokenizer_options,
                  _metadata_fb.SentencePieceTokenizerOptionsT):
    return _get_file_path(tokenizer_options.vocabFile) + _get_file_path(
        tokenizer_options.sentencePieceModel)
  else:
    return []


def _get_subgraph(model_buffer: bytearray) -> _schema_fb.SubGraph:
  """Gets the subgraph of the model.

  TFLite does not support multi-subgraph. A model should have exactly one
  subgraph.

  Args:
    model_buffer: valid buffer of the model file.

  Returns:
    The subgraph of the model.

  Raises:
    ValueError: if the model has more than one subgraph or has no subgraph.
  """

  model = _schema_fb.Model.GetRootAsModel(model_buffer, 0)

  # Use the first subgraph as default. TFLite Interpreter doesn't support
  # multiple subgraphs yet, but models with mini-benchmark may have multiple
  # subgraphs for acceleration evaluation purpose.
  return model.Subgraphs(0)
