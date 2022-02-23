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
"""Writes metadata and label file to the NL classifier models."""

from typing import List, Optional

from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_writer
from tensorflow_lite_support.metadata.python.metadata_writers import writer_utils

_MODEL_NAME = "NLClassifier"
_MODEL_DESCRIPTION = ("Classify the input text into a set of known categories.")
_INPUT_NAME = "input_text"
_INPUT_DESCRIPTION = ("Embedding vectors representing the input text to be "
                      "classified.")
_OUTPUT_NAME = "probability"
_OUTPUT_DESCRIPTION = "Probabilities of the labels respectively."


class MetadataWriter(metadata_writer.MetadataWriter):
  """Writes metadata into the NL classifier."""

  @classmethod
  def create_from_metadata_info(
      cls,
      model_buffer: bytearray,
      general_md: Optional[metadata_info.GeneralMd] = None,
      input_md: Optional[metadata_info.InputTextTensorMd] = None,
      output_md: Optional[metadata_info.ClassificationTensorMd] = None):
    """Creates MetadataWriter based on general/input/output information.

    Args:
      model_buffer: valid buffer of the model file.
      general_md: general information about the model. If not specified, default
        general metadata will be generated.
      input_md: input text tensor information, if not specified, default input
        metadata will be generated.
      output_md: output classification tensor information, if not specified,
        default output metadata will be generated.

    Returns:
      A MetadataWriter object.
    """

    if general_md is None:
      general_md = metadata_info.GeneralMd(
          name=_MODEL_NAME, description=_MODEL_DESCRIPTION)

    if input_md is None:
      input_md = metadata_info.InputTextTensorMd(
          name=_INPUT_NAME, description=_INPUT_DESCRIPTION)

    if output_md is None:
      output_md = metadata_info.ClassificationTensorMd(
          name=_OUTPUT_NAME, description=_OUTPUT_DESCRIPTION)

    if output_md.associated_files is None:
      output_md.associated_files = []

    tokenizer_files = []
    if input_md.tokenizer_md:
      tokenizer_files = writer_utils.get_tokenizer_associated_files(
          input_md.tokenizer_md.create_metadata().options)

    return super().create_from_metadata_info(
        model_buffer=model_buffer,
        general_md=general_md,
        input_md=[input_md],
        output_md=[output_md],
        associated_files=[
            file.file_path for file in output_md.associated_files
        ] + tokenizer_files)

  @classmethod
  def create_for_inference(
      cls, model_buffer: bytearray,
      tokenizer_md: Optional[metadata_info.RegexTokenizerMd],
      label_file_paths: List[str]):
    """Creates mandatory metadata for TFLite Support inference.

    The parameters required in this method are mandatory when using TFLite
    Support features, such as Task library and Codegen tool (Android Studio ML
    Binding). Other metadata fields will be set to default. If other fields need
    to be filled, use the method `create_from_metadata_info` to edit them.

    Args:
      model_buffer: valid buffer of the model file.
      tokenizer_md: information of the tokenizer used to process the input
        string, if any. Only `RegexTokenizer` [1] is currently supported. If the
        tokenizer is `BertTokenizer` [2] or `SentencePieceTokenizer` [3], refer
        to `bert_nl_classifier.MetadataWriter`.
        [1]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L475
        [2]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L436
        [3]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L473
      label_file_paths: paths to the label files [4] in the classification
        tensor. Pass in an empty list if the model does not have any label
        file.
        [4]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L95

    Returns:
      A MetadataWriter object.
    """
    input_md = metadata_info.InputTextTensorMd(
        name=_INPUT_NAME,
        description=_INPUT_DESCRIPTION,
        tokenizer_md=tokenizer_md)

    output_md = metadata_info.ClassificationTensorMd(
        name=_OUTPUT_NAME,
        description=_OUTPUT_DESCRIPTION,
        label_files=[
            metadata_info.LabelFileMd(file_path=file_path)
            for file_path in label_file_paths
        ],
        tensor_type=writer_utils.get_output_tensor_types(model_buffer)[0])

    return cls.create_from_metadata_info(
        model_buffer, input_md=input_md, output_md=output_md)
