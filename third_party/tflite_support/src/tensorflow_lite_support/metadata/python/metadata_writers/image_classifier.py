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
"""Writes metadata and label file to the image classifier models."""

from typing import List, Optional

from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_writer
from tensorflow_lite_support.metadata.python.metadata_writers import writer_utils

_MODEL_NAME = "ImageClassifier"
MODEL_DESCRIPTION = ("Identify the most prominent object in the image from a "
                     "known set of categories.")
INPUT_NAME = "image"
INPUT_DESCRIPTION = "Input image to be classified."
OUTPUT_NAME = "probability"
OUTPUT_DESCRIPTION = "Probabilities of the labels respectively."


class MetadataWriter(metadata_writer.MetadataWriter):
  """Writes metadata into an image classifier."""

  @classmethod
  def create_from_metadata_info(
      cls,
      model_buffer: bytearray,
      general_md: Optional[metadata_info.GeneralMd] = None,
      input_md: Optional[metadata_info.InputImageTensorMd] = None,
      output_md: Optional[metadata_info.ClassificationTensorMd] = None):
    """Creates MetadataWriter based on general/input/output information.

    Args:
      model_buffer: valid buffer of the model file.
      general_md: general information about the model. If not specified, default
        general metadata will be generated.
      input_md: input image tensor informaton, if not specified, default input
        metadata will be generated.
      output_md: output classification tensor informaton, if not specified,
        default output metadata will be generated.

    Returns:
      A MetadataWriter object.
    """

    if general_md is None:
      general_md = metadata_info.GeneralMd(
          name=_MODEL_NAME, description=MODEL_DESCRIPTION)

    if input_md is None:
      input_md = metadata_info.InputImageTensorMd(
          name=INPUT_NAME,
          description=INPUT_DESCRIPTION,
          color_space_type=_metadata_fb.ColorSpaceType.RGB)

    if output_md is None:
      output_md = metadata_info.ClassificationTensorMd(
          name=OUTPUT_NAME, description=OUTPUT_DESCRIPTION)

    if output_md.associated_files is None:
      output_md.associated_files = []

    return super().create_from_metadata_info(
        model_buffer=model_buffer,
        general_md=general_md,
        input_md=[input_md],
        output_md=[output_md],
        associated_files=[
            file.file_path for file in output_md.associated_files
        ])

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
      label_file_paths: paths to the label files [2] in the classification
        tensor. Pass in an empty list if the model does not have any label file.
      score_calibration_md: information of the score calibration operation [3]
        in the classification tensor. Optional if the model does not use score
        calibration.
      [1]:
        https://www.tensorflow.org/lite/convert/metadata#normalization_and_quantization_parameters
      [2]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L95
      [3]:
        https://github.com/tensorflow/tflite-support/blob/5e0cdf5460788c481f5cd18aab8728ec36cf9733/tensorflow_lite_support/metadata/metadata_schema.fbs#L434

    Returns:
      A MetadataWriter object.
    """
    input_md = metadata_info.InputImageTensorMd(
        name=INPUT_NAME,
        description=INPUT_DESCRIPTION,
        norm_mean=input_norm_mean,
        norm_std=input_norm_std,
        color_space_type=_metadata_fb.ColorSpaceType.RGB,
        tensor_type=writer_utils.get_input_tensor_types(model_buffer)[0])

    output_md = metadata_info.ClassificationTensorMd(
        name=OUTPUT_NAME,
        description=OUTPUT_DESCRIPTION,
        label_files=[
            metadata_info.LabelFileMd(file_path=file_path)
            for file_path in label_file_paths
        ],
        tensor_type=writer_utils.get_output_tensor_types(model_buffer)[0],
        score_calibration_md=score_calibration_md)

    return cls.create_from_metadata_info(
        model_buffer, input_md=input_md, output_md=output_md)
