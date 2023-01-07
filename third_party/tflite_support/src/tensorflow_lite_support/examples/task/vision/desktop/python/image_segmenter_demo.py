#!/usr/bin/env python3
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
# ==============================================================================
"""Python demo tool for Image Segmentation."""

import inspect
import os.path as _os_path
import subprocess
import sys

from absl import app
from absl import flags

FLAGS = flags.FLAGS

flags.DEFINE_string('model_path', None,
                    'Absolute path to the ".tflite" image segmenter model.')
flags.DEFINE_string(
    'image_path', None,
    'Absolute path to the image to segment. The image must be RGB or '
    'RGBA (grayscale is not supported). The image EXIF orientation '
    'flag, if any, is NOT taken into account.')
flags.DEFINE_string(
    'output_mask_png', None,
    'Absolute path to the output category mask (confidence masks outputs '
    'are not supported by this tool). Must have a ".png" extension.')
flags.DEFINE_bool(
    'use_coral', False,
    'If true, inference will be delegated to a connected Coral Edge TPU '
    'device.')
# Required flag.
flags.mark_flag_as_required('model_path')
flags.mark_flag_as_required('image_path')
flags.mark_flag_as_required('output_mask_png')

_IMAGE_SEGMENTATION_NATIVE_PATH = _os_path.join(
    _os_path.dirname(inspect.getfile(inspect.currentframe())),
    '../image_segmenter_demo')


def classify(model_path, image_path, output_mask_png, use_coral):
  """Segments the input image.

  Args:
      model_path: Path to model
      image_path: Absolute path to the image to segment
      output_mask_png: Absolute path to the output category mask (confidence 
        masks outputs are not supported by this tool
      use_coral: Optional; If true, inference will be delegated to a connected 
        Coral Edge TPU device
  """
  # Run the segmentation tool:
  subprocess.run([
      _IMAGE_SEGMENTATION_NATIVE_PATH + ' --model_path=' + model_path +
      ' --image_path=' + image_path + ' --output_mask_png=' + output_mask_png +
      ' --use_coral=' + str(use_coral)
  ],
                 shell=True,
                 check=True)


def run_main(argv):
  del argv  # Unused.
  classify(FLAGS.model_path, FLAGS.image_path, FLAGS.output_mask_png,
           FLAGS.use_coral)


# Simple wrapper to make the code pip-friendly
def main():
  app.run(main=run_main, argv=sys.argv)


if __name__ == '__main__':
  main()
