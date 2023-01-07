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
r"""Generate python docs for tflite_support.

# How to run

```
pip install tflite-support absl-py pyyaml git+https://github.com/tensorflow/docs
python build_docs.py --output_dir=/path/to/output
```
"""

import pathlib

from absl import app
from absl import flags

from tensorflow_docs.api_generator import generate_lib
from tensorflow_docs.api_generator import toc_processing

import yaml

try:
  # pytype: disable=import-error # pylint: disable=g-import-not-at-top
  import tflite_support
  import tensorflow_lite_support
  # pytype: enable=import-error # pylint: enable=g-import-not-at-top
except ImportError as e:
  raise ImportError('Please `pip install tflite-support`.') from e

_OUT_DIR = flags.DEFINE_string('output_dir', '/tmp/support_api/',
                               'The path to output the files to')

_CODE_PREFIX = flags.DEFINE_string(
    'code_url_prefix',
    'https://github.com/tensorflow/tflite-support/blob/master'
    '/tensorflow_lite_support',
    'The url prefix for links to code.')

_SEARCH_HINTS = flags.DEFINE_bool(
    'search_hints', True,
    'Include metadata search hints in the generated files')

_SITE_PATH = flags.DEFINE_string('site_path', 'lite/api_docs/python',
                                 'Path prefix in the _toc.yaml')


def main(_):
  # The TF Lite support library is split over 2 installed packages,
  # tflite_support and tensorflow_lite_support. The former is the main
  # interface, but it imports from the latter, so we need to include it in the
  # doc scope.
  tflite_support_base_dir = pathlib.Path(tflite_support.__file__).parent
  tensorflow_lite_support_dir = pathlib.Path(
      tensorflow_lite_support.__file__).parent

  # Additionally, the tflite_support package is composed of smaller packages
  # that live in separate directories. To ensure "view code" URLs work, list
  # them explicitly alongside tensorflow_lite_support, which also lives
  # somewhere else.
  base_dirs = [
      tensorflow_lite_support_dir,
      tflite_support_base_dir / 'metadata_writers',
      tflite_support_base_dir / 'task']
  code_prefixes = [
      _CODE_PREFIX.value,
      f'{_CODE_PREFIX.value}/metadata/python/metadata_writers',
      f'{_CODE_PREFIX.value}/python/task']

  # schema_py_generated is a generated API, so we can't use annotations to
  # suppress doc generation.
  del tflite_support.schema_py_generated

  doc_generator = generate_lib.DocGenerator(
      root_title='TensorFlow Lite Support',
      py_modules=[('tflite_support', tflite_support)],
      base_dir=base_dirs,
      code_url_prefix=code_prefixes,
      search_hints=_SEARCH_HINTS.value,
      site_path=_SITE_PATH.value,
      callbacks=[])

  doc_generator.build(output_dir=_OUT_DIR.value)

  # Re-flow TOC to nest packages.
  yaml_path = pathlib.Path(_OUT_DIR.value) / 'tflite_support/_toc.yaml'
  yaml_toc = toc_processing.nest_toc(yaml.safe_load(yaml_path.read_text()))
  yaml_path.write_text(yaml.dump(yaml_toc))


if __name__ == '__main__':
  app.run(main)
