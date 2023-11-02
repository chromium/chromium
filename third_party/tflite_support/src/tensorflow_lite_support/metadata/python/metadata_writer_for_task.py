# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Object oriented generic metadata writer for modular task API."""

import collections
import os
import tempfile
from typing import Optional, List

from tensorflow_lite_support.metadata import metadata_schema_py_generated as _metadata_fb
from tensorflow_lite_support.metadata.python import metadata as _metadata
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_writer
from tensorflow_lite_support.metadata.python.metadata_writers import writer_utils

CalibrationParameter = collections.namedtuple(
    'CalibrationParameter', ['scale', 'slope', 'offset', 'min_score'])

LabelItem = collections.namedtuple('LabelItem', ['locale', 'filename', 'names'])


class Labels:
  """Simple container holding classification labels of a particular tensor."""

  def __init__(self):
    self._labels = []  # [LabelItem]

  def add(self,
          labels: List[str],
          locale: Optional[str] = None,
          use_as_category_name=False,
          exported_filename: Optional[str] = None):
    """Adds labels in the container."""
    if not labels:
      raise ValueError('The list of labels is empty')

    # Prepare the new item to be inserted
    if not exported_filename:
      exported_filename = 'labels'
      if locale:
        exported_filename += f'_{locale}'
      exported_filename += '.txt'
    item = LabelItem(locale, exported_filename, labels)

    if self._labels and use_as_category_name:
      # Category names should be the first one in the list
      pos = 0

      # Double check if we need to replace exising category name or insert one.
      if self._labels[pos].locale:
        # No category names available, insert one
        self._labels.insert(pos, item)
      else:
        # Update the category name
        self._labels[pos] = item

    else:
      # insert the new element at the end of the list
      self._labels.append(item)
    return self

  def add_from_file(self,
                    label_filepath: str,
                    locale: Optional[str] = None,
                    use_as_category_name=False,
                    exported_filename: Optional[str] = None):
    """Adds a label file in the container."""
    with open(label_filepath, 'r') as f:
      labels = f.read().split('\n')
      return self.add(labels, locale, use_as_category_name, exported_filename)


class ScoreCalibration:
  """Simple container holding score calibration related parameters."""

  # A shortcut to avoid client side code importing _metadata_fb
  transformation_types = _metadata_fb.ScoreTransformationType

  def __init__(self,
               transformation_type: _metadata_fb.ScoreTransformationType,
               parameters: List[CalibrationParameter],
               default_score: int = 0):
    self.transformation_type = transformation_type
    self.parameters = parameters
    self.default_score = default_score


class Writer:
  """Generic object-oriented Metadata writer.

  Note that this API is experimental and is subject to changes. Also it only
  supports limited input and output tensor types for now. More types are being
  added.

  Example usage:

  The model has two inputs, audio and image respectively. And generates two
  outputs: classification and embedding.

  with open(model_path, 'rb') as f:
    with Writer(f.read(), 'model_name', 'model description') as writer:
      writer
        .add_audio_input(sample_rate=16000, channels=1)
        .add_image_input()
        .add_classification_output(Labels().add(['A', 'B']))
        .add_embedding_output()
        .populate('model.tflite', 'model.json')
  """

  def __init__(self, model_buffer: bytearray, model_name: str,
               model_description: str):
    self._model_buffer = model_buffer
    self._general_md = metadata_info.GeneralMd(
        name=model_name, description=model_description)
    self._input_mds = []
    self._output_mds = []
    self._associate_files = []

  def __enter__(self):
    self._temp_folder = tempfile.TemporaryDirectory()
    return self

  def __exit__(self, unused_exc_type, unused_exc_val, unused_exc_tb):
    self._temp_folder.cleanup()
    # Delete the attribute so that it errors out outside the `with` statement.
    delattr(self, '_temp_folder')

  def populate(self,
               tflite_path: Optional[str] = None,
               json_path: Optional[str] = None):
    """Writes the generated flatbuffer file / json metadata to disk.

    Note that you'll only need the tflite file for deployment. The JSON file
    is useful to help you understand what's in the metadata.

    Args:
      tflite_path: path to the tflite file.
      json_path: path to the JSON file.

    Returns:
      A tuple of (tflite_content_in_bytes, metdata_json_content)
    """
    tflite_content = None
    metadata_json_content = None

    writer = metadata_writer.MetadataWriter.create_from_metadata_info(
        model_buffer=self._model_buffer,
        general_md=self._general_md,
        input_md=self._input_mds,
        output_md=self._output_mds,
        associated_files=self._associate_files)

    if tflite_path:
      tflite_content = writer.populate()
      writer_utils.save_file(tflite_content, tflite_path)

    if json_path:
      displayer = _metadata.MetadataDisplayer.with_model_file(tflite_path)
      metadata_json_content = displayer.get_metadata_json()
      with open(json_path, 'w') as f:
        f.write(metadata_json_content)

    return (tflite_content, metadata_json_content)

  def _export_labels(self, filename: str, index_to_label: List[str]):
    filepath = os.path.join(self._temp_folder.name, filename)
    with open(filepath, 'w') as f:
      f.write('\n'.join(index_to_label))
    self._associate_files.append(filepath)
    return filepath

  def _input_tensor_type(self, idx):
    return writer_utils.get_input_tensor_types(self._model_buffer)[idx]

  def _output_tensor_type(self, idx):
    return writer_utils.get_output_tensor_types(self._model_buffer)[idx]

  _INPUT_AUDIO_NAME = 'audio'
  _INPUT_AUDIO_DESCRIPTION = 'Input audio clip to be processed.'

  def add_feature_input(self,
                        name: Optional[str] = None,
                        description: Optional[str] = None) -> 'Writer':
    """Marks the next input tensor as a basic feature input."""
    input_md = metadata_info.TensorMd(name=name, description=description)
    self._input_mds.append(input_md)
    return self

  def add_audio_input(self,
                      sample_rate: int,
                      channels: int,
                      name: str = _INPUT_AUDIO_NAME,
                      description: str = _INPUT_AUDIO_DESCRIPTION):
    """Marks the next input tensor as an audio input."""
    # To make Task Library working properly, sample_rate, channels need to be
    # positive.
    if sample_rate <= 0:
      raise ValueError(
          'sample_rate should be positive, but got {}.'.format(sample_rate))
    if channels <= 0:
      raise ValueError(
          'channels should be positive, but got {}.'.format(channels))

    input_md = metadata_info.InputAudioTensorMd(
        name=name,
        description=description,
        sample_rate=sample_rate,
        channels=channels)
    self._input_mds.append(input_md)
    return self

  _INPUT_IMAGE_NAME = 'image'
  _INPUT_IMAGE_DESCRIPTION = 'Input image to be processed.'
  color_space_types = _metadata_fb.ColorSpaceType

  def add_image_input(
      self,
      norm_mean: List[float],
      norm_std: List[float],
      color_space_type: Optional[
          _metadata_fb.ColorSpaceType] = _metadata_fb.ColorSpaceType.RGB,
      name: str = _INPUT_IMAGE_NAME,
      description: str = _INPUT_IMAGE_DESCRIPTION) -> 'Writer':
    """Marks the next input tensor as an image input.

    Args:
      norm_mean: The mean value used to normalize each input channel. If there
        is only one element in the list, its value will be broadcasted to all
        channels. Also note that norm_mean and norm_std should have the same
        number of elements. [1]
      norm_std: The std value used to normalize each input channel. If there is
        only one element in the list, its value will be broadcasted to all
        channels. [1]
      color_space_type: The color space type of the input image. [2]
      name: Name of the input tensor.
      description: Description of the input tensor.

    Returns:
      The Writer instance, can be used for chained operation.

    [1]:
    https://www.tensorflow.org/lite/convert/metadata#normalization_and_quantization_parameters
    [2]:
    https://github.com/tensorflow/tflite-support/blob/b80289c4cd1224d0e1836c7654e82f070f9eefaa/tensorflow_lite_support/metadata/metadata_schema.fbs#L172
    """
    input_md = metadata_info.InputImageTensorMd(
        name=name,
        description=description,
        norm_mean=norm_mean,
        norm_std=norm_std,
        color_space_type=color_space_type,
        tensor_type=self._input_tensor_type(len(self._input_mds)))

    self._input_mds.append(input_md)
    return self

  _OUTPUT_EMBEDDING_NAME = 'embedding'
  _OUTPUT_EMBEDDING_DESCRIPTION = 'Embedding vector of the input.'

  def add_feature_output(self,
                         name: Optional[str] = None,
                         description: Optional[str] = None) -> 'Writer':
    """Marks the next input tensor as a basic feature output."""
    output_md = metadata_info.TensorMd(name=name, description=description)
    self._output_mds.append(output_md)
    return self

  def add_embedding_output(self,
                           name: str = _OUTPUT_EMBEDDING_NAME,
                           description: str = _OUTPUT_EMBEDDING_DESCRIPTION):
    """Marks the next output tensor as embedding."""
    output_md = metadata_info.TensorMd(name=name, description=description)
    self._output_mds.append(output_md)
    return self

  def _export_calibration_file(self, filename: str,
                               calibrations: List[CalibrationParameter]):
    """Store calibration parameters in a csv file."""
    filepath = os.path.join(self._temp_folder.name, filename)
    with open(filepath, 'w') as f:
      for idx, item in enumerate(calibrations):
        if idx != 0:
          f.write('\n')
        if item:
          scale, slope, offset, min_score = item
          if all(x is not None for x in item):
            f.write(f'{scale},{slope},{offset},{min_score}')
          elif all(x is not None for x in item[:3]):
            f.write(f'{scale},{slope},{offset}')
          else:
            raise ValueError('scale, slope and offset values can not be set to '
                             'None.')
          self._associate_files.append(filepath)
    return filepath

  _OUTPUT_CLASSIFICATION_NAME = 'score'
  _OUTPUT_CLASSIFICATION_DESCRIPTION = 'Score of the labels respectively'

  def add_classification_output(
      self,
      labels: Labels,
      score_calibration: Optional[ScoreCalibration] = None,
      name=_OUTPUT_CLASSIFICATION_NAME,
      description=_OUTPUT_CLASSIFICATION_DESCRIPTION) -> 'Writer':
    """Marks model's next output tensor as a classification head.

    Example usage:
    writer.add_classification_output(
      Labels()
        .add(['cat', 'dog], 'en')
        .add(['chat', 'chien], 'fr')
        .add(['/m/011l78', '/m/031d23'], use_as_category_name=True))

    Args:
      labels: an instance of Labels helper class.
      score_calibration: an instance of ScoreCalibration helper class.
      name: Metadata name of the tensor. Note that this is different from tensor
        name in the flatbuffer.
      description: human readable description of what the tensor does.

    Returns:
      The current Writer instance to allow chained operation.
    """
    calibration_md = None
    if score_calibration:
      calibration_md = metadata_info.ScoreCalibrationMd(
          score_transformation_type=score_calibration.transformation_type,
          default_score=score_calibration.default_score,
          file_path=self._export_calibration_file('score_calibration.txt',
                                                  score_calibration.parameters))

    idx = len(self._output_mds)

    label_files = []
    for item in labels._labels:  # pylint: disable=protected-access
      label_files.append(
          metadata_info.LabelFileMd(
              self._export_labels(item.filename, item.names),
              locale=item.locale))

    output_md = metadata_info.ClassificationTensorMd(
        name=name,
        description=description,
        label_files=label_files,
        tensor_type=self._output_tensor_type(idx),
        score_calibration_md=calibration_md,
    )
    self._output_mds.append(output_md)
    return self
