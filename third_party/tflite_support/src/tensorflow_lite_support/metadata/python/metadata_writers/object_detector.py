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
"""Writes metadata and label file to the object detector models."""

import logging
from typing import List, Optional, Type, Union

import flatbuffers
from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata import schema_py_generated as _schema_fb
from tensorflow_lite_support.metadata.python import metadata as _metadata
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_writer
from tensorflow_lite_support.metadata.python.metadata_writers import writer_utils

_MODEL_NAME = "ObjectDetector"
_MODEL_DESCRIPTION = (
    "Identify which of a known set of objects might be present and provide "
    "information about their positions within the given image or a video "
    "stream.")
_INPUT_NAME = "image"
_INPUT_DESCRIPTION = "Input image to be detected."
# The output tensor names shouldn't be changed since these name will be used
# to handle the order of output in TFLite Task Library when doing inference
# in on-device application.
_OUTPUT_LOCATION_NAME = "location"
_OUTPUT_LOCATION_DESCRIPTION = "The locations of the detected boxes."
_OUTPUT_CATRGORY_NAME = "category"
_OUTPUT_CATEGORY_DESCRIPTION = "The categories of the detected boxes."
_OUTPUT_SCORE_NAME = "score"
_OUTPUT_SCORE_DESCRIPTION = "The scores of the detected boxes."
_OUTPUT_NUMBER_NAME = "number of detections"
_OUTPUT_NUMBER_DESCRIPTION = "The number of the detected boxes."
_CONTENT_VALUE_DIM = 2
_BOUNDING_BOX_INDEX = (1, 0, 3, 2)
_GROUP_NAME = "detection_result"


def _create_1d_value_range(dim: int) -> _metadata_fb.ValueRangeT:
  """Creates the 1d ValueRange based on the given dimension."""
  value_range = _metadata_fb.ValueRangeT()
  value_range.min = dim
  value_range.max = dim
  return value_range


def _create_location_metadata(
    location_md: metadata_info.TensorMd) -> _metadata_fb.TensorMetadataT:
  """Creates the metadata for the location tensor."""
  location_metadata = location_md.create_metadata()
  content = _metadata_fb.ContentT()
  content.contentPropertiesType = (
      _metadata_fb.ContentProperties.BoundingBoxProperties)
  properties = _metadata_fb.BoundingBoxPropertiesT()
  properties.index = list(_BOUNDING_BOX_INDEX)
  properties.type = _metadata_fb.BoundingBoxType.BOUNDARIES
  properties.coordinateType = _metadata_fb.CoordinateType.RATIO
  content.contentProperties = properties
  content.range = _create_1d_value_range(_CONTENT_VALUE_DIM)
  location_metadata.content = content
  return location_metadata


# This is needed for both the output category tensor and the score tensor.
def _create_metadata_with_value_range(
    tensor_md: metadata_info.TensorMd) -> _metadata_fb.TensorMetadataT:
  """Creates tensor metadata with extra value range information."""
  tensor_metadata = tensor_md.create_metadata()
  tensor_metadata.content.range = _create_1d_value_range(_CONTENT_VALUE_DIM)
  return tensor_metadata


def _get_tflite_outputs(model_buffer: bytearray) -> List[int]:
  """Gets the tensor indices of output in the TFLite Subgraph."""
  model = _schema_fb.Model.GetRootAsModel(model_buffer, 0)
  return model.Subgraphs(0).OutputsAsNumpy()


def _extend_new_files(
    file_list: List[str],
    associated_files: Optional[List[Type[metadata_info.AssociatedFileMd]]]):
  """Extends new associated files to the file list."""
  if not associated_files:
    return

  for file in associated_files:
    if file.file_path not in file_list:
      file_list.append(file.file_path)


class MetadataWriter(metadata_writer.MetadataWriter):
  """Writes metadata into an object detector."""

  @classmethod
  def create_from_metadata_info(
      cls,
      model_buffer: bytearray,
      general_md: Optional[metadata_info.GeneralMd] = None,
      input_md: Optional[metadata_info.InputImageTensorMd] = None,
      output_location_md: Optional[metadata_info.TensorMd] = None,
      output_category_md: Optional[metadata_info.CategoryTensorMd] = None,
      output_score_md: Union[None, metadata_info.TensorMd,
                             metadata_info.ClassificationTensorMd] = None,
      output_number_md: Optional[metadata_info.TensorMd] = None):
    """Creates MetadataWriter based on general/input/outputs information.

    Args:
      model_buffer: valid buffer of the model file.
      general_md: general information about the model.
      input_md: input image tensor informaton.
      output_location_md: output location tensor informaton. The location tensor
        is a multidimensional array of [N][4] floating point values between 0
        and 1, the inner arrays representing bounding boxes in the form [top,
        left, bottom, right].
      output_category_md: output category tensor information. The category
        tensor is an array of N integers (output as floating point values) each
        indicating the index of a class label from the labels file.
      output_score_md: output score tensor information. The score tensor is an
        array of N floating point values between 0 and 1 representing
        probability that a class was detected. Use ClassificationTensorMd to
        calibrate score.
      output_number_md: output number of detections tensor information. This
        tensor is an integer value of N.

    Returns:
      A MetadataWriter object.
    """
    if general_md is None:
      general_md = metadata_info.GeneralMd(
          name=_MODEL_NAME, description=_MODEL_DESCRIPTION)

    if input_md is None:
      input_md = metadata_info.InputImageTensorMd(
          name=_INPUT_NAME,
          description=_INPUT_DESCRIPTION,
          color_space_type=_metadata_fb.ColorSpaceType.RGB)

    warn_message_format = (
        "The output name isn't the default string \"%s\". This may cause the "
        "model not work in the TFLite Task Library since the tensor name will "
        "be used to handle the output order in the TFLite Task Library.")
    if output_location_md is None:
      output_location_md = metadata_info.TensorMd(
          name=_OUTPUT_LOCATION_NAME, description=_OUTPUT_LOCATION_DESCRIPTION)
    elif output_location_md.name != _OUTPUT_LOCATION_NAME:
      logging.warning(warn_message_format, _OUTPUT_LOCATION_NAME)

    if output_category_md is None:
      output_category_md = metadata_info.CategoryTensorMd(
          name=_OUTPUT_CATRGORY_NAME, description=_OUTPUT_CATEGORY_DESCRIPTION)
    elif output_category_md.name != _OUTPUT_CATRGORY_NAME:
      logging.warning(warn_message_format, _OUTPUT_CATRGORY_NAME)

    if output_score_md is None:
      output_score_md = metadata_info.ClassificationTensorMd(
          name=_OUTPUT_SCORE_NAME,
          description=_OUTPUT_SCORE_DESCRIPTION,
      )
    elif output_score_md.name != _OUTPUT_SCORE_NAME:
      logging.warning(warn_message_format, _OUTPUT_SCORE_NAME)

    if output_number_md is None:
      output_number_md = metadata_info.TensorMd(
          name=_OUTPUT_NUMBER_NAME, description=_OUTPUT_NUMBER_DESCRIPTION)
    elif output_number_md.name != _OUTPUT_NUMBER_NAME:
      logging.warning(warn_message_format, _OUTPUT_NUMBER_NAME)

    # Create output tensor group info.
    group = _metadata_fb.TensorGroupT()
    group.name = _GROUP_NAME
    group.tensorNames = [
        output_location_md.name, output_category_md.name, output_score_md.name
    ]

    # Gets the tensor inidces of tflite outputs and then gets the order of the
    # output metadata by the value of tensor indices. For instance, if the
    # output indices are [601, 599, 598, 600], tensor names and indices aligned
    # are:
    #   - location: 598
    #   - category: 599
    #   - score: 600
    #   - number of detections: 601
    # because of the op's ports of TFLITE_DETECTION_POST_PROCESS
    # (https://github.com/tensorflow/tensorflow/blob/a4fe268ea084e7d323133ed7b986e0ae259a2bc7/tensorflow/lite/kernels/detection_postprocess.cc#L47-L50).
    # Thus, the metadata of tensors are sorted in this way, according to
    # output_tensor_indicies correctly.
    output_tensor_indices = _get_tflite_outputs(model_buffer)
    metadata_list = [
        _create_location_metadata(output_location_md),
        _create_metadata_with_value_range(output_category_md),
        _create_metadata_with_value_range(output_score_md),
        output_number_md.create_metadata()
    ]

    # Align indices with tensors.
    sorted_indices = sorted(output_tensor_indices)
    indices_to_tensors = dict(zip(sorted_indices, metadata_list))

    # Output metadata according to output_tensor_indices.
    output_metadata = [indices_to_tensors[i] for i in output_tensor_indices]

    # Create subgraph info.
    subgraph_metadata = _metadata_fb.SubGraphMetadataT()
    subgraph_metadata.inputTensorMetadata = [input_md.create_metadata()]
    subgraph_metadata.outputTensorMetadata = output_metadata
    subgraph_metadata.outputTensorGroups = [group]

    # Create model metadata
    model_metadata = general_md.create_metadata()
    model_metadata.subgraphMetadata = [subgraph_metadata]

    b = flatbuffers.Builder(0)
    b.Finish(
        model_metadata.Pack(b),
        _metadata.MetadataPopulator.METADATA_FILE_IDENTIFIER)

    associated_files = []
    _extend_new_files(associated_files, output_category_md.associated_files)
    _extend_new_files(associated_files, output_score_md.associated_files)
    return cls(model_buffer, b.Output(), associated_files=associated_files)

  @classmethod
  def create_for_inference(
      cls,
      model_buffer: bytearray,
      input_norm_mean: List[float],
      input_norm_std: List[float],
      label_file_paths: List[str],
      score_calibration_md: Optional[metadata_info.ScoreCalibrationMd] = None):
    """Creates mandatory metadata for TFLite Support inference.

    The parameters required in this method are mandatory when using TFLite
    Support features, such as Task library and Codegen tool (Android Studio ML
    Binding). Other metadata fields will be set to default. If other fields need
    to be filled, use the method `create_from_metadata_info` to edit them.

    Args:
      model_buffer: valid buffer of the model file.
      input_norm_mean: the mean value used in the input tensor normalization
        [1].
      input_norm_std: the std value used in the input tensor normalizarion [1].
      label_file_paths: paths to the label files [2] in the category tensor.
        Pass in an empty list, If the model does not have any label file.
      score_calibration_md: information of the score calibration operation [3]
        in the classification tensor. Optional if the model does not use score
        calibration.
      [1]:
        https://www.tensorflow.org/lite/convert/metadata#normalization_and_quantization_parameters
      [2]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L108
      [3]:
        https://github.com/tensorflow/tflite-support/blob/5e0cdf5460788c481f5cd18aab8728ec36cf9733/tensorflow_lite_support/metadata/metadata_schema.fbs#L434

    Returns:
      A MetadataWriter object.
    """
    input_md = metadata_info.InputImageTensorMd(
        name=_INPUT_NAME,
        description=_INPUT_DESCRIPTION,
        norm_mean=input_norm_mean,
        norm_std=input_norm_std,
        color_space_type=_metadata_fb.ColorSpaceType.RGB,
        tensor_type=writer_utils.get_input_tensor_types(model_buffer)[0])

    output_category_md = metadata_info.CategoryTensorMd(
        name=_OUTPUT_CATRGORY_NAME,
        description=_OUTPUT_CATEGORY_DESCRIPTION,
        label_files=[
            metadata_info.LabelFileMd(file_path=file_path)
            for file_path in label_file_paths
        ])

    output_score_md = metadata_info.ClassificationTensorMd(
        name=_OUTPUT_SCORE_NAME,
        description=_OUTPUT_SCORE_DESCRIPTION,
        score_calibration_md=score_calibration_md
    )

    return cls.create_from_metadata_info(
        model_buffer,
        input_md=input_md,
        output_category_md=output_category_md,
        output_score_md=output_score_md)
