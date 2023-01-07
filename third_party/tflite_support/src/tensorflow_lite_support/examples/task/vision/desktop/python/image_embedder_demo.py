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
"""Python demo tool for Image Embedder."""

import os

from absl import app
from absl import flags

from tensorflow_lite_support.python.task.core.proto import base_options_pb2
from tensorflow_lite_support.python.task.processor.proto import embedding_options_pb2
from tensorflow_lite_support.python.task.vision import image_embedder
from tensorflow_lite_support.python.task.vision.core import tensor_image

FLAGS = flags.FLAGS
_BaseOptions = base_options_pb2.BaseOptions

flags.DEFINE_string("model_path", None,
                    "Absolute path to the \".tflite\" image embedder model.")
flags.DEFINE_string(
    "first_image_path", None,
    "Absolute path to the first image, whose feature vector will be extracted "
    "and compared to the second image using cosine similarity. The image must "
    "be RGB or RGBA (grayscale is not supported). The image EXIF orientation "
    "flag, if any, is NOT taken into account.")
flags.DEFINE_string(
    "second_image_path", None,
    "Absolute path to the second image, whose feature vector will be extracted "
    "and compared to the first image using cosine similarity. The image must "
    "be RGB or RGBA (grayscale is not supported). The image EXIF orientation "
    "flag, if any, is NOT taken into account.")
flags.DEFINE_bool(
    "l2_normalize", False,
    "If true, the raw feature vectors returned by the image embedder will be "
    "normalized with L2-norm. Generally only needed if the model doesn't "
    "already contain a L2_NORMALIZATION TFLite Op.")
flags.DEFINE_bool(
    "quantize", False,
    "If true, the raw feature vectors returned by the image embedder will be "
    "quantized to 8 bit integers (uniform quantization) via post-processing "
    "before cosine similarity is computed.")
flags.DEFINE_bool(
    "use_coral", False,
    "If true, inference will be delegated to a connected Coral Edge TPU device."
)


def build_options():
  base_options = _BaseOptions(
      file_name=FLAGS.model_path, use_coral=FLAGS.use_coral)
  embedding_options = embedding_options_pb2.EmbeddingOptions(
      l2_normalize=FLAGS.l2_normalize, quantize=FLAGS.quantize)
  return image_embedder.ImageEmbedderOptions(
      base_options=base_options, embedding_options=embedding_options)


def main(_) -> None:
  # Creates embedder.
  options = build_options()
  embedder = image_embedder.ImageEmbedder.create_from_options(options)

  # Loads images.
  first_image = tensor_image.TensorImage.from_file(FLAGS.first_image_path)
  second_image = tensor_image.TensorImage.from_file(FLAGS.second_image_path)

  # Extracts both embeddings.
  first_result = embedder.embed(first_image)
  second_result = embedder.embed(second_image)

  # Gets consine similarity.
  cosine_similarity = embedder.cosine_similarity(
      first_result.embeddings[0].feature_vector,
      second_result.embeddings[0].feature_vector)
  print("The cosine similarity of %s and %s is %f" %
        (os.path.basename(FLAGS.first_image_path),
         os.path.basename(FLAGS.second_image_path), cosine_similarity))


if __name__ == "__main__":
  flags.mark_flag_as_required("model_path")
  flags.mark_flag_as_required("first_image_path")
  flags.mark_flag_as_required("second_image_path")
  app.run(main)
