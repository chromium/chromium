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
"""Writes metadata and label file to the image segmenter models."""

from typing import List, Optional

from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_writer
from tensorflow_lite_support.metadata.python.metadata_writers import writer_utils

_MODEL_NAME = "ImageSegmenter"
_MODEL_DESCRIPTION = ("Semantic image segmentation predicts whether each pixel "
                      "of an image is associated with a certain class.")
_INPUT_NAME = "image"
_INPUT_DESCRIPTION = "Input image to be segmented."
_OUTPUT_NAME = "segmentation_masks"
_OUTPUT_DESCRIPTION = "Masks over the target objects with high accuracy."
# The output tensor is in the shape of [1, ImageHeight, ImageWidth, N], where N
# is the number of objects that the segmentation model can recognize. The output
# tensor is essentially a list of grayscale bitmaps, where each value is the
# probability of the corresponding pixel belonging to a certain object type.
# Therefore, the content dimension range of the output tensor is [1, 2].
_CONTENT_DIM_MIN = 1
_CONTENT_DIM_MAX = 2


def _create_segmentation_masks_metadata(
    masks_md: metadata_info.TensorMd) -> _metadata_fb.TensorMetadataT:
  """Creates the metadata for the segmentation masks tensor."""
  masks_metadata = masks_md.create_metadata()

  # Create tensor content information.
  content = _metadata_fb.ContentT()
  content.contentProperties = _metadata_fb.ImagePropertiesT()
  content.contentProperties.colorSpace = _metadata_fb.ColorSpaceType.GRAYSCALE
  content.contentPropertiesType = _metadata_fb.ContentProperties.ImageProperties
  # Add the content range. See
  # https://github.com/tensorflow/tflite-support/blob/ace5d3f3ce44c5f77c70284fa9c5a4e3f2f92abb/tensorflow_lite_support/metadata/metadata_schema.fbs#L285-L347
  dim_range = _metadata_fb.ValueRangeT()
  dim_range.min = _CONTENT_DIM_MIN
  dim_range.max = _CONTENT_DIM_MAX
  content.range = dim_range
  masks_metadata.content = content

  return masks_metadata


class MetadataWriter(metadata_writer.MetadataWriter):
  """Writes metadata into an image segmenter."""

  @classmethod
  def create_from_metadata_info(
      cls,
      model_buffer: bytearray,
      general_md: Optional[metadata_info.GeneralMd] = None,
      input_md: Optional[metadata_info.InputImageTensorMd] = None,
      output_md: Optional[metadata_info.TensorMd] = None):
    """Creates MetadataWriter based on general/input/outputs information.

    Args:
      model_buffer: valid buffer of the model file.
      general_md: general information about the model.
      input_md: input image tensor informaton.
      output_md: output segmentation mask tensor informaton. This tensor is a
        multidimensional array of [1 x mask_height x mask_width x num_classes],
        where mask_width and mask_height are the dimensions of the segmentation
        masks produced by the model, and num_classes is the number of classes
        supported by the model.

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

    if output_md is None:
      output_md = metadata_info.TensorMd(
          name=_OUTPUT_NAME, description=_OUTPUT_DESCRIPTION)

    if output_md.associated_files is None:
      output_md.associated_files = []

    return super().create_from_metadata(
        model_buffer,
        model_metadata=general_md.create_metadata(),
        input_metadata=[input_md.create_metadata()],
        output_metadata=[_create_segmentation_masks_metadata(output_md)],
        associated_files=[
            file.file_path for file in output_md.associated_files
        ])

  @classmethod
  def create_for_inference(cls, model_buffer: bytearray,
                           input_norm_mean: List[float],
                           input_norm_std: List[float],
                           label_file_paths: List[str]):
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
        Pass in an empty list If the model does not have any label file.
      [1]:
        https://www.tensorflow.org/lite/convert/metadata#normalization_and_quantization_parameters
      [2]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L108

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

    output_md = metadata_info.TensorMd(
        name=_OUTPUT_NAME,
        description=_OUTPUT_DESCRIPTION,
        associated_files=[
            metadata_info.LabelFileMd(file_path=file_path)
            for file_path in label_file_paths
        ])

    return cls.create_from_metadata_info(
        model_buffer, input_md=input_md, output_md=output_md)
