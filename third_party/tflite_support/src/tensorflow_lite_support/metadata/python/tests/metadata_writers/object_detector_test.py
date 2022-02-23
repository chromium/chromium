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
"""Tests for ObjectDetector.MetadataWriter."""

import json
import os
import tempfile

from absl.testing import parameterized
import tensorflow as tf

from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata.python import metadata
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import object_detector
from tensorflow_lite_support.metadata.python.tests.metadata_writers import test_utils

_PATH = "../testdata/object_detector/"
_MODEL = "../testdata/object_detector/ssd_mobilenet_v1.tflite"
_LABEL_FILE = "../testdata/object_detector/labelmap.txt"
_NORM_MEAN = 127.5
_NORM_STD = 127.5
_JSON_FOR_INFERENCE = "../testdata/object_detector/ssd_mobilenet_v1.json"
_JSON_DEFAULT = "../testdata/object_detector/ssd_mobilenet_v1_default.json"

_MODEL_COCO = "../testdata/object_detector/coco_ssd_mobilenet_v1_1.0_quant_2018_06_29_no_metadata.tflite"
_SCORE_CALIBRATION_FILE = "../testdata/object_detector/score_calibration.csv"
_SCORE_CALIBRATION_DEFAULT_SCORE = 0.2
_JSON_FOR_SCORE_CALIBRATION = "../testdata/object_detector/coco_ssd_mobilenet_v1_score_calibration.json"

_DUMMY_SCORE_CALIBRATION_FILE = "../testdata/object_detector/score_calibration_dummy.csv"
_DUMMY_SCORE_CALIBRATION_DEFAULT_SCORE = 0.0
_JSON_FOR_DUMMY_SCORE_CALIBRATION = "../testdata/object_detector/coco_ssd_mobilenet_v1_score_calibration_dummy.json"
_EXPECTED_DUMMY_MODEL = "../testdata/object_detector/coco_ssd_mobilenet_v1_1.0_quant_2018_06_29_score_calibration.tflite"


class MetadataWriterTest(tf.test.TestCase, parameterized.TestCase):

  def setUp(self):
    super().setUp()
    self._label_file = test_utils.get_resource_path(_LABEL_FILE)
    self._score_file = test_utils.get_resource_path(_SCORE_CALIBRATION_FILE)
    self._dummy_score_file = test_utils.get_resource_path(
        _DUMMY_SCORE_CALIBRATION_FILE)

  @parameterized.parameters(
      ("ssd_mobilenet_v1"),
      ("efficientdet_lite0_v1"),
  )
  def test_create_for_inference_should_succeed(self, model_name):
    model_path = os.path.join(_PATH, model_name + ".tflite")
    writer = object_detector.MetadataWriter.create_for_inference(
        test_utils.load_file(model_path), [_NORM_MEAN], [_NORM_STD],
        [self._label_file])

    json_path = os.path.join(_PATH, model_name + ".json")
    self._validate_metadata(writer, json_path)
    self._validate_populated_model(writer)

  @parameterized.parameters(
      ("ssd_mobilenet_v1"),
      ("efficientdet_lite0_v1"),
  )
  def test_create_from_metadata_info_by_default_should_succeed(
      self, model_name: str):
    model_path = os.path.join(_PATH, model_name + ".tflite")
    writer = object_detector.MetadataWriter.create_from_metadata_info(
        test_utils.load_file(model_path))
    json_path = os.path.join(_PATH, model_name + "_default.json")
    self._validate_metadata(writer, json_path)
    self._validate_populated_model(writer)

  def test_create_for_inference_score_calibration_should_succeed(self):
    score_calibration_md = metadata_info.ScoreCalibrationMd(
        _metadata_fb.ScoreTransformationType.INVERSE_LOGISTIC,
        _SCORE_CALIBRATION_DEFAULT_SCORE,
        self._score_file,
    )
    writer = object_detector.MetadataWriter.create_for_inference(
        test_utils.load_file(_MODEL_COCO), [_NORM_MEAN], [_NORM_STD],
        [self._label_file], score_calibration_md)
    self._validate_metadata(writer, _JSON_FOR_SCORE_CALIBRATION)
    self._validate_populated_model(writer)

  def test_create_for_inference_dummy_score_calibration_should_succeed(self):
    score_calibration_md = metadata_info.ScoreCalibrationMd(
        _metadata_fb.ScoreTransformationType.INVERSE_LOGISTIC,
        _DUMMY_SCORE_CALIBRATION_DEFAULT_SCORE,
        self._dummy_score_file,
    )
    writer = object_detector.MetadataWriter.create_for_inference(
        test_utils.load_file(_MODEL_COCO), [_NORM_MEAN], [_NORM_STD],
        [self._label_file], score_calibration_md)
    self._validate_metadata(writer, _JSON_FOR_DUMMY_SCORE_CALIBRATION)
    self._validate_populated_model(writer)

    # Test if populated model is equivalent to the expected model.
    metadata_dict = json.loads(writer.get_metadata_json())
    displayer = metadata.MetadataDisplayer.with_model_buffer(
        test_utils.load_file(_EXPECTED_DUMMY_MODEL))
    expected_metadata_dict = json.loads(displayer.get_metadata_json())
    self.assertDictContainsSubset(metadata_dict, expected_metadata_dict)

  def _validate_metadata(self, writer, expected_json_file):
    metadata_json = writer.get_metadata_json()
    expected_json = test_utils.load_file(expected_json_file, "r")
    self.assertEqual(metadata_json, expected_json)

  def _validate_populated_model(self, writer):
    with tempfile.NamedTemporaryFile() as tmp:
      with open(tmp.name, "wb") as f:
        f.write(writer.populate())
      self.assertGreater(os.path.getsize(tmp.name), 0)


if __name__ == "__main__":
  tf.test.main()
