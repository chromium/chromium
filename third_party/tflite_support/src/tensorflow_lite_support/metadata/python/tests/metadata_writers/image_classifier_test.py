# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
"""Tests for ImageClassifier.MetadataWriter."""

from absl.testing import parameterized

import tensorflow as tf

from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata.python.metadata_writers import image_classifier
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.tests.metadata_writers import test_utils

_FLOAT_MODEL = "../testdata/image_classifier/mobilenet_v2_1.0_224.tflite"
_QUANT_MODEL = "../testdata/image_classifier/mobilenet_v2_1.0_224_quant.tflite"
_LABEL_FILE = "../testdata/image_classifier/labels.txt"
_SCORE_CALIBRATION_FILE = "../testdata/image_classifier/score_calibration.txt"
_DEFAULT_SCORE_CALIBRATION_VALUE = 0.2
_NORM_MEAN = 127.5
_NORM_STD = 127.5
_FLOAT_JSON_FOR_INFERENCE = "../testdata/image_classifier/mobilenet_v2_1.0_224.json"
_FLOAT_JSON_DEFAULT = "../testdata/image_classifier/mobilenet_v2_1.0_224_default.json"
_QUANT_JSON_FOR_INFERENCE = "../testdata/image_classifier/mobilenet_v2_1.0_224_quant.json"
_JSON_DEFAULT = "../testdata/image_classifier/mobilenet_v2_1.0_224_default.json"


class MetadataWriterTest(tf.test.TestCase, parameterized.TestCase):

  @parameterized.named_parameters(
      {
          "testcase_name": "float_model",
          "model_file": _FLOAT_MODEL,
          "golden_json": _FLOAT_JSON_FOR_INFERENCE
      }, {
          "testcase_name": "quant_model",
          "model_file": _QUANT_MODEL,
          "golden_json": _QUANT_JSON_FOR_INFERENCE
      })
  def test_create_for_inference_should_succeed(self, model_file, golden_json):
    writer = image_classifier.MetadataWriter.create_for_inference(
        test_utils.load_file(model_file), [_NORM_MEAN], [_NORM_STD],
        [_LABEL_FILE],
        metadata_info.ScoreCalibrationMd(
            _metadata_fb.ScoreTransformationType.LOG,
            _DEFAULT_SCORE_CALIBRATION_VALUE,
            test_utils.get_resource_path(_SCORE_CALIBRATION_FILE)))

    metadata_json = writer.get_metadata_json()
    expected_json = test_utils.load_file(golden_json, "r")
    self.assertEqual(metadata_json, expected_json)

  @parameterized.named_parameters(
      {
          "testcase_name": "float_model",
          "model_file": _FLOAT_MODEL,
      }, {
          "testcase_name": "quant_model",
          "model_file": _QUANT_MODEL,
      })
  def test_create_from_metadata_info_by_default_should_succeed(
      self, model_file):
    writer = image_classifier.MetadataWriter.create_from_metadata_info(
        test_utils.load_file(model_file))

    metadata_json = writer.get_metadata_json()
    expected_json = test_utils.load_file(_JSON_DEFAULT, "r")
    self.assertEqual(metadata_json, expected_json)


if __name__ == "__main__":
  tf.test.main()
