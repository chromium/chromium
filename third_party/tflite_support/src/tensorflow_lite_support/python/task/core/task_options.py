# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
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
"""Options to configure Task APIs."""

import dataclasses

from typing import Any, Optional


@dataclasses.dataclass
class ExternalFile:
  """Represents external files used by the Task APIs.

  The files can be specified by one of the following
  two ways:

  (1) file contents loaded in `file_content`.
  (2) file path in `file_name`.

  If more than one field of these fields is provided, they are used in this
  precedence order.
  """
  # The file contents as a byte array.
  file_content: Optional[bytearray] = None

  # The path to the file to open and mmap in memory.
  file_name: Optional[str] = None

  def __eq__(self, other: Any) -> bool:
    if self is other:
      return True

    if not isinstance(other, self.__class__):
      return False

    if self.file_name is not None and self.file_name == other.file_name:
      return True

    if (self.file_content is not None and other.file_content is not None and
        len(self.file_content) == len(other.file_content) and
        self.file_content == other.file_content):
      return True

    return False


@dataclasses.dataclass
class BaseOptions:
  """Base options that is used for creation of any type of task."""
  # The external model file, as a single standalone TFLite file. It could be
  # packed with TFLite Model Metadata[1] and associated files if exist. Failure
  # to provide the necessary metadata and associated files might result in
  # errors.
  # Check the documentation for each task about the specific requirement.
  # [1]: https://www.tensorflow.org/lite/convert/metadata
  model_file: ExternalFile

  # Number of thread: the defaule value -1 means Interpreter will decide what
  # is the most appropriate num_threads.
  num_threads: int = -1

  # If true, inference will be delegated to a connected Coral Edge TPU device.
  use_coral: bool = False

  def __eq__(self, other: Any) -> bool:
    return (isinstance(other, self.__class__) and
            self.model_file == other.model_file and
            self.num_threads == other.num_threads and
            self.use_coral == other.use_coral)
