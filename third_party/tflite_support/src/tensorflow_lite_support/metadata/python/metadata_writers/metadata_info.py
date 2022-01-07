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
"""Helper classes for common model metadata information."""

import collections
import csv
import os
from typing import List, Optional, Type, Union

from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata import schema_py_generated as _schema_fb
from tensorflow_lite_support.metadata.python.metadata_writers import writer_utils

# Min and max values for UINT8 tensors.
_MIN_UINT8 = 0
_MAX_UINT8 = 255

# Default description for vocabulary files.
_VOCAB_FILE_DESCRIPTION = ("Vocabulary file to convert natural language "
                           "words to embedding vectors.")


class GeneralMd:
  """A container for common metadata information of a model.

  Attributes:
    name: name of the model.
    version: version of the model.
    description: description of what the model does.
    author: author of the model.
    licenses: licenses of the model.
  """

  def __init__(self,
               name: Optional[str] = None,
               version: Optional[str] = None,
               description: Optional[str] = None,
               author: Optional[str] = None,
               licenses: Optional[str] = None):
    self.name = name
    self.version = version
    self.description = description
    self.author = author
    self.licenses = licenses

  def create_metadata(self) -> _metadata_fb.ModelMetadataT:
    """Creates the model metadata based on the general model information.

    Returns:
      A Flatbuffers Python object of the model metadata.
    """
    model_metadata = _metadata_fb.ModelMetadataT()
    model_metadata.name = self.name
    model_metadata.version = self.version
    model_metadata.description = self.description
    model_metadata.author = self.author
    model_metadata.license = self.licenses
    return model_metadata


class AssociatedFileMd:
  """A container for common associated file metadata information.

  Attributes:
    file_path: path to the associated file.
    description: description of the associated file.
    file_type: file type of the associated file [1].
    locale: locale of the associated file [2].
    [1]:
      https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L77
    [2]:
      https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L154
  """

  def __init__(
      self,
      file_path: str,
      description: Optional[str] = None,
      file_type: Optional[_metadata_fb.AssociatedFileType] = _metadata_fb
      .AssociatedFileType.UNKNOWN,
      locale: Optional[str] = None):
    self.file_path = file_path
    self.description = description
    self.file_type = file_type
    self.locale = locale

  def create_metadata(self) -> _metadata_fb.AssociatedFileT:
    """Creates the associated file metadata.

    Returns:
      A Flatbuffers Python object of the associated file metadata.
    """
    file_metadata = _metadata_fb.AssociatedFileT()
    file_metadata.name = os.path.basename(self.file_path)
    file_metadata.description = self.description
    file_metadata.type = self.file_type
    file_metadata.locale = self.locale
    return file_metadata


class LabelFileMd(AssociatedFileMd):
  """A container for label file metadata information."""

  _LABEL_FILE_DESCRIPTION = ("Labels for categories that the model can "
                             "recognize.")
  _FILE_TYPE = _metadata_fb.AssociatedFileType.TENSOR_AXIS_LABELS

  def __init__(self, file_path: str, locale: Optional[str] = None):
    """Creates a LabelFileMd object.

    Args:
      file_path: file_path of the label file.
      locale: locale of the label file [1].
      [1]:
      https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L154
    """
    super().__init__(file_path, self._LABEL_FILE_DESCRIPTION, self._FILE_TYPE,
                     locale)


class RegexTokenizerMd:
  """A container for the Regex tokenizer [1] metadata information.

  [1]:
    https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L459
  """

  def __init__(self, delim_regex_pattern: str, vocab_file_path: str):
    """Initializes a RegexTokenizerMd object.

    Args:
      delim_regex_pattern: the regular expression to segment strings and create
        tokens.
      vocab_file_path: path to the vocabulary file.
    """
    self._delim_regex_pattern = delim_regex_pattern
    self._vocab_file_path = vocab_file_path

  def create_metadata(self) -> _metadata_fb.ProcessUnitT:
    """Creates the Bert tokenizer metadata based on the information.

    Returns:
      A Flatbuffers Python object of the Bert tokenizer metadata.
    """
    vocab = _metadata_fb.AssociatedFileT()
    vocab.name = self._vocab_file_path
    vocab.description = _VOCAB_FILE_DESCRIPTION
    vocab.type = _metadata_fb.AssociatedFileType.VOCABULARY

    # Create the RegexTokenizer.
    tokenizer = _metadata_fb.ProcessUnitT()
    tokenizer.optionsType = (
        _metadata_fb.ProcessUnitOptions.RegexTokenizerOptions)
    tokenizer.options = _metadata_fb.RegexTokenizerOptionsT()
    tokenizer.options.delimRegexPattern = self._delim_regex_pattern
    tokenizer.options.vocabFile = [vocab]
    return tokenizer


class BertTokenizerMd:
  """A container for the Bert tokenizer [1] metadata information.

  [1]:
    https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L436
  """

  def __init__(self, vocab_file_path: str):
    """Initializes a BertTokenizerMd object.

    Args:
      vocab_file_path: path to the vocabulary file.
    """
    self._vocab_file_path = vocab_file_path

  def create_metadata(self) -> _metadata_fb.ProcessUnitT:
    """Creates the Bert tokenizer metadata based on the information.

    Returns:
      A Flatbuffers Python object of the Bert tokenizer metadata.
    """
    vocab = _metadata_fb.AssociatedFileT()
    vocab.name = self._vocab_file_path
    vocab.description = _VOCAB_FILE_DESCRIPTION
    vocab.type = _metadata_fb.AssociatedFileType.VOCABULARY
    tokenizer = _metadata_fb.ProcessUnitT()
    tokenizer.optionsType = _metadata_fb.ProcessUnitOptions.BertTokenizerOptions
    tokenizer.options = _metadata_fb.BertTokenizerOptionsT()
    tokenizer.options.vocabFile = [vocab]
    return tokenizer


class SentencePieceTokenizerMd:
  """A container for the sentence piece tokenizer [1] metadata information.

  [1]:
    https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L473
  """

  _SP_MODEL_DESCRIPTION = "The sentence piece model file."
  _SP_VOCAB_FILE_DESCRIPTION = _VOCAB_FILE_DESCRIPTION + (
      " This file is optional during tokenization, while the sentence piece "
      "model is mandatory.")

  def __init__(self,
               sentence_piece_model_path: str,
               vocab_file_path: Optional[str] = None):
    """Initializes a SentencePieceTokenizerMd object.

    Args:
      sentence_piece_model_path: path to the sentence piece model file.
      vocab_file_path: path to the vocabulary file.
    """
    self._sentence_piece_model_path = sentence_piece_model_path
    self._vocab_file_path = vocab_file_path

  def create_metadata(self) -> _metadata_fb.ProcessUnitT:
    """Creates the sentence piece tokenizer metadata based on the information.

    Returns:
      A Flatbuffers Python object of the sentence piece tokenizer metadata.
    """
    tokenizer = _metadata_fb.ProcessUnitT()
    tokenizer.optionsType = (
        _metadata_fb.ProcessUnitOptions.SentencePieceTokenizerOptions)
    tokenizer.options = _metadata_fb.SentencePieceTokenizerOptionsT()

    sp_model = _metadata_fb.AssociatedFileT()
    sp_model.name = self._sentence_piece_model_path
    sp_model.description = self._SP_MODEL_DESCRIPTION
    tokenizer.options.sentencePieceModel = [sp_model]
    if self._vocab_file_path:
      vocab = _metadata_fb.AssociatedFileT()
      vocab.name = self._vocab_file_path
      vocab.description = self._SP_VOCAB_FILE_DESCRIPTION
      vocab.type = _metadata_fb.AssociatedFileType.VOCABULARY
      tokenizer.options.vocabFile = [vocab]
    return tokenizer


class ScoreCalibrationMd:
  """A container for score calibration [1] metadata information.

  [1]:
    https://github.com/tensorflow/tflite-support/blob/5e0cdf5460788c481f5cd18aab8728ec36cf9733/tensorflow_lite_support/metadata/metadata_schema.fbs#L434
  """

  _SCORE_CALIBRATION_FILE_DESCRIPTION = (
      "Contains sigmoid-based score calibration parameters. The main purposes "
      "of score calibration is to make scores across classes comparable, so "
      "that a common threshold can be used for all output classes.")
  _FILE_TYPE = _metadata_fb.AssociatedFileType.TENSOR_AXIS_SCORE_CALIBRATION

  def __init__(self,
               score_transformation_type: _metadata_fb.ScoreTransformationType,
               default_score: float, file_path: str):
    """Creates a ScoreCalibrationMd object.

    Args:
      score_transformation_type: type of the function used for transforming the
        uncalibrated score before applying score calibration.
      default_score: the default calibrated score to apply if the uncalibrated
        score is below min_score or if no parameters were specified for a given
        index.
      file_path: file_path of the score calibration file [1].
      [1]:
        https://github.com/tensorflow/tflite-support/blob/5e0cdf5460788c481f5cd18aab8728ec36cf9733/tensorflow_lite_support/metadata/metadata_schema.fbs#L122

    Raises:
      ValueError: if the score_calibration file is malformed.
    """
    self._score_transformation_type = score_transformation_type
    self._default_score = default_score
    self._file_path = file_path

    # Sanity check the score calibration file.
    with open(self._file_path) as calibration_file:
      csv_reader = csv.reader(calibration_file, delimiter=",")
      for row in csv_reader:
        if row and len(row) != 3 and len(row) != 4:
          raise ValueError(
              f"Expected empty lines or 3 or 4 parameters per line in score"
              f" calibration file, but got {len(row)}.")

        if row and float(row[0]) < 0:
          raise ValueError(
              f"Expected scale to be a non-negative value, but got "
              f"{float(row[0])}.")

  def create_metadata(self) -> _metadata_fb.ProcessUnitT:
    """Creates the score calibration metadata based on the information.

    Returns:
      A Flatbuffers Python object of the score calibration metadata.
    """
    score_calibration = _metadata_fb.ProcessUnitT()
    score_calibration.optionsType = (
        _metadata_fb.ProcessUnitOptions.ScoreCalibrationOptions)
    options = _metadata_fb.ScoreCalibrationOptionsT()
    options.scoreTransformation = self._score_transformation_type
    options.defaultScore = self._default_score
    score_calibration.options = options
    return score_calibration

  def create_score_calibration_file_md(self) -> AssociatedFileMd:
    return AssociatedFileMd(self._file_path,
                            self._SCORE_CALIBRATION_FILE_DESCRIPTION,
                            self._FILE_TYPE)


class TensorMd:
  """A container for common tensor metadata information.

  Attributes:
    name: name of the tensor.
    description: description of what the tensor is.
    min_values: per-channel minimum value of the tensor.
    max_values: per-channel maximum value of the tensor.
    content_type: content_type of the tensor.
    associated_files: information of the associated files in the tensor.
    tensor_name: name of the corresponding tensor [1] in the TFLite model. It is
      used to locate the corresponding tensor and decide the order of the tensor
      metadata [2] when populating model metadata.
    [1]:
      https://github.com/tensorflow/tensorflow/blob/cb67fef35567298b40ac166b0581cd8ad68e5a3a/tensorflow/lite/schema/schema.fbs#L1129-L1136
    [2]:
      https://github.com/tensorflow/tflite-support/blob/b2a509716a2d71dfff706468680a729cc1604cff/tensorflow_lite_support/metadata/metadata_schema.fbs#L595-L612
  """

  def __init__(self,
               name: Optional[str] = None,
               description: Optional[str] = None,
               min_values: Optional[List[float]] = None,
               max_values: Optional[List[float]] = None,
               content_type: _metadata_fb.ContentProperties = _metadata_fb
               .ContentProperties.FeatureProperties,
               associated_files: Optional[List[Type[AssociatedFileMd]]] = None,
               tensor_name: Optional[str] = None):
    self.name = name
    self.description = description
    self.min_values = min_values
    self.max_values = max_values
    self.content_type = content_type
    self.associated_files = associated_files
    self.tensor_name = tensor_name

  def create_metadata(self) -> _metadata_fb.TensorMetadataT:
    """Creates the input tensor metadata based on the information.

    Returns:
      A Flatbuffers Python object of the input metadata.
    """
    tensor_metadata = _metadata_fb.TensorMetadataT()
    tensor_metadata.name = self.name
    tensor_metadata.description = self.description

    # Create min and max values
    stats = _metadata_fb.StatsT()
    stats.max = self.max_values
    stats.min = self.min_values
    tensor_metadata.stats = stats

    # Create content properties
    content = _metadata_fb.ContentT()
    if self.content_type is _metadata_fb.ContentProperties.FeatureProperties:
      content.contentProperties = _metadata_fb.FeaturePropertiesT()
    elif self.content_type is _metadata_fb.ContentProperties.ImageProperties:
      content.contentProperties = _metadata_fb.ImagePropertiesT()
    elif self.content_type is (
        _metadata_fb.ContentProperties.BoundingBoxProperties):
      content.contentProperties = _metadata_fb.BoundingBoxPropertiesT()
    elif self.content_type is _metadata_fb.ContentProperties.AudioProperties:
      content.contentProperties = _metadata_fb.AudioPropertiesT()

    content.contentPropertiesType = self.content_type
    tensor_metadata.content = content

    # TODO(b/174091474): check if multiple label files have populated locale.
    # Create associated files
    if self.associated_files:
      tensor_metadata.associatedFiles = [
          file.create_metadata() for file in self.associated_files
      ]
    return tensor_metadata


class InputImageTensorMd(TensorMd):
  """A container for input image tensor metadata information.

  Attributes:
    norm_mean: the mean value used in tensor normalization [1].
    norm_std: the std value used in the tensor normalization [1]. norm_mean and
      norm_std must have the same dimension.
    color_space_type: the color space type of the input image [2].
    [1]:
      https://www.tensorflow.org/lite/convert/metadata#normalization_and_quantization_parameters
    [2]:
      https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L172
  """

  # Min and max float values for image pixels.
  _MIN_PIXEL = 0.0
  _MAX_PIXEL = 255.0

  def __init__(
      self,
      name: Optional[str] = None,
      description: Optional[str] = None,
      norm_mean: Optional[List[float]] = None,
      norm_std: Optional[List[float]] = None,
      color_space_type: Optional[
          _metadata_fb.ColorSpaceType] = _metadata_fb.ColorSpaceType.UNKNOWN,
      tensor_type: Optional[_schema_fb.TensorType] = None):
    """Initializes the instance of InputImageTensorMd.

    Args:
      name: name of the tensor.
      description: description of what the tensor is.
      norm_mean: the mean value used in tensor normalization [1].
      norm_std: the std value used in the tensor normalization [1]. norm_mean
        and norm_std must have the same dimension.
      color_space_type: the color space type of the input image [2].
      tensor_type: data type of the tensor.
      [1]:
        https://www.tensorflow.org/lite/convert/metadata#normalization_and_quantization_parameters
      [2]:
      https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L172

    Raises:
      ValueError: if norm_mean and norm_std have different dimensions.
    """
    if norm_std and norm_mean and len(norm_std) != len(norm_mean):
      raise ValueError(
          f"norm_mean and norm_std are expected to be the same dim. But got "
          f"{len(norm_mean)} and {len(norm_std)}")

    if tensor_type is _schema_fb.TensorType.UINT8:
      min_values = [_MIN_UINT8]
      max_values = [_MAX_UINT8]
    elif tensor_type is _schema_fb.TensorType.FLOAT32 and norm_std and norm_mean:
      min_values = [
          float(self._MIN_PIXEL - mean) / std
          for mean, std in zip(norm_mean, norm_std)
      ]
      max_values = [
          float(self._MAX_PIXEL - mean) / std
          for mean, std in zip(norm_mean, norm_std)
      ]
    else:
      # Uint8 and Float32 are the two major types currently. And Task library
      # doesn't support other types so far.
      min_values = None
      max_values = None

    super().__init__(name, description, min_values, max_values,
                     _metadata_fb.ContentProperties.ImageProperties)
    self.norm_mean = norm_mean
    self.norm_std = norm_std
    self.color_space_type = color_space_type

  def create_metadata(self) -> _metadata_fb.TensorMetadataT:
    """Creates the input image metadata based on the information.

    Returns:
      A Flatbuffers Python object of the input image metadata.
    """
    tensor_metadata = super().create_metadata()
    tensor_metadata.content.contentProperties.colorSpace = self.color_space_type
    # Create normalization parameters
    if self.norm_mean and self.norm_std:
      normalization = _metadata_fb.ProcessUnitT()
      normalization.optionsType = (
          _metadata_fb.ProcessUnitOptions.NormalizationOptions)
      normalization.options = _metadata_fb.NormalizationOptionsT()
      normalization.options.mean = self.norm_mean
      normalization.options.std = self.norm_std
      tensor_metadata.processUnits = [normalization]
    return tensor_metadata


class InputTextTensorMd(TensorMd):
  """A container for the input text tensor metadata information.

  Attributes:
    tokenizer_md: information of the tokenizer in the input text tensor, if any.
  """

  def __init__(self,
               name: Optional[str] = None,
               description: Optional[str] = None,
               tokenizer_md: Optional[RegexTokenizerMd] = None):
    """Initializes the instance of InputTextTensorMd.

    Args:
      name: name of the tensor.
      description: description of what the tensor is.
      tokenizer_md: information of the tokenizer in the input text tensor, if
        any. Only `RegexTokenizer` [1] is currenly supported. If the tokenizer
        is `BertTokenizer` [2] or `SentencePieceTokenizer` [3], refer to
        `bert_nl_classifier.MetadataWriter`.
        [1]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L475
        [2]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L436
        [3]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L473
    """
    super().__init__(name, description)
    self.tokenizer_md = tokenizer_md

  def create_metadata(self) -> _metadata_fb.TensorMetadataT:
    """Creates the input text metadata based on the information.

    Returns:
      A Flatbuffers Python object of the input text metadata.

    Raises:
      ValueError: if the type of tokenizer_md is unsupported.
    """
    if not isinstance(self.tokenizer_md, (type(None), RegexTokenizerMd)):
      raise ValueError(
          f"The type of tokenizer_options, {type(self.tokenizer_md)}, is "
          f"unsupported")

    tensor_metadata = super().create_metadata()
    if self.tokenizer_md:
      tensor_metadata.processUnits = [self.tokenizer_md.create_metadata()]
    return tensor_metadata


class InputAudioTensorMd(TensorMd):
  """A container for the input audio tensor metadata information.

  Attributes:
    sample_rate: the sample rate in Hz when the audio was captured.
    channels: the channel count of the audio.
  """

  def __init__(self,
               name: Optional[str] = None,
               description: Optional[str] = None,
               sample_rate: int = 0,
               channels: int = 0):
    """Initializes the instance of InputAudioTensorMd.

    Args:
      name: name of the tensor.
      description: description of what the tensor is.
      sample_rate: the sample rate in Hz when the audio was captured.
      channels: the channel count of the audio.
    """
    super().__init__(
        name,
        description,
        content_type=_metadata_fb.ContentProperties.AudioProperties)

    self.sample_rate = sample_rate
    self.channels = channels

  def create_metadata(self) -> _metadata_fb.TensorMetadataT:
    """Creates the input audio metadata based on the information.

    Returns:
      A Flatbuffers Python object of the input audio metadata.

    Raises:
      ValueError: if any value of sample_rate, channels is negative.
    """
    # 0 is the default value in Flatbuffers.
    if self.sample_rate < 0:
      raise ValueError(
          f"sample_rate should be non-negative, but got {self.sample_rate}.")

    if self.channels < 0:
      raise ValueError(
          f"channels should be non-negative, but got {self.channels}.")

    tensor_metadata = super().create_metadata()
    properties = tensor_metadata.content.contentProperties
    properties.sampleRate = self.sample_rate
    properties.channels = self.channels

    return tensor_metadata


class ClassificationTensorMd(TensorMd):
  """A container for the classification tensor metadata information.

  Attributes:
    label_files: information of the label files [1] in the classification
      tensor.
    score_calibration_md: information of the score calibration operation [2] in
      the classification tensor.
    [1]:
      https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L95
    [2]:
      https://github.com/tensorflow/tflite-support/blob/5e0cdf5460788c481f5cd18aab8728ec36cf9733/tensorflow_lite_support/metadata/metadata_schema.fbs#L434
  """

  # Min and max float values for classification results.
  _MIN_FLOAT = 0.0
  _MAX_FLOAT = 1.0

  def __init__(self,
               name: Optional[str] = None,
               description: Optional[str] = None,
               label_files: Optional[List[LabelFileMd]] = None,
               tensor_type: Optional[_schema_fb.TensorType] = None,
               score_calibration_md: Optional[ScoreCalibrationMd] = None,
               tensor_name: Optional[str] = None):
    """Initializes the instance of ClassificationTensorMd.

    Args:
      name: name of the tensor.
      description: description of what the tensor is.
      label_files: information of the label files [1] in the classification
        tensor.
      tensor_type: data type of the tensor.
      score_calibration_md: information of the score calibration files operation
        [2] in the classification tensor.
      tensor_name: name of the corresponding tensor [3] in the TFLite model. It
        is used to locate the corresponding classification tensor and decide the
        order of the tensor metadata [4] when populating model metadata.
      [1]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L95
      [2]:
        https://github.com/tensorflow/tflite-support/blob/5e0cdf5460788c481f5cd18aab8728ec36cf9733/tensorflow_lite_support/metadata/metadata_schema.fbs#L434
      [3]:
        https://github.com/tensorflow/tensorflow/blob/cb67fef35567298b40ac166b0581cd8ad68e5a3a/tensorflow/lite/schema/schema.fbs#L1129-L1136
      [4]:
        https://github.com/tensorflow/tflite-support/blob/b2a509716a2d71dfff706468680a729cc1604cff/tensorflow_lite_support/metadata/metadata_schema.fbs#L595-L612
    """
    self.score_calibration_md = score_calibration_md

    if tensor_type is _schema_fb.TensorType.UINT8:
      min_values = [_MIN_UINT8]
      max_values = [_MAX_UINT8]
    elif tensor_type is _schema_fb.TensorType.FLOAT32:
      min_values = [self._MIN_FLOAT]
      max_values = [self._MAX_FLOAT]
    else:
      # Uint8 and Float32 are the two major types currently. And Task library
      # doesn't support other types so far.
      min_values = None
      max_values = None

    associated_files = label_files or []
    if self.score_calibration_md:
      associated_files.append(
          score_calibration_md.create_score_calibration_file_md())

    super().__init__(name, description, min_values, max_values,
                     _metadata_fb.ContentProperties.FeatureProperties,
                     associated_files, tensor_name)

  def create_metadata(self) -> _metadata_fb.TensorMetadataT:
    """Creates the classification tensor metadata based on the information."""
    tensor_metadata = super().create_metadata()
    if self.score_calibration_md:
      tensor_metadata.processUnits = [
          self.score_calibration_md.create_metadata()
      ]
    return tensor_metadata


class CategoryTensorMd(TensorMd):
  """A container for the category tensor metadata information."""

  def __init__(self,
               name: Optional[str] = None,
               description: Optional[str] = None,
               label_files: Optional[List[LabelFileMd]] = None):
    """Initializes a CategoryTensorMd object.

    Args:
      name: name of the tensor.
      description: description of what the tensor is.
      label_files: information of the label files [1] in the category tensor.
      [1]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L108
    """
    # In category tensors, label files are in the type of TENSOR_VALUE_LABELS.
    value_label_files = label_files
    if value_label_files:
      for file in value_label_files:
        file.file_type = _metadata_fb.AssociatedFileType.TENSOR_VALUE_LABELS

    super().__init__(
        name=name, description=description, associated_files=value_label_files)


class BertInputTensorsMd:
  """A container for the input tensor metadata information of Bert models."""

  _IDS_NAME = "ids"
  _IDS_DESCRIPTION = "Tokenized ids of the input text."
  _MASK_NAME = "mask"
  _MASK_DESCRIPTION = ("Mask with 1 for real tokens and 0 for padding "
                       "tokens.")
  _SEGMENT_IDS_NAME = "segment_ids"
  _SEGMENT_IDS_DESCRIPTION = (
      "0 for the first sequence, 1 for the second sequence if exists.")

  def __init__(self,
               model_buffer: bytearray,
               ids_name: str,
               mask_name: str,
               segment_name: str,
               ids_md: Optional[TensorMd] = None,
               mask_md: Optional[TensorMd] = None,
               segment_ids_md: Optional[TensorMd] = None,
               tokenizer_md: Union[None, BertTokenizerMd,
                                   SentencePieceTokenizerMd] = None):
    """Initializes a BertInputTensorsMd object.

    `ids_name`, `mask_name`, and `segment_name` correspond to the `Tensor.name`
    in the TFLite schema, which help to determine the tensor order when
    populating metadata.

    Args:
      model_buffer: valid buffer of the model file.
      ids_name: name of the ids tensor, which represents the tokenized ids of
        the input text.
      mask_name: name of the mask tensor, which represents the mask with 1 for
        real tokens and 0 for padding tokens.
      segment_name: name of the segment ids tensor, where `0` stands for the
        first sequence, and `1` stands for the second sequence if exists.
      ids_md: input ids tensor informaton.
      mask_md: input mask tensor informaton.
      segment_ids_md: input segment tensor informaton.
      tokenizer_md: information of the tokenizer used to process the input
        string, if any. Supported tokenziers are: `BertTokenizer` [1] and
          `SentencePieceTokenizer` [2]. If the tokenizer is `RegexTokenizer`
          [3], refer to `nl_classifier.MetadataWriter`.
        [1]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L436
        [2]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L473
        [3]:
        https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L475
    """

    self._input_names = [ids_name, mask_name, segment_name]

    # Get the input tensor names in order from the model. Later, we need to
    # order the input metadata according to this tensor order.
    self._ordered_input_names = writer_utils.get_input_tensor_names(
        model_buffer)

    # Verify that self._ordered_input_names (read from the model) and
    # self._input_name (collected from users) are aligned.
    if collections.Counter(self._ordered_input_names) != collections.Counter(
        self._input_names):
      raise ValueError(
          f"The input tensor names ({self._ordered_input_names}) do not match "
          f"the tensor names read from the model ({self._input_names}).")

    if ids_md is None:
      ids_md = TensorMd(name=self._IDS_NAME, description=self._IDS_DESCRIPTION)

    if mask_md is None:
      mask_md = TensorMd(
          name=self._MASK_NAME, description=self._MASK_DESCRIPTION)

    if segment_ids_md is None:
      segment_ids_md = TensorMd(
          name=self._SEGMENT_IDS_NAME,
          description=self._SEGMENT_IDS_DESCRIPTION)

    # The order of self._input_md matches the order of self._input_names.
    self._input_md = [ids_md, mask_md, segment_ids_md]

    if not isinstance(tokenizer_md,
                      (type(None), BertTokenizerMd, SentencePieceTokenizerMd)):
      raise ValueError(
          f"The type of tokenizer_options, {type(tokenizer_md)}, is unsupported"
      )

    self._tokenizer_md = tokenizer_md

  def create_input_tesnor_metadata(self) -> List[_metadata_fb.TensorMetadataT]:
    """Creates the input metadata for the three input tesnors."""
    # The order of the three input tensors may vary with each model conversion.
    # We need to order the input metadata according to the tensor order in the
    # model.
    ordered_metadata = []
    name_md_dict = dict(zip(self._input_names, self._input_md))
    for name in self._ordered_input_names:
      ordered_metadata.append(name_md_dict[name].create_metadata())
    return ordered_metadata

  def create_input_process_unit_metadata(
      self) -> List[_metadata_fb.ProcessUnitT]:
    """Creates the input process unit metadata."""
    if self._tokenizer_md:
      return [self._tokenizer_md.create_metadata()]
    else:
      return []

  def get_tokenizer_associated_files(self) -> List[str]:
    """Gets the associated files that are packed in the tokenizer."""
    if self._tokenizer_md:
      return writer_utils.get_tokenizer_associated_files(
          self._tokenizer_md.create_metadata().options)
    else:
      return []
