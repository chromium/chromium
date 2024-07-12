# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from core.services import resultdb_service


class TestResultDbApi(unittest.TestCase):

  def setUp(self):
    self.mock_request = mock.patch('core.services.request.Request').start()
    self.mock_request.return_value = 'OK'

  def tearDown(self):
    mock.patch.stopall()

  def testGetBuild(self):
    self.assertEqual(resultdb_service.GetQueryTestResult(8744499105835250977),
                     'OK')
    self.mock_request.assert_called_once_with(
        resultdb_service.SERVICE_URL + 'QueryTestResults',
        method='POST',
        use_auth=True,
        content_type='json',
        accept='json',
        data={
            'invocations': ['invocations/build-8744499105835250977'],
            'predicate': {
                'expectancy': 'VARIANTS_WITH_UNEXPECTED_RESULTS'
            }
        })

  def testGetBuilds(self):
    self.assertEqual(
        resultdb_service.GetQueryTestResults(
            [8745382427271257361, 8744499105835250977]), 'OK')
    self.mock_request.assert_called_once_with(
        resultdb_service.SERVICE_URL + 'QueryTestResults',
        method='POST',
        use_auth=True,
        content_type='json',
        accept='json',
        data={
            'invocations': [
                'invocations/build-8745382427271257361',
                'invocations/build-8744499105835250977'
            ],
            'predicate': {
                'expectancy': 'VARIANTS_WITH_UNEXPECTED_RESULTS'
            }
        })
