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
"""Python demo tool for BertQuestionAnswerer."""

import inspect
import os.path as _os_path
import subprocess
import sys

from absl import app
from absl import flags

FLAGS = flags.FLAGS
flags.DEFINE_string(
    'model_path', None,
    'Absolute path to the ".tflite" bert question answerer model.')
flags.DEFINE_string('question', None, 'Question to ask.')
flags.DEFINE_string('context', None,
                    'Context the asked question is based upon.')
flags.DEFINE_bool(
    'use_coral', False,
    'If true, inference will be delegated to a connected Coral Edge TPU device.'
)

# Required flag.
flags.mark_flag_as_required('model_path')
flags.mark_flag_as_required('question')
flags.mark_flag_as_required('context')

_BERT_QUESTION_ANSWERER_NATIVE_PATH = _os_path.join(
    _os_path.dirname(inspect.getfile(inspect.currentframe())),
    '../bert_question_answerer_demo')


def classify(model_path, question, context, use_coral):
  """Predicts the answers for the given question based on the given context

  Args:
      model_path: Path to model
      question: Question to ask
      context: Context the asked question is based upon
      use_coral: Optional; If true, inference will be delegated to a connected 
        Coral Edge TPU device.
  """
  # Run the detection tool:
  subprocess.run([
      _BERT_QUESTION_ANSWERER_NATIVE_PATH + ' --model_path=' + model_path +
      ' --question="' + question + '" --context="' + context +
      '" --use_coral=' + str(use_coral)
  ],
                 shell=True,
                 check=True)


def run_main(argv):
  del argv  # Unused.
  classify(FLAGS.model_path, FLAGS.question, FLAGS.context, FLAGS.use_coral)


# Simple wrapper to make the code pip-friendly
def main():
  app.run(main=run_main, argv=sys.argv)


if __name__ == '__main__':
  main()
