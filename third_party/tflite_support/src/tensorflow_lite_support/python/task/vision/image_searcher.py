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
"""Image searcher task."""

import dataclasses
from typing import Optional

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.processor.proto import embedding_options_pb2
from tensorflow_lite_support.python.task.processor.proto import search_options_pb2
from tensorflow_lite_support.python.task.processor.proto import search_result_pb2
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.task.vision.core.pybinds import image_utils
from tensorflow_lite_support.python.task.vision.pybinds import _pywrap_image_searcher

_CppImageSearcher = _pywrap_image_searcher.ImageSearcher
_BaseOptions = base_options_module.BaseOptions
_EmbeddingOptions = embedding_options_pb2.EmbeddingOptions
_SearchOptions = search_options_pb2.SearchOptions


@dataclasses.dataclass
class ImageSearcherOptions:
  """Options for the image search task.

  Attributes:
    base_options: Base options for the image searcher task.
    embedding_options: Embedding options for the image searcher task.
    search_options: Search options for the image searcher task.
  """
  base_options: _BaseOptions
  embedding_options: _EmbeddingOptions = _EmbeddingOptions()
  search_options: _SearchOptions = _SearchOptions()


class ImageSearcher(object):
  """Class to performs image search.

  It works by performing embedding extraction on images, followed by
  nearest-neighbor search in an index of embeddings through ScaNN.
  """

  def __init__(self, options: ImageSearcherOptions,
               cpp_searcher: _CppImageSearcher) -> None:
    """Initializes the `ImageSearcher` object."""
    # Creates the object of C++ ImageSearcher class.
    self._options = options
    self._searcher = cpp_searcher

  @classmethod
  def create_from_file(
      cls,
      model_file_path: str,
      index_file_path: Optional[str] = None) -> "ImageSearcher":
    """Creates the `ImageSearcher` object from a TensorFlow Lite model.

    Args:
      model_file_path: Path to the model.
      index_file_path: Path to the index. Only required if the index is not
        attached to the output tensor metadata as an AssociatedFile with type
        SCANN_INDEX_FILE.

    Returns:
      `ImageSearcher` object that's created from `options`.

    Raises:
      ValueError: If failed to create `ImageSearcher` object from the provided
        file such as invalid file.
      RuntimeError: If other types of error occurred.
    """
    options = ImageSearcherOptions(
        base_options=_BaseOptions(file_name=model_file_path),
        search_options=_SearchOptions(index_file_name=index_file_path))
    return cls.create_from_options(options)

  @classmethod
  def create_from_options(cls,
                          options: ImageSearcherOptions) -> "ImageSearcher":
    """Creates the `ImageSearcher` object from image searcher options.

    Args:
      options: Options for the image searcher task.

    Returns:
      `ImageSearcher` object that's created from `options`.
    Raises:
      ValueError: If failed to create `ImageSearcher` object from
        `ImageSearcherOptions` such as missing the model.
      RuntimeError: If other types of error occurred.
    """
    searcher = _CppImageSearcher.create_from_options(
        options.base_options.to_pb2(), options.embedding_options.to_pb2(),
        options.search_options.to_pb2())
    return cls(options, searcher)

  def search(
      self,
      image: tensor_image.TensorImage,
      bounding_box: Optional[bounding_box_pb2.BoundingBox] = None
  ) -> search_result_pb2.SearchResult:
    """Search for image with similar semantic meaning.

    This method performs actual feature extraction on the provided image input,
    followed by nearest-neighbor search in the index.

    Args:
      image: Tensor image, used to extract the feature vectors.
      bounding_box: Bounding box, optional. If set, performed feature vector
        extraction only on the provided region of interest. Note that the region
        of interest is not clamped, so this method will fail if the region is
        out of bounds of the input image.

    Returns:
      Search result.

    Raises:
      ValueError: If any of the input arguments is invalid.
      RuntimeError: If failed to perform nearest-neighbor search.
    """
    image_data = image_utils.ImageData(image.buffer)
    if bounding_box is None:
      search_result = self._searcher.search(image_data)
    else:
      search_result = self._searcher.search(image_data, bounding_box.to_pb2())
    return search_result_pb2.SearchResult.create_from_pb2(search_result)

  def get_user_info(self) -> str:
    """Gets the user info stored in the index file.

    Returns:
      Opaque user info stored in the index file (if any), in raw binary form.
      Returns an empty string if the index doesn't contain user info.
    """
    return self._searcher.get_user_info()

  @property
  def options(self) -> ImageSearcherOptions:
    return self._options
