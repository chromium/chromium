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
"""Python demo tool for Audio Classification."""

import inspect
import os.path as _os_path
import subprocess
import sys

from absl import app
from absl import flags

FLAGS = flags.FLAGS
flags.DEFINE_string(
    'model_path', None,
    'Absolute path to the ".tflite" audio classification model.')
flags.DEFINE_string(
    'audio_wav_path', None,
    'Absolute path to the 16-bit PCM WAV file to classify. The WAV '
    'file must be monochannel and has a sampling rate matches the model '
    'expected sampling rate (as in the Metadata).  If the WAV file is '
    'longer than what the model requires, only the beginning section is '
    'used for inference.')
flags.DEFINE_float(
    'score_threshold', None,
    'Apply a filter on the results. Only display classes with score '
    'higher than the threshold.')
flags.DEFINE_bool(
    'use_coral', False,
    'If true, inference will be delegated to a connected Coral Edge TPU '
    'device.')

# Required flag.
flags.mark_flag_as_required('model_path')
flags.mark_flag_as_required('score_threshold')
flags.mark_flag_as_required('audio_wav_path')

_AUDIO_CLASSIFICATION_NATIVE_PATH = _os_path.join(
    _os_path.dirname(inspect.getfile(inspect.currentframe())),
    '../audio_classifier_demo')


def classify(model_path, score_threshold, audio_wav_path, use_coral):
  """Classifies input audio clip into different categories.

  Args:
      model_path: Path to model
      score_threshold: Absolute path to the 16-bit PCM WAV file to classify
      audio_wav_path: Apply a filter on the results
      use_coral: Optional; If true, inference will be delegated to a connected 
        Coral Edge TPU device.
  """
  # Run the classification tool:
  subprocess.run([
      _AUDIO_CLASSIFICATION_NATIVE_PATH + ' --model_path=' + model_path +
      ' --score_threshold=' + str(score_threshold) + ' --audio_wav_path="' +
      audio_wav_path + '" --use_coral=' + str(use_coral)
  ],
                 shell=True,
                 check=True)


def run_main(argv):
  del argv  # Unused.
  classify(FLAGS.model_path, FLAGS.score_threshold, FLAGS.audio_wav_path,
           FLAGS.use_coral)


# Simple wrapper to make the code pip-friendly
def main():
  app.run(main=run_main, argv=sys.argv)


if __name__ == '__main__':
  main()
