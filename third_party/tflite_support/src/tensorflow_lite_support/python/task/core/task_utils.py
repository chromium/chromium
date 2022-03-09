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
"""Utils functions used in Task Python API."""

from tensorflow_lite_support.cc.task.core.proto import base_options_pb2
from tensorflow_lite_support.python.task.core import task_options
from tensorflow_lite_support.python.task.core.proto import configuration_pb2

_ProtoBaseOptions = base_options_pb2.BaseOptions


def ConvertToProtoBaseOptions(
    options: task_options.BaseOptions) -> _ProtoBaseOptions:
  """Convert the Python BaseOptions to Proto BaseOptions.

  Python BaseOptions is a subset of the Proto BaseOptions that strips off
  configurations that are useless in Python development.

  Args:
    options: the Python BaseOptions object.

  Returns:
    The Proto BaseOptions object.
  """
  proto_options = _ProtoBaseOptions()

  if options.model_file.file_content:
    proto_options.model_file.file_content = options.model_file.file_content
  elif options.model_file.file_name:
    proto_options.model_file.file_name = options.model_file.file_name

  proto_options.compute_settings.tflite_settings.cpu_settings.num_threads = (
      options.num_threads)

  if options.use_coral:
    proto_options.compute_settings.tflite_settings.delegate = (
        configuration_pb2.Delegate.EDGETPU_CORAL)

  return proto_options
