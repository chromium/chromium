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
"""TF Lite Metadata Writer API.

This module provides interfaces for writing metadata for common model types
supported by the task library, such as:

  * Image classification
  * Object detection
  * Image segmentation
  * (Bert) Natural language classification
  * Audio classification

It is provided as part of the `tflite-support` package:

```
pip install tflite-support
```

Learn more about this API in the [metadata writer
tutorial](https://www.tensorflow.org/lite/convert/metadata_writer_tutorial).
"""

from tensorflow_lite_support.metadata.python.metadata_writers import audio_classifier
from tensorflow_lite_support.metadata.python.metadata_writers import bert_nl_classifier
from tensorflow_lite_support.metadata.python.metadata_writers import image_classifier
from tensorflow_lite_support.metadata.python.metadata_writers import image_segmenter
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import nl_classifier
from tensorflow_lite_support.metadata.python.metadata_writers import object_detector
from tensorflow_lite_support.metadata.python.metadata_writers import writer_utils
