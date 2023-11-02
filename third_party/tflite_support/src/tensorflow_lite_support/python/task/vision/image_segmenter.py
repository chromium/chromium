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
"""Image segmenter task."""

import dataclasses

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import segmentation_options_pb2
from tensorflow_lite_support.python.task.processor.proto import segmentations_pb2
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.task.vision.core.pybinds import image_utils
from tensorflow_lite_support.python.task.vision.pybinds import _pywrap_image_segmenter

_CppImageSegmenter = _pywrap_image_segmenter.ImageSegmenter
_SegmentationOptions = segmentation_options_pb2.SegmentationOptions
_BaseOptions = base_options_module.BaseOptions


@dataclasses.dataclass
class ImageSegmenterOptions:
  """Options for the image segmenter task.

  Attributes:
    base_options: Base options for the image segmenter task.
    segmentation_options: Segmentation options for the image segmenter task.
  """
  base_options: _BaseOptions
  segmentation_options: _SegmentationOptions = _SegmentationOptions()


class ImageSegmenter(object):
  """Class that performs segmentation on images."""

  def __init__(self, options: ImageSegmenterOptions,
               segmenter: _CppImageSegmenter) -> None:
    """Initializes the `ImageSegmenter` object."""
    # Creates the object of C++ ImageSegmenter class.
    self._options = options
    self._segmenter = segmenter

  @classmethod
  def create_from_file(cls, file_path: str) -> "ImageSegmenter":
    """Creates the `ImageSegmenter` object from a TensorFlow Lite model.

    Args:
      file_path: Path to the model.
    Returns:
      `ImageSegmenter` object that's created from `options`.
    Raises:
      ValueError: If failed to create `ImageSegmenter` object from the
        provided file such as invalid file.
      RuntimeError: If other types of error occurred.
    """
    base_options = _BaseOptions(file_name=file_path)
    options = ImageSegmenterOptions(base_options=base_options)
    return cls.create_from_options(options)

  @classmethod
  def create_from_options(cls,
                          options: ImageSegmenterOptions) -> "ImageSegmenter":
    """Creates the `ImageSegmenter` object from image segmenter options.

    Args:
      options: Options for the image segmenter task.
    Returns:
      `ImageSegmenter` object that's created from `options`.
    Raises:
      ValueError: If failed to create `ImageSegmenter` object from
        `ImageSegmenterOptions` such as missing the model.
      RuntimeError: If other types of error occurred.
    """
    segmenter = _CppImageSegmenter.create_from_options(
        options.base_options.to_pb2(), options.segmentation_options.to_pb2())
    return cls(options, segmenter)

  def segment(
      self,
      image: tensor_image.TensorImage
  ) -> segmentations_pb2.SegmentationResult:
    """Performs segmentation on the provided TensorImage.

    Args:
      image: Tensor image, used to extract the feature vectors.
    Returns:
      segmentation result.
    Raises:
      ValueError: If any of the input arguments is invalid.
      RuntimeError: If failed to run segmentation.
    """
    image_data = image_utils.ImageData(image.buffer)
    segmentation_result = self._segmenter.segment(image_data)
    return segmentations_pb2.SegmentationResult.create_from_pb2(
        segmentation_result)
