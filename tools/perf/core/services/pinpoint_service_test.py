# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from unittest import mock

from core.services import pinpoint_service


class TestPinpointService(unittest.TestCase):
  def setUp(self):
    self.get_user_email = mock.patch(
        'core.services.luci_auth.GetUserEmail').start()
    self.get_user_email.return_value = 'user@example.com'
    self.mock_request = mock.patch('core.services.request.Request').start()
    self.mock_request.return_value = 'OK'

  def tearDown(self):
    mock.patch.stopall()

  def testJob(self):
    self.assertEqual(pinpoint_service.Job('1234'), 'OK')
    self.mock_request.assert_called_once_with(
        pinpoint_service.SERVICE_URL + '/job/1234', params=[], use_auth=True,
        accept='json')

  def testJob_withState(self):
    self.assertEqual(pinpoint_service.Job('1234', with_state=True), 'OK')
    self.mock_request.assert_called_once_with(
        pinpoint_service.SERVICE_URL + '/job/1234', params=[('o', 'STATE')],
        use_auth=True, accept='json')

  def testJobs(self):
    self.mock_request.return_value = ['job1', 'job2', 'job3']
    self.assertEqual(pinpoint_service.Jobs(), ['job1', 'job2', 'job3'])
    self.mock_request.assert_called_once_with(
        pinpoint_service.SERVICE_URL + '/jobs', use_auth=True, accept='json')

  def testNewJob(self):
    self.assertEqual(pinpoint_service.NewJob(
        name='test_job', configuration='some_config'), 'OK')
    self.mock_request.assert_called_once_with(
        pinpoint_service.SERVICE_URL + '/new', method='POST',
        data={'name': 'test_job', 'configuration': 'some_config',
              'user': 'user@example.com'},
        use_auth=True, accept='json')
