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
"""Tests for tensorflow_lite_support.metadata.metadata_writer_for_task."""

import os
import sys
import tensorflow as tf
from tensorflow_lite_support.metadata.python import metadata_writer_for_task as mt
from tensorflow_lite_support.metadata.python.tests.metadata_writers import test_utils

_AUDIO_CLASSIFICATION_MODEL = '../testdata/audio_classifier/yamnet_wavin_quantized_mel_relu6.tflite'
_AUDIO_EMBEDDING_MODEL = '../testdata/audio_embedder/yamnet_embedding.tflite'
_IMAGE_CLASSIFIER_MODEL = '../testdata/image_classifier/mobilenet_v2_1.0_224.tflite'


class LabelsTest(tf.test.TestCase):

  def test_category_name(self):
    labels = mt.Labels()
    self.assertEqual(
        labels.add(['a', 'b'], use_as_category_name=True)._labels,
        [(None, 'labels.txt', ['a', 'b'])])
    # Overwrite categories
    self.assertEqual(
        labels.add(['new_a', 'new_b'], use_as_category_name=True)._labels,
        [(None, 'labels.txt', ['new_a', 'new_b'])])

  def test_locale(self):
    labels = mt.Labels()

    # Add from file.
    en_filepath = self.create_tempfile().full_path
    with open(en_filepath, 'w') as f:
      f.write('a\nb')
    labels.add_from_file(en_filepath, 'en')

    # Customized file name
    labels.add(['A', 'B'], 'fr', exported_filename='my_file.txt')
    self.assertEqual(labels._labels, [
        ('en', 'labels_en.txt', ['a', 'b']),
        ('fr', 'my_file.txt', ['A', 'B']),
    ])

    # Add category name, which should be the first file in the list.
    labels.add(['aa', 'bb'], 'cn', use_as_category_name=True)
    self.assertEqual(labels._labels, [
        ('cn', 'labels_cn.txt', ['aa', 'bb']),
        ('en', 'labels_en.txt', ['a', 'b']),
        ('fr', 'my_file.txt', ['A', 'B']),
    ])


class MetadataWriterForTaskTest(tf.test.TestCase):

  def test_initialize_without_with_block(self):
    writer = mt.Writer(
        test_utils.load_file(_AUDIO_CLASSIFICATION_MODEL),
        model_name='test_model',
        model_description='test_description')

    # Calling `add_classification_output` outside the `with` block fails.
    with self.assertRaisesRegex(AttributeError, '_temp_folder'):
      writer.add_classification_output(mt.Labels().add(['cat', 'dog']))

    writer.__enter__()
    writer.add_classification_output(mt.Labels().add(['cat', 'dog']))
    writer.__exit__(*sys.exc_info())

    # Calling `add_classification_output` after `with` block closes also fails.
    with self.assertRaisesRegex(AttributeError, '_temp_folder'):
      writer.add_classification_output(mt.Labels().add(['cat', 'dog']))

  def test_initialize_and_populate(self):
    with mt.Writer(
        test_utils.load_file(_AUDIO_CLASSIFICATION_MODEL),
        model_name='my_audio_model',
        model_description='my_description') as writer:
      out_dir = self.create_tempdir()
      _, metadata_json = writer.populate(
          os.path.join(out_dir, 'model.tflite'),
          os.path.join(out_dir, 'metadata.json'))
      self.assertJsonEqual(
          metadata_json, """{
  "name": "my_audio_model",
  "description": "my_description",
  "subgraph_metadata": [
    {
      "input_tensor_metadata": [
        {
          "name": "waveform_binary"
        }
      ],
      "output_tensor_metadata": [
        {
          "name": "tower0/network/layer32/final_output"
        }
      ]
    }
  ],
  "min_parser_version": "1.0.0"
}
""")

  def test_general_model(self):
    with mt.Writer(
        test_utils.load_file(_AUDIO_CLASSIFICATION_MODEL),
        model_name='my_model',
        model_description='my_description') as writer:
      writer.add_feature_input(
          name='input_tesnor', description='a feature input tensor')
      writer.add_feature_output(
          name='output_tesnor', description='a feature output tensor')

      out_dir = self.create_tempdir()
      writer.populate(
          os.path.join(out_dir, 'model.tflite'),
          os.path.join(out_dir, 'metadata.tflite'))
      self.assertEqual(
          test_utils.load_file(os.path.join(out_dir, 'metadata.tflite'), 'r'),
          """{
  "name": "my_model",
  "description": "my_description",
  "subgraph_metadata": [
    {
      "input_tensor_metadata": [
        {
          "name": "input_tesnor",
          "description": "a feature input tensor",
          "content": {
            "content_properties_type": "FeatureProperties",
            "content_properties": {
            }
          },
          "stats": {
          }
        }
      ],
      "output_tensor_metadata": [
        {
          "name": "output_tesnor",
          "description": "a feature output tensor",
          "content": {
            "content_properties_type": "FeatureProperties",
            "content_properties": {
            }
          },
          "stats": {
          }
        }
      ]
    }
  ],
  "min_parser_version": "1.0.0"
}
""")

  def test_audio_classifier(self):
    with mt.Writer(
        test_utils.load_file(_AUDIO_CLASSIFICATION_MODEL),
        model_name='audio_classifier',
        model_description='Classify the input audio clip') as writer:
      out_dir = self.create_tempdir()
      writer.add_audio_input(sample_rate=16000, channels=1)
      writer.add_classification_output(mt.Labels().add(
          ['sound1', 'sound2'], 'en', use_as_category_name=True))
      writer.populate(
          os.path.join(out_dir, 'model.tflite'),
          os.path.join(out_dir, 'metadata.tflite'))
      self.assertEqual(
          test_utils.load_file(os.path.join(out_dir, 'metadata.tflite'), 'r'),
          """{
  "name": "audio_classifier",
  "description": "Classify the input audio clip",
  "subgraph_metadata": [
    {
      "input_tensor_metadata": [
        {
          "name": "audio",
          "description": "Input audio clip to be processed.",
          "content": {
            "content_properties_type": "AudioProperties",
            "content_properties": {
              "sample_rate": 16000,
              "channels": 1
            }
          },
          "stats": {
          }
        }
      ],
      "output_tensor_metadata": [
        {
          "name": "score",
          "description": "Score of the labels respectively",
          "content": {
            "content_properties_type": "FeatureProperties",
            "content_properties": {
            }
          },
          "stats": {
            "max": [
              1.0
            ],
            "min": [
              0.0
            ]
          },
          "associated_files": [
            {
              "name": "labels_en.txt",
              "description": "Labels for categories that the model can recognize.",
              "type": "TENSOR_AXIS_LABELS",
              "locale": "en"
            }
          ]
        }
      ]
    }
  ],
  "min_parser_version": "1.3.0"
}
""")

  def test_audio_classifier_with_locale(self):
    with mt.Writer(
        test_utils.load_file(_AUDIO_CLASSIFICATION_MODEL),
        model_name='audio_classifier',
        model_description='Classify the input audio clip') as writer:
      out_dir = self.create_tempdir()
      writer.add_audio_input(sample_rate=16000, channels=1)
      writer.add_classification_output(mt.Labels().add(
          ['/id1', '/id2'],
          use_as_category_name=True).add(['sound1', 'sound2'],
                                         'en').add(['son1', 'son2'], 'fr'))
      _, metadata_json = writer.populate(
          os.path.join(out_dir, 'model.tflite'),
          os.path.join(out_dir, 'metadata.tflite'))
      self.assertEqual(
          metadata_json, """{
  "name": "audio_classifier",
  "description": "Classify the input audio clip",
  "subgraph_metadata": [
    {
      "input_tensor_metadata": [
        {
          "name": "audio",
          "description": "Input audio clip to be processed.",
          "content": {
            "content_properties_type": "AudioProperties",
            "content_properties": {
              "sample_rate": 16000,
              "channels": 1
            }
          },
          "stats": {
          }
        }
      ],
      "output_tensor_metadata": [
        {
          "name": "score",
          "description": "Score of the labels respectively",
          "content": {
            "content_properties_type": "FeatureProperties",
            "content_properties": {
            }
          },
          "stats": {
            "max": [
              1.0
            ],
            "min": [
              0.0
            ]
          },
          "associated_files": [
            {
              "name": "labels.txt",
              "description": "Labels for categories that the model can recognize.",
              "type": "TENSOR_AXIS_LABELS"
            },
            {
              "name": "labels_en.txt",
              "description": "Labels for categories that the model can recognize.",
              "type": "TENSOR_AXIS_LABELS",
              "locale": "en"
            },
            {
              "name": "labels_fr.txt",
              "description": "Labels for categories that the model can recognize.",
              "type": "TENSOR_AXIS_LABELS",
              "locale": "fr"
            }
          ]
        }
      ]
    }
  ],
  "min_parser_version": "1.3.0"
}
""")

  def test_audio_classifier_with_locale_and_score_calibration(self):
    with mt.Writer(
        test_utils.load_file(_AUDIO_CLASSIFICATION_MODEL),
        model_name='audio_classifier',
        model_description='Classify the input audio clip') as writer:
      out_dir = self.create_tempdir()
      writer.add_audio_input(sample_rate=16000, channels=1)
      writer.add_classification_output(
          mt.Labels().add(['/id1', '/id2'], use_as_category_name=True).add(
              ['sound1', 'sound2'], 'en').add(['son1', 'son2'], 'fr'),
          score_calibration=mt.ScoreCalibration(
              mt.ScoreCalibration.transformation_types.INVERSE_LOGISTIC, [
                  mt.CalibrationParameter(1., 2., 3., None),
                  mt.CalibrationParameter(1., 2., 3., 4.),
              ],
              default_score=0.5))
      _, metadata_json = writer.populate(
          os.path.join(out_dir, 'model.tflite'),
          os.path.join(out_dir, 'metadata.tflite'))
      self.assertEqual(
          metadata_json, """{
  "name": "audio_classifier",
  "description": "Classify the input audio clip",
  "subgraph_metadata": [
    {
      "input_tensor_metadata": [
        {
          "name": "audio",
          "description": "Input audio clip to be processed.",
          "content": {
            "content_properties_type": "AudioProperties",
            "content_properties": {
              "sample_rate": 16000,
              "channels": 1
            }
          },
          "stats": {
          }
        }
      ],
      "output_tensor_metadata": [
        {
          "name": "score",
          "description": "Score of the labels respectively",
          "content": {
            "content_properties_type": "FeatureProperties",
            "content_properties": {
            }
          },
          "process_units": [
            {
              "options_type": "ScoreCalibrationOptions",
              "options": {
                "score_transformation": "INVERSE_LOGISTIC",
                "default_score": 0.5
              }
            }
          ],
          "stats": {
            "max": [
              1.0
            ],
            "min": [
              0.0
            ]
          },
          "associated_files": [
            {
              "name": "labels.txt",
              "description": "Labels for categories that the model can recognize.",
              "type": "TENSOR_AXIS_LABELS"
            },
            {
              "name": "labels_en.txt",
              "description": "Labels for categories that the model can recognize.",
              "type": "TENSOR_AXIS_LABELS",
              "locale": "en"
            },
            {
              "name": "labels_fr.txt",
              "description": "Labels for categories that the model can recognize.",
              "type": "TENSOR_AXIS_LABELS",
              "locale": "fr"
            },
            {
              "name": "score_calibration.txt",
              "description": "Contains sigmoid-based score calibration parameters. The main purposes of score calibration is to make scores across classes comparable, so that a common threshold can be used for all output classes.",
              "type": "TENSOR_AXIS_SCORE_CALIBRATION"
            }
          ]
        }
      ]
    }
  ],
  "min_parser_version": "1.3.0"
}
""")

  def test_audio_embedder(self):
    with mt.Writer(
        test_utils.load_file(_AUDIO_EMBEDDING_MODEL),
        model_name='audio_embedder',
        model_description='Generate embedding for the input audio clip'
    ) as writer:
      out_dir = self.create_tempdir()
      writer.add_audio_input(sample_rate=16000, channels=1)
      writer.add_embedding_output()
      _, metadata_json = writer.populate(
          os.path.join(out_dir, 'model.tflite'),
          os.path.join(out_dir, 'metadata.json'))
      self.assertEqual(
          metadata_json, """{
  "name": "audio_embedder",
  "description": "Generate embedding for the input audio clip",
  "subgraph_metadata": [
    {
      "input_tensor_metadata": [
        {
          "name": "audio",
          "description": "Input audio clip to be processed.",
          "content": {
            "content_properties_type": "AudioProperties",
            "content_properties": {
              "sample_rate": 16000,
              "channels": 1
            }
          },
          "stats": {
          }
        }
      ],
      "output_tensor_metadata": [
        {
          "name": "embedding",
          "description": "Embedding vector of the input.",
          "content": {
            "content_properties_type": "FeatureProperties",
            "content_properties": {
            }
          },
          "stats": {
          }
        }
      ]
    }
  ],
  "min_parser_version": "1.3.0"
}
""")

  def test_image_classifier(self):
    with mt.Writer(
        test_utils.load_file(_IMAGE_CLASSIFIER_MODEL),
        model_name='image_classifier',
        model_description='Imagenet classification model') as writer:
      out_dir = self.create_tempdir()
      writer.add_image_input(
          norm_mean=[127.5, 127.5, 127.5],
          norm_std=[127.5, 127.5, 127.5],
          color_space_type=mt.Writer.color_space_types.RGB)
      writer.add_classification_output(mt.Labels().add(['a', 'b', 'c']))
      _, metadata_json = writer.populate(
          os.path.join(out_dir, 'model.tflite'),
          os.path.join(out_dir, 'metadat.json'))
      self.assertEqual(
          metadata_json, """{
  "name": "image_classifier",
  "description": "Imagenet classification model",
  "subgraph_metadata": [
    {
      "input_tensor_metadata": [
        {
          "name": "image",
          "description": "Input image to be processed.",
          "content": {
            "content_properties_type": "ImageProperties",
            "content_properties": {
              "color_space": "RGB"
            }
          },
          "process_units": [
            {
              "options_type": "NormalizationOptions",
              "options": {
                "mean": [
                  127.5,
                  127.5,
                  127.5
                ],
                "std": [
                  127.5,
                  127.5,
                  127.5
                ]
              }
            }
          ],
          "stats": {
            "max": [
              1.0,
              1.0,
              1.0
            ],
            "min": [
              -1.0,
              -1.0,
              -1.0
            ]
          }
        }
      ],
      "output_tensor_metadata": [
        {
          "name": "score",
          "description": "Score of the labels respectively",
          "content": {
            "content_properties_type": "FeatureProperties",
            "content_properties": {
            }
          },
          "stats": {
            "max": [
              1.0
            ],
            "min": [
              0.0
            ]
          },
          "associated_files": [
            {
              "name": "labels.txt",
              "description": "Labels for categories that the model can recognize.",
              "type": "TENSOR_AXIS_LABELS"
            }
          ]
        }
      ]
    }
  ],
  "min_parser_version": "1.0.0"
}
""")


if __name__ == '__main__':
  tf.test.main()
