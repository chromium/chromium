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
"""Python demo tool for NLClassifier."""

import inspect
import os.path as _os_path
import subprocess
import sys

from absl import app
from absl import flags

FLAGS = flags.FLAGS
flags.DEFINE_string('model_path', None, 'Model Path')
flags.DEFINE_string('text', None, 'Text to Predict')

# Required flag.
flags.mark_flag_as_required('model_path')
flags.mark_flag_as_required('text')

_NL_CLASSIFIER_NATIVE_PATH = _os_path.join(
    _os_path.dirname(inspect.getfile(inspect.currentframe())),
    '../nl_classifier_demo')


def classify(model_path, text):
  """Classifies input text into different categories.

  Args:
      model_path: path to model
      text: input text
  """
  # Run the detection tool:
  subprocess.run([
      _NL_CLASSIFIER_NATIVE_PATH + ' --model_path=' + model_path + ' --text="' +
      text + '"'
  ],
                 shell=True,
                 check=True)


def run_main(argv):
  del argv  # Unused.
  classify(FLAGS.model_path, FLAGS.text)


# Simple wrapper to make the code pip-friendly
def main():
  app.run(main=run_main, argv=sys.argv)


if __name__ == '__main__':
  main()
