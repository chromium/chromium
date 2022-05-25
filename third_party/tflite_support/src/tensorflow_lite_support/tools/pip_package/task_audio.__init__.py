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
"""TensorFlow Lite Task Library Audio APIs.

This module provides interface to run TensorFlow Lite audio models.
"""

from tensorflow_lite_support.python.task.audio import audio_classifier
from tensorflow_lite_support.python.task.audio import audio_embedder
from tensorflow_lite_support.python.task.audio.core import audio_record
from tensorflow_lite_support.python.task.audio.core import tensor_audio

AudioClassifier = audio_classifier.AudioClassifier
AudioClassifierOptions = audio_classifier.AudioClassifierOptions
AudioEmbedder = audio_embedder.AudioEmbedder
AudioEmbedderOptions = audio_embedder.AudioEmbedderOptions
AudioRecord = audio_record.AudioRecord
AudioFormat = tensor_audio.AudioFormat
TensorAudio = tensor_audio.TensorAudio

# Remove unnecessary modules to avoid duplication in API docs.
del audio_classifier
del audio_embedder
del audio_record
del tensor_audio