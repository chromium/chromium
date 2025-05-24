# Copyright 2023 The TensorFlow Authors. All Rights Reserved.
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

class ImageData:
    def __init__(self, arg0) -> None: ...
    @property
    def channels(self) -> int: ...
    @property
    def height(self) -> int: ...
    @property
    def width(self) -> int: ...

def decode_image_from_buffer(arg0: str, arg1: int) -> ImageData: ...
def decode_image_from_file(arg0: str) -> ImageData: ...
def image_data_free(arg0: ImageData) -> None: ...
