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
"""TensorFlow Lite Task Library Vision APIs.

This module provides interface to run TensorFlow Lite computer vision models.
"""

from tensorflow_lite_support.python.task.vision import image_classifier
from tensorflow_lite_support.python.task.vision import image_embedder
from tensorflow_lite_support.python.task.vision import image_segmenter
from tensorflow_lite_support.python.task.vision import image_searcher
from tensorflow_lite_support.python.task.vision import object_detector
from tensorflow_lite_support.python.task.vision.core import tensor_image

ImageClassifier = image_classifier.ImageClassifier
ImageClassifierOptions = image_classifier.ImageClassifierOptions
ObjectDetector = object_detector.ObjectDetector
ObjectDetectorOptions = object_detector.ObjectDetectorOptions
ImageEmbedder = image_embedder.ImageEmbedder
ImageEmbedderOptions = image_embedder.ImageEmbedderOptions
ImageSegmenter = image_segmenter.ImageSegmenter
ImageSegmenterOptions = image_segmenter.ImageSegmenterOptions
ImageSearcher = image_searcher.ImageSearcher
ImageSearcherOptions = image_searcher.ImageSearcherOptions
TensorImage = tensor_image.TensorImage

# Remove unnecessary modules to avoid duplication in API docs.
del image_classifier
del image_embedder
del image_segmenter
del image_searcher
del object_detector
del tensor_image
