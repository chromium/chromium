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
"""Python demo tool for Object Detection."""

import inspect
import os.path as _os_path
import subprocess
import sys

from absl import app
from absl import flags

FLAGS = flags.FLAGS

flags.DEFINE_string('model_path', None,
                    'Absolute path to the ".tflite" object detector model.')
flags.DEFINE_string(
    'image_path', None,
    'Absolute path to the image to run detection on. The image must be '
    'RGB or RGBA (grayscale is not supported). The image EXIF '
    'orientation flag, if any, is NOT taken into account.')
flags.DEFINE_string(
    'output_png', None,
    'Absolute path to a file where to draw the detection results on top '
    'of the input image. Must have a ".png" extension.')
flags.DEFINE_integer('max_results', 5,
                     'Maximum number of detection results to display.')
flags.DEFINE_float(
    'score_threshold', 0,
    'Detection results with a confidence score below this value are '
    'rejected. If specified, overrides the score threshold(s) provided in the '
    'TFLite Model Metadata. Ignored otherwise.')
flags.DEFINE_string(
    'class_name_whitelist', '',
    'Comma-separated list of class names that acts as a whitelist. If '
    'non-empty, detections results whose "class_name" is not in this list '
    'are filtered out. Mutually exclusive with "class_name_blacklist".')
flags.DEFINE_string(
    'class_name_blacklist', '',
    'Comma-separated list of class names that acts as a blacklist. If '
    'non-empty, detections results whose "class_name" is in this list '
    'are filtered out. Mutually exclusive with "class_name_whitelist".')
flags.DEFINE_bool(
    'use_coral', False,
    'If true, inference will be delegated to a connected Coral Edge TPU '
    'device.')

# Required flag.
flags.mark_flag_as_required('model_path')
flags.mark_flag_as_required('image_path')
flags.mark_flag_as_required('output_png')
flags.mark_flag_as_required('max_results')

_OBJECT_DETECTION_NATIVE_PATH = _os_path.join(
    _os_path.dirname(inspect.getfile(inspect.currentframe())),
    '../object_detector_demo')


def classify(model_path, image_path, output_png, max_results, score_threshold,
             class_name_whitelist, class_name_blacklist, use_coral):
  """Detects the input image.

  Args:
      model_path: Path to model
      image_path: Absolute path to the image to run detection on
      output_png: Absolute path to a file where to draw the detection results 
        on top of the input image
      max_results: Maximum number of detection results to display
      score_threshold: Optional; Detection results with a confidence score 
        below this value are rejected
      class_name_whitelist: Optional; Comma-separated list of class names 
        that acts as a whitelist.
      class_name_blacklist: Optional; Comma-separated list of class names 
        that acts as a blacklist.
      use_coral: Optional; If true, inference will be delegated to a 
        connected Coral Edge TPU device
  """
  # Run the detection tool:
  subprocess.run([
      _OBJECT_DETECTION_NATIVE_PATH + ' --model_path=' + model_path +
      ' --image_path=' + image_path + ' --output_png=' + output_png +
      ' --max_results=' + str(max_results) + ' --score_threshold=' +
      str(score_threshold) + ' --class_name_whitelist="' +
      str(class_name_whitelist) + '" --class_name_blacklist="' +
      str(class_name_blacklist) + '" --use_coral=' + str(use_coral)
  ],
                 shell=True,
                 check=True)


def run_main(argv):
  del argv  # Unused.
  classify(FLAGS.model_path, FLAGS.image_path, FLAGS.output_png,
           FLAGS.max_results, FLAGS.score_threshold, FLAGS.class_name_whitelist,
           FLAGS.class_name_blacklist, FLAGS.use_coral)


# Simple wrapper to make the code pip-friendly
def main():
  app.run(main=run_main, argv=sys.argv)


if __name__ == '__main__':
  main()
