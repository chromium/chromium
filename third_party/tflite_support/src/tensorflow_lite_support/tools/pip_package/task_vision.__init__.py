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
"""An import entry for Task Vision Library."""

from tensorflow_lite_support.python.task.vision import image_classifier
from tensorflow_lite_support.python.task.vision import image_embedder
from tensorflow_lite_support.python.task.vision import object_detector
from tensorflow_lite_support.python.task.vision.core import tensor_image

ImageClassifier = image_classifier.ImageClassifier
ImageClassifierOptions = image_classifier.ImageClassifierOptions
ObjectDetector = object_detector.ObjectDetector
ObjectDetectorOptions = object_detector.ObjectDetectorOptions
ImageEmbedder = image_embedder.ImageEmbedder
ImageEmbedderOptions = image_embedder.ImageEmbedderOptions
TensorImage = tensor_image.TensorImage
