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

import numpy
import numpy.typing

class AudioBuffer:
    def __init__(self, arg0, arg1: int, arg2: AudioFormat) -> None: ...
    @property
    def audio_format(self) -> AudioFormat: ...
    @property
    def buffer_size(self) -> int: ...
    @property
    def float_buffer(self) -> numpy.typing.NDArray[numpy.float32]: ...

class AudioFormat:
    def __init__(self, arg0: int, arg1: int) -> None: ...
    @property
    def channels(self) -> int: ...
    @property
    def sample_rate(self) -> int: ...

def LoadAudioBufferFromFile(arg0: str, arg1: int, arg2: int, arg3) -> AudioBuffer: ...
