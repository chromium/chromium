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
"""Tests for MetadataWriter."""

import os
import tensorflow as tf

from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata.python import metadata as _metadata
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_writer
from tensorflow_lite_support.metadata.python.tests.metadata_writers import test_utils

_MODEL = "../testdata/mobilenet_v2_1.0_224_quant.tflite"
_MULTI_INPUTS_MODEL = "../testdata/question_answerer/mobilebert_float.tflite"
_MULTI_OUTPUTS_MODEL = "../testdata/audio_classifier/two_heads.tflite"
_MODEL_NAME = "mobilenet_v2_1.0_224_quant"
_INPUT_NAME = "image"
_OUTPUT_NAME = "probability"
_LABEL_FILE = tf.compat.v1.resource_loader.get_path_to_datafile(
    "../testdata/labels.txt"
)
_EXPECTED_DUMMY_JSON = "../testdata/mobilenet_v2_1.0_224_quant_dummy.json"
_EXPECTED_META_INFO_JSON = "../testdata/mobilenet_v2_1.0_224_quant_meta_info_.json"
_EXPECTED_DEFAULT_JSON = "../testdata/mobilenet_v2_1.0_224_quant_default.json"
# Before populated into the model, metadata does not have the verson string
_EXPECTED_DUMMY_NO_VERSION_JSON = "../testdata/mobilenet_v2_1.0_224_quant_dummy_no_version.json"
_EXPECTED_MULTI_INPUTS_JSON = "../testdata/multi_inputs.json"
_EXPECTED_MULTI_OUTPUTS_JSON = "../testdata/multi_outputs.json"


class MetadataWriterTest(tf.test.TestCase):

  def test_populate_from_metadata_should_succeed(self):
    model_buffer = test_utils.load_file(_MODEL)
    model_metadata, input_metadata, output_metadata = (
        self._create_dummy_metadata())

    writer = metadata_writer.MetadataWriter.create_from_metadata(
        model_buffer, model_metadata, [input_metadata], [output_metadata],
        [_LABEL_FILE])
    model_with_metadata = writer.populate()

    self._assert_correct_metadata(model_with_metadata, _EXPECTED_DUMMY_JSON,
                                  _LABEL_FILE)

  def test_create_from_metadata_with_default_value_should_succeed(self):
    model_buffer = test_utils.load_file(_MODEL)

    writer = metadata_writer.MetadataWriter.create_from_metadata(model_buffer)
    model_with_metadata = writer.populate()

    self._assert_correct_metadata(model_with_metadata, _EXPECTED_DEFAULT_JSON)

  def test_populate_create_from_metadata_info_should_succeed(self):
    model_buffer = test_utils.load_file(_MODEL)
    general_md = metadata_info.GeneralMd(name=_MODEL_NAME)
    input_md = metadata_info.TensorMd(name=_INPUT_NAME)
    output_md = metadata_info.TensorMd(name=_OUTPUT_NAME)

    writer = metadata_writer.MetadataWriter.create_from_metadata_info(
        model_buffer, general_md, [input_md], [output_md], [_LABEL_FILE])
    model_with_metadata = writer.populate()

    self._assert_correct_metadata(model_with_metadata, _EXPECTED_META_INFO_JSON,
                                  _LABEL_FILE)

  def test_create_from_metadata_info_with_default_value_should_succeed(self):
    model_buffer = test_utils.load_file(_MODEL)

    writer = metadata_writer.MetadataWriter.create_from_metadata_info(
        model_buffer)
    model_with_metadata = writer.populate()

    self._assert_correct_metadata(model_with_metadata, _EXPECTED_DEFAULT_JSON)

  def test_create_from_metadata_info_with_input_tensor_name_should_succeed(
      self):
    model_buffer = test_utils.load_file(_MULTI_INPUTS_MODEL)
    # The input tensors in the model are: input_ids, input_mask, segment_ids.
    input_md_1 = metadata_info.TensorMd(name="ids", tensor_name="input_ids")
    input_md_2 = metadata_info.TensorMd(name="mask", tensor_name="input_mask")
    input_md_3 = metadata_info.TensorMd(
        name="segment", tensor_name="segment_ids")

    # Create input metadata in a different order to test if MetadataWriter can
    # correct it.
    writer = metadata_writer.MetadataWriter.create_from_metadata_info(
        model_buffer, input_md=[input_md_2, input_md_3, input_md_1])
    model_with_metadata = writer.populate()

    self._assert_correct_metadata(model_with_metadata,
                                  _EXPECTED_MULTI_INPUTS_JSON)

  def test_create_from_metadata_info_fails_with_wrong_input_tesnor_name(self):
    model_buffer = test_utils.load_file(_MODEL)
    input_md = metadata_info.TensorMd(tensor_name="wrong_tensor_name")
    with self.assertRaises(ValueError) as error:
      metadata_writer.MetadataWriter.create_from_metadata_info(
          model_buffer, input_md=[input_md])
    self.assertEqual(
        "The tensor names from arguments (['wrong_tensor_name']) do not match"
        " the tensor names read from the model (['input']).",
        str(error.exception))

  def test_create_from_metadata_info_with_output_tensor_name_should_succeed(
      self):
    model_buffer = test_utils.load_file(_MULTI_OUTPUTS_MODEL)
    # The output tensors in the model are: Identity, Identity_1
    output_md_1 = metadata_info.TensorMd(
        name="Identity", tensor_name="Identity")
    output_md_2 = metadata_info.TensorMd(
        name="Identity 1", tensor_name="Identity_1")

    # Create output metadata in a different order to test if MetadataWriter can
    # correct it.
    writer = metadata_writer.MetadataWriter.create_from_metadata_info(
        model_buffer, output_md=[output_md_2, output_md_1])
    model_with_metadata = writer.populate()

    self._assert_correct_metadata(model_with_metadata,
                                  _EXPECTED_MULTI_OUTPUTS_JSON)

  def test_create_from_metadata_info_fails_with_wrong_output_tesnor_name(self):
    model_buffer = test_utils.load_file(_MODEL)
    output_md = metadata_info.TensorMd(tensor_name="wrong_tensor_name")
    with self.assertRaises(ValueError) as error:
      metadata_writer.MetadataWriter.create_from_metadata_info(
          model_buffer, output_md=[output_md])
    self.assertEqual(
        "The tensor names from arguments (['wrong_tensor_name']) do not match"
        " the tensor names read from the model (['output']).",
        str(error.exception))

  def test_get_metadata_json_should_succeed(self):
    model_buffer = test_utils.load_file(_MODEL)
    model_metadata, input_metadata, output_metadata = (
        self._create_dummy_metadata())

    writer = metadata_writer.MetadataWriter.create_from_metadata(
        model_buffer, model_metadata, [input_metadata], [output_metadata],
        [_LABEL_FILE])
    metadata_json = writer.get_metadata_json()

    expected_json = test_utils.load_file(_EXPECTED_DUMMY_NO_VERSION_JSON, "r")
    self.assertEqual(metadata_json, expected_json)

  def test_get_populated_metadata_json_should_succeed(self):
    model_buffer = test_utils.load_file(_MODEL)
    model_metadata, input_metadata, output_metadata = (
        self._create_dummy_metadata())

    writer = metadata_writer.MetadataWriter.create_from_metadata(
        model_buffer, model_metadata, [input_metadata], [output_metadata],
        [_LABEL_FILE])
    metadata_json = writer.get_populated_metadata_json()

    expected_json = test_utils.load_file(_EXPECTED_DUMMY_JSON, "r")
    self.assertEqual(metadata_json, expected_json)

  def _assert_correct_metadata(self,
                               model_with_metadata,
                               expected_json_file,
                               expected_label_file=None):
    # Verify if the metadata populated is correct.
    displayer = _metadata.MetadataDisplayer.with_model_buffer(
        model_with_metadata)
    metadata_json = displayer.get_metadata_json()
    expected_json = test_utils.load_file(expected_json_file, "r")
    self.assertEqual(metadata_json, expected_json)

    # Verify if the associated file is packed as expected.
    if expected_label_file:
      packed_files = displayer.get_packed_associated_file_list()
      expected_packed_files = [os.path.basename(expected_label_file)]
      self.assertEqual(set(packed_files), set(expected_packed_files))

  def _create_dummy_metadata(self):
    # Create dummy input metadata
    input_metadata = _metadata_fb.TensorMetadataT()
    input_metadata.name = _INPUT_NAME
    # Create dummy output metadata
    output_metadata = _metadata_fb.TensorMetadataT()
    output_metadata.name = _OUTPUT_NAME
    # Create dummy model_metadata
    model_metadata = _metadata_fb.ModelMetadataT()
    model_metadata.name = _MODEL_NAME
    return model_metadata, input_metadata, output_metadata


if __name__ == "__main__":
  tf.test.main()
