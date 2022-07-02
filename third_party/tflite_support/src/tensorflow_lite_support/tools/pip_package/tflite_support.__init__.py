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
"""The TensorFlow Lite Support Library.

Install the pip package:

```
pip install tflite-support
```

This package provides two major features:
* Metadata writers: add metadata to TensorFlow Lite models.
* Task Library: run TensorFlow Lite models of major machine learning tasks.

To learn more about metadata, flatbuffers and TensorFlow Lite models, check out
the [metadata section](https://www.tensorflow.org/lite/convert/metadata) of the
TensorFlow Lite guide.

To learn more about Task Library, check out the
[documentation](https://www.tensorflow.org/lite/inference_with_metadata/task_library/overview)
on the TensorFlow Lite website.
"""

# In the original project structure, all python targets are accessed by paths
# like tensorflow_lite_support.metadata.metadata.MetadataDisplayer, which is
# verbose and deep. This file provides some shortcuts. It's also compatible
# with our first version Pip package.

# In pip build, this file will be renamed as tflite_support/__init__.py.

import flatbuffers
import platform

from tensorflow_lite_support.metadata import metadata_schema_py_generated
from tensorflow_lite_support.metadata import schema_py_generated
from tensorflow_lite_support.metadata.python import metadata
from tflite_support import metadata_writers

if platform.system() != 'Windows':
  # Task Library is not supported on Windows yet.
  from tflite_support import task
