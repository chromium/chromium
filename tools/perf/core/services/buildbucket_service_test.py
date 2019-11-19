# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

from core.services import buildbucket_service


class TestBuildbucketApi(unittest.TestCase):
  def setUp(self):
    self.mock_request = mock.patch('core.services.request.Request').start()
    self.mock_request.return_value = 'OK'

  def tearDown(self):
    mock.patch.stopall()

  def testGetBuild(self):
    self.assertEqual(buildbucket_service.GetBuild(
        'chromium', 'try', 'linux_chromium_rel_ng', 227278), 'OK')
    self.mock_request.assert_called_once_with(
        buildbucket_service.SERVICE_URL + 'GetBuild', method='POST',
        use_auth=True, content_type='json', accept='json',
        data={
            'builder': {
                'project': 'chromium',
                'bucket': 'try',
                'builder': 'linux_chromium_rel_ng',
            },
            'buildNumber': 227278
        })

  def testGetBuilds(self):
    self.assertEqual(buildbucket_service.GetBuilds(
        'chromium', 'try', 'linux_chromium_rel_ng'), 'OK')
    self.mock_request.assert_called_once_with(
        buildbucket_service.SERVICE_URL + 'SearchBuilds', method='POST',
        use_auth=True, content_type='json', accept='json',
        data={
            'predicate': {
                'builder': {
                    'project': 'chromium',
                    'bucket': 'try',
                    'builder': 'linux_chromium_rel_ng',
                },
                'status': 'ENDED_MASK'
            }
        })

  def testGetBuildsIncludeUnfinished(self):
    self.assertEqual(
        buildbucket_service.GetBuilds(
            'chromium', 'try', 'linux_chromium_rel_ng',
            only_completed=False),
        'OK')
    self.mock_request.assert_called_once_with(
        buildbucket_service.SERVICE_URL + 'SearchBuilds', method='POST',
        use_auth=True, content_type='json', accept='json',
        data={
            'predicate': {
                'builder': {
                    'project': 'chromium',
                    'bucket': 'try',
                    'builder': 'linux_chromium_rel_ng',
                }
            }
        })
