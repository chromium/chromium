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
"""Test utils for MetadataWriter."""

import os
from typing import Union
import tensorflow as tf


def create_calibration_file(file_dir: str,
                            file_name: str = "score_calibration.txt",
                            content: str = "1.0,2.0,3.0,4.0") -> str:
  """Creates the calibration file."""
  calibration_file = os.path.join(file_dir, file_name)
  with open(calibration_file, mode="w") as file:
    file.write(content)
  return calibration_file


def load_file(file_name: str, mode: str = "rb") -> Union[str, bytes]:
  """Loads files from resources."""
  file_path = get_resource_path(file_name)
  with open(file_path, mode) as file:
    return file.read()


def get_resource_path(file_name: str) -> str:
  """Gets resource path from the loader."""
  return tf.compat.v1.resource_loader.get_path_to_datafile(file_name)
