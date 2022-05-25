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
# ==============================================================================
"""The TensorFlow Lite Task Library.

TensorFlow Lite Task Library contains a set of powerful and easy-to-use
task-specific libraries for app developers to create ML experiences with
TensorFlow Lite. It provides optimized out-of-box model interfaces for popular
machine learning tasks, such as image and text classification. The model
interfaces are specifically designed for each task to achieve the best
performance and usability.

Read more in the [Task Library Guide](
https://tensorflow.org/lite/inference_with_metadata/task_library/overview).
"""

from . import audio
from . import core
from . import processor
from . import text
from . import vision
