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
"""Helper class to write metadata into TFLite models."""

import collections
from typing import List, Optional, Type

import flatbuffers
from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata import schema_py_generated as _schema_fb
from tensorflow_lite_support.metadata.python import metadata as _metadata
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import writer_utils


class MetadataWriter:
  """Writes the metadata and associated files into a TFLite model."""

  def __init__(self,
               model_buffer: bytearray,
               metadata_buffer: Optional[bytearray] = None,
               associated_files: Optional[List[str]] = None):
    """Constructs the MetadataWriter.

    Args:
      model_buffer: valid buffer of the model file.
      metadata_buffer: valid buffer of the metadata.
      associated_files: path to the associated files to be populated.
    """
    self._model_buffer = model_buffer
    self._metadata_buffer = metadata_buffer
    self._associated_files = associated_files if associated_files else []
    self._populated_model_buffer = None

  @classmethod
  def create_from_metadata_info(
      cls,
      model_buffer: bytearray,
      general_md: Optional[metadata_info.GeneralMd] = None,
      input_md: Optional[List[Type[metadata_info.TensorMd]]] = None,
      output_md: Optional[List[Type[metadata_info.TensorMd]]] = None,
      associated_files: Optional[List[str]] = None):
    """Creates MetadataWriter based on the metadata information.

    Args:
      model_buffer: valid buffer of the model file.
      general_md: general information about the model.
      input_md: metadata information of the input tensors.
      output_md: metadata information of the output tensors.
      associated_files: path to the associated files to be populated.

    Returns:
      A MetadataWriter Object.

    Raises:
      ValueError: if the tensor names from `input_md` and `output_md` do not
      match the tensor names read from the model.
    """

    if general_md is None:
      general_md = metadata_info.GeneralMd()
    if input_md is None:
      input_md = []
    if output_md is None:
      output_md = []

    # Order the input/output metadata according to tensor orders from the model.
    input_md = _order_tensor_metadata(
        input_md, writer_utils.get_input_tensor_names(model_buffer))
    output_md = _order_tensor_metadata(
        output_md, writer_utils.get_output_tensor_names(model_buffer))

    model_metadata = general_md.create_metadata()
    input_metadata = [m.create_metadata() for m in input_md]
    output_metadata = [m.create_metadata() for m in output_md]
    return cls.create_from_metadata(model_buffer, model_metadata,
                                    input_metadata, output_metadata,
                                    associated_files)

  @classmethod
  def create_from_metadata(
      cls,
      model_buffer: bytearray,
      model_metadata: Optional[_metadata_fb.ModelMetadataT] = None,
      input_metadata: Optional[List[_metadata_fb.TensorMetadataT]] = None,
      output_metadata: Optional[List[_metadata_fb.TensorMetadataT]] = None,
      associated_files: Optional[List[str]] = None,
      input_process_units: Optional[List[_metadata_fb.ProcessUnitT]] = None,
      output_process_units: Optional[List[_metadata_fb.ProcessUnitT]] = None):
    """Creates MetadataWriter based on the metadata Flatbuffers Python Objects.

    Args:
      model_buffer: valid buffer of the model file.
      model_metadata: general model metadata [1]. The subgraph_metadata will be
        refreshed with input_metadata and output_metadata.
      input_metadata: a list of metadata of the input tensors [2].
      output_metadata: a list of metadata of the output tensors [3].
      associated_files: path to the associated files to be populated.
      input_process_units: a lits of metadata of the input process units [4].
      output_process_units: a lits of metadata of the output process units [5].
      [1]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L640-L681
      [2]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L590
      [3]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L599
      [4]:
        https://github.com/tensorflow/tflite-support/blob/b5cc57c74f7990d8bc055795dfe8d50267064a57/tensorflow_lite_support/metadata/metadata_schema.fbs#L646
      [5]:
        https://github.com/tensorflow/tflite-support/blob/b5cc57c74f7990d8bc055795dfe8d50267064a57/tensorflow_lite_support/metadata/metadata_schema.fbs#L650
    Returns:
      A MetadataWriter Object.
    """
    # Create empty tensor metadata when input_metadata/output_metadata are None
    # to bypass MetadataPopulator verification.
    if not input_metadata:
      model = _schema_fb.Model.GetRootAsModel(model_buffer, 0)
      num_input_tensors = model.Subgraphs(0).InputsLength()
      input_metadata = [
          _metadata_fb.TensorMetadataT() for i in range(num_input_tensors)
      ]

    if not output_metadata:
      model = _schema_fb.Model.GetRootAsModel(model_buffer, 0)
      num_output_tensors = model.Subgraphs(0).OutputsLength()
      output_metadata = [
          _metadata_fb.TensorMetadataT() for i in range(num_output_tensors)
      ]

    _fill_default_tensor_names(
        input_metadata, writer_utils.get_input_tensor_names(model_buffer))

    _fill_default_tensor_names(
        output_metadata, writer_utils.get_output_tensor_names(model_buffer))

    subgraph_metadata = _metadata_fb.SubGraphMetadataT()
    subgraph_metadata.inputTensorMetadata = input_metadata
    subgraph_metadata.outputTensorMetadata = output_metadata
    subgraph_metadata.inputProcessUnits = input_process_units
    subgraph_metadata.outputProcessUnits = output_process_units

    if model_metadata is None:
      model_metadata = _metadata_fb.ModelMetadataT()
    model_metadata.subgraphMetadata = [subgraph_metadata]

    b = flatbuffers.Builder(0)
    b.Finish(
        model_metadata.Pack(b),
        _metadata.MetadataPopulator.METADATA_FILE_IDENTIFIER)
    return cls(model_buffer, b.Output(), associated_files)

  def populate(self) -> bytearray:
    """Populates the metadata and label file to the model file.

    Returns:
      A new model buffer with the metadata and associated files.
    """
    if self._populated_model_buffer:
      return self._populated_model_buffer

    populator = _metadata.MetadataPopulator.with_model_buffer(
        self._model_buffer)
    if self._model_buffer is not None:
      populator.load_metadata_buffer(self._metadata_buffer)
    if self._associated_files:
      populator.load_associated_files(self._associated_files)
    populator.populate()
    self._populated_model_buffer = populator.get_model_buffer()
    return self._populated_model_buffer

  def get_metadata_json(self) -> str:
    """Gets the generated JSON metadata string before populated into model.

    This method returns the metadata buffer before populated into the model.
    More fields could be filled by MetadataPopulator, such as
    min_parser_version. Use get_populated_metadata_json() if you want to get the
    final metadata string.

    Returns:
      The generated JSON metadata string before populated into model.
    """
    return _metadata.convert_to_json(bytes(self._metadata_buffer))

  def get_populated_metadata_json(self) -> str:
    """Gets the generated JSON metadata string after populated into model.

    More fields could be filled by MetadataPopulator, such as
    min_parser_version. Use get_metadata_json() if you want to get the
    original metadata string.

    Returns:
      The generated JSON metadata string after populated into model.
    """
    displayer = _metadata.MetadataDisplayer.with_model_buffer(self.populate())
    return displayer.get_metadata_json()


# If tensor name in metadata is empty, default to the tensor name saved in
# the model.
def _fill_default_tensor_names(
    tensor_metadata: List[_metadata_fb.TensorMetadataT],
    tensor_names_from_model: List[str]):
  for metadata, name in zip(tensor_metadata, tensor_names_from_model):
    metadata.name = metadata.name or name


def _order_tensor_metadata(
    tensor_md: List[Type[metadata_info.TensorMd]],
    tensor_names_from_model: List[str]) -> List[Type[metadata_info.TensorMd]]:
  """Orders tensor_md according to the tensor names from the model."""
  tensor_names_from_arg = [
      md.tensor_name for md in tensor_md or [] if md.tensor_name is not None
  ]
  if not tensor_names_from_arg:
    return tensor_md

  if collections.Counter(tensor_names_from_arg) != collections.Counter(
      tensor_names_from_model):
    raise ValueError(
        "The tensor names from arguments ({}) do not match the tensor names"
        " read from the model ({}).".format(tensor_names_from_arg,
                                            tensor_names_from_model))
  ordered_tensor_md = []
  name_md_dict = dict(zip(tensor_names_from_arg, tensor_md))
  for name in tensor_names_from_model:
    ordered_tensor_md.append(name_md_dict[name])
  return ordered_tensor_md
