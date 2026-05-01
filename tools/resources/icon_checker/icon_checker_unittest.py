# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest.mock import MagicMock, patch, mock_open
import os
import sys
import json

# Add the tools/resources directory to sys.path
sys.path.append(os.path.dirname(__file__))
import icon_checker


class IconCheckerTest(unittest.TestCase):

  def test_CheckIcons_Valid(self):
    input_api = MagicMock()
    input_api.change.RepositoryRoot.return_value = '/root'
    input_api.os_path.join = os.path.join
    output_api = MagicMock()

    # Mock FetchValidIconNames to return a set with 'menu' and 'add_to_drive'
    with patch('icon_checker.FetchValidIconNames',
               return_value={'menu', 'add_to_drive'}):
      affected_icons = [('components/vector_icons/menu.icon', 'menu', None),
                        ('chrome/browser/resources/pdf/elements/icons.html',
                         'add-to-drive', 10)]

      results = icon_checker.CheckIcons(input_api, output_api, affected_icons)
      self.assertEqual(len(results), 0)

  def test_CheckIcons_Invalid(self):
    input_api = MagicMock()
    input_api.change.RepositoryRoot.return_value = '/root'
    input_api.os_path.join = os.path.join
    output_api = MagicMock()
    output_api.PresubmitPromptWarning = MagicMock(side_effect=lambda x: x)

    # Mock FetchValidIconNames
    with patch('icon_checker.FetchValidIconNames', return_value={'menu'}):
      affected_icons = [('components/vector_icons/invalid.icon', 'invalid',
                         None)]

      results = icon_checker.CheckIcons(input_api, output_api, affected_icons)
      self.assertEqual(len(results), 1)
      self.assertIn('Icon "invalid" does not match', results[0])

  def test_FetchValidIconNames_Parsing(self):
    mock_data = json.dumps({"icons": ["menu", "chevron_right"]})

    with patch('builtins.open', mock_open(read_data=mock_data)):
      names = icon_checker.FetchValidIconNames()
      self.assertEqual(names, {'menu', 'chevron_right'})

  def test_FetchValidIconNames_FileError(self):
    input_api = MagicMock()
    input_api.change.RepositoryRoot.return_value = '/root'
    input_api.os_path.join = os.path.join
    output_api = MagicMock()
    output_api.PresubmitNotifyResult = MagicMock(side_effect=lambda x: x)

    # Mock FetchValidIconNames to return None (simulating failure)
    with patch('icon_checker.FetchValidIconNames', return_value=None):
      affected_icons = [('some/path.icon', 'some_icon', None)]

      with self.assertRaises(AssertionError) as cm:
        icon_checker.CheckIcons(input_api, output_api, affected_icons)
      self.assertIn('Could not read tools/resources/icon_list.json',
                    str(cm.exception))


if __name__ == '__main__':
  unittest.main()
