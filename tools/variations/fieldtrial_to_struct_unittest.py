# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import fieldtrial_to_struct
import os


class FieldTrialToStruct(unittest.TestCase):

  def FullRelativePath(self, relative_path):
    base_path = os.path.dirname(__file__)
    if not base_path:
      # Handle test being run from the current directory.
      base_path = '.'
    return base_path + relative_path

  def test_FieldTrialToDescription(self):
    config = {
      'Trial1': [
        {
          'platforms': ['windows'],
          'experiments': [
            {
              'name': 'Group1',
              'params': {
                'x': '1',
                'y': '2'
              },
              'enable_features': ['A', 'B'],
              'disable_features': ['C']
            },
            {
              'name': 'Group2',
              'params': {
                'x': '3',
                'y': '4'
              },
              'enable_features': ['D', 'E'],
              'disable_features': ['F']
            },
          ]
        }
      ],
      'Trial2': [
        {
          'platforms': ['windows'],
          'experiments': [{'name': 'OtherGroup'}]
        }
      ],
      'TrialWithForcingFlag':  [
        {
          'platforms': ['windows'],
          'experiments': [
            {
              'name': 'ForcedGroup',
              'forcing_flag': "my-forcing-flag"
            }
          ]
        }
      ]
    }
    result = fieldtrial_to_struct._FieldTrialConfigToDescription(
        config, ['windows'])
    expected = {
      'elements': {
        'kFieldTrialConfig': {
          'studies': [
            {
              'name': 'Trial1',
              'experiments': [
                {
                  'name': 'Group1',
                  'platforms': ['Study::PLATFORM_WINDOWS'],
                  'params': [
                    {'key': 'x', 'value': '1'},
                    {'key': 'y', 'value': '2'}
                  ],
                  'enable_features': ['A', 'B'],
                  'disable_features': ['C'],
                  'form_factors': [],
                },
                {
                  'name': 'Group2',
                  'platforms': ['Study::PLATFORM_WINDOWS'],
                  'params': [
                    {'key': 'x', 'value': '3'},
                    {'key': 'y', 'value': '4'}
                  ],
                  'enable_features': ['D', 'E'],
                  'disable_features': ['F'],
                  'form_factors': [],
                },
              ],
            },
            {
              'name': 'Trial2',
              'experiments': [
                {
                  'name': 'OtherGroup',
                  'platforms': ['Study::PLATFORM_WINDOWS'],
                  'form_factors': [],
                }
              ]
            },
            {
              'name': 'TrialWithForcingFlag',
              'experiments': [
                  {
                    'name': 'ForcedGroup',
                    'platforms': ['Study::PLATFORM_WINDOWS'],
                    'forcing_flag': "my-forcing-flag",
                    'form_factors': [],
                  }
              ]
            },
          ]
        }
      }
    }
    self.maxDiff = None
    self.assertEqual(expected, result)

  _MULTIPLE_PLATFORM_CONFIG = {
    'Trial1': [
      {
        'platforms': ['windows', 'ios'],
        'is_low_end_device': 'true',
        'experiments': [
          {
            'name': 'Group1',
            'params': {
              'x': '1',
              'y': '2'
            },
            'enable_features': ['A', 'B'],
            'disable_features': ['C']
          },
          {
            'name': 'Group2',
            'params': {
              'x': '3',
              'y': '4'
            },
            'enable_features': ['D', 'E'],
            'disable_features': ['F']
          }
        ]
      },
      {
        'platforms': ['ios'],
        'experiments': [
          {
            'name': 'IOSOnly'
          }
        ]
      },
    ],
    'Trial2': [
      {
        'platforms': ['windows', 'mac'],
        'experiments': [{'name': 'OtherGroup'}]
      }
    ]
  }

  def test_FieldTrialToDescriptionMultipleSinglePlatformMultipleTrial(self):
    result = fieldtrial_to_struct._FieldTrialConfigToDescription(
        self._MULTIPLE_PLATFORM_CONFIG, ['ios'])
    expected = {
      'elements': {
        'kFieldTrialConfig': {
          'studies': [
            {
              'name': 'Trial1',
              'experiments': [
                {
                  'name': 'Group1',
                  'platforms': ['Study::PLATFORM_IOS'],
                  'params': [
                    {'key': 'x', 'value': '1'},
                    {'key': 'y', 'value': '2'}
                  ],
                  'enable_features': ['A', 'B'],
                  'disable_features': ['C'],
                  'is_low_end_device': 'true',
                  'form_factors': [],
                },
                {
                  'name': 'Group2',
                  'platforms': ['Study::PLATFORM_IOS'],
                  'params': [
                    {'key': 'x', 'value': '3'},
                    {'key': 'y', 'value': '4'}
                  ],
                  'enable_features': ['D', 'E'],
                  'disable_features': ['F'],
                  'is_low_end_device': 'true',
                  'form_factors': [],
                },
                {
                  'name': 'IOSOnly',
                  'platforms': ['Study::PLATFORM_IOS'],
                  'form_factors': [],
                },
              ],
            },
          ]
        }
      }
    }
    self.maxDiff = None
    self.assertEqual(expected, result)

  def test_FieldTrialToDescriptionMultipleSinglePlatformSingleTrial(self):
    result = fieldtrial_to_struct._FieldTrialConfigToDescription(
        self._MULTIPLE_PLATFORM_CONFIG, ['mac'])
    expected = {
      'elements': {
        'kFieldTrialConfig': {
          'studies': [
            {
              'name': 'Trial2',
              'experiments': [
                {
                  'name': 'OtherGroup',
                  'platforms': ['Study::PLATFORM_MAC'],
                  'form_factors': [],
                },
              ],
            },
          ]
        }
      }
    }
    self.maxDiff = None
    self.assertEqual(expected, result)

  _MULTIPLE_FORM_FACTORS_CONFIG = {
      'Trial1': [
        {
          'platforms': ['windows'],
          'form_factors': ['desktop', 'phone'],
          'experiments': [{'name': 'Group1'}]
        }
      ],
      'Trial2': [
        {
          'platforms': ['windows'],
          'form_factors': ['tablet'],
          'experiments': [{'name': 'OtherGroup'}]
        }
      ]
    }

  def test_FieldTrialToDescriptionMultipleFormFactorsTrial(self):
    result = fieldtrial_to_struct._FieldTrialConfigToDescription(
        self._MULTIPLE_FORM_FACTORS_CONFIG, ['windows'])
    expected = {
      'elements': {
        'kFieldTrialConfig': {
          'studies': [
            {
              'name': 'Trial1',
              'experiments': [
                {
                  'name': 'Group1',
                  'platforms': ['Study::PLATFORM_WINDOWS'],
                  'form_factors': ['Study::DESKTOP', 'Study::PHONE'],
                },
              ],
            },
            {
              'name': 'Trial2',
              'experiments': [
                {
                  'name': 'OtherGroup',
                  'platforms': ['Study::PLATFORM_WINDOWS'],
                  'form_factors': ['Study::TABLET'],
                },
              ],
            },
          ]
        }
      }
    }
    self.maxDiff = None
    self.assertEqual(expected, result)

  _MULTIPLE_OVERRIDE_UI_STRING_CONFIG = {
    'Trial1': [
      {
        'platforms': ['windows'],
        'experiments': [
          {
            'name': 'Group1',
            'override_ui_strings': {
              'IDS_NEW_TAB_TITLE': 'test1',
              'IDS_SAD_TAB_TITLE': 'test2',
            },
          },
        ]
      }
    ],
    'Trial2': [
      {
        'platforms': ['windows'],
        'experiments': [
          {
            'name': 'Group2',
            'override_ui_strings': {
              'IDS_DEFAULT_TAB_TITLE': 'test3',
            },
          }
        ]
      }
    ]
  }

  def test_FieldTrialToDescriptionMultipleOverrideUIStringTrial(self):
    result = fieldtrial_to_struct._FieldTrialConfigToDescription(
        self._MULTIPLE_OVERRIDE_UI_STRING_CONFIG, ['windows'])
    expected = {
      'elements': {
        'kFieldTrialConfig': {
          'studies': [
            {
              'name': 'Trial1',
              'experiments': [
                {
                  'name': 'Group1',
                  'platforms': ['Study::PLATFORM_WINDOWS'],
                  'override_ui_string': [
                    {
                      'name_hash':
                        4045341670,
                      'value': 'test1'
                    },
                    {
                      'name_hash':
                        1173727369,
                      'value': 'test2'
                    },
                  ],
                  'form_factors': [],
                },
              ],
            },
            {
              'name': 'Trial2',
              'experiments': [
                {
                  'name': 'Group2',
                  'platforms': ['Study::PLATFORM_WINDOWS'],
                  'override_ui_string': [
                    {
                      'name_hash':
                        3477264953,
                      'value': 'test3'
                    },
                  ],
                  'form_factors': [],
                },
              ],
            }
          ]
        }
      }
    }
    self.maxDiff = None
    self.assertEqual(expected, result)

  def test_FieldTrialToStructMain(self):

    schema = self.FullRelativePath(
              '/../../components/variations/field_trial_config/'
              'field_trial_testing_config_schema.json')
    unittest_data_dir = self.FullRelativePath('/unittest_data/')
    test_output_filename = 'test_output'
    fieldtrial_to_struct.main([
      '--schema=' + schema,
      '--output=' + test_output_filename,
      '--platform=windows',
      '--year=2015',
      unittest_data_dir + 'test_config.json'
    ])
    header_filename = test_output_filename + '.h'
    with open(header_filename, 'r') as header:
      test_header = header.read()
      with open(unittest_data_dir + 'expected_output.h', 'r') as expected:
        expected_header = expected.read()
        self.assertEqual(expected_header, test_header)
    os.unlink(header_filename)

    cc_filename = test_output_filename + '.cc'
    with open(cc_filename, 'r') as cc:
      test_cc = cc.read()
      with open(unittest_data_dir + 'expected_output.cc', 'r') as expected:
        expected_cc = expected.read()
        self.assertEqual(expected_cc, test_cc)
    os.unlink(cc_filename)

if __name__ == '__main__':
  unittest.main()
