# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

# TODO(crbug.com/996778): Figure out how to get httplib2 hermetically.
import httplib2  # pylint: disable=import-error
import mock

from core.services import dashboard_service
from core.services import request


def TestResponse(code, content):
  def Request(url, *args, **kwargs):
    del args  # Unused.
    del kwargs  # Unused.
    response = httplib2.Response({'status': str(code)})
    if code != 200:
      raise request.BuildRequestError(url, response, content)
    else:
      return content
  return Request


class TestDashboardApi(unittest.TestCase):
  def setUp(self):
    self.mock_request = mock.patch('core.services.request.Request').start()
    self.mock_request.side_effect = TestResponse(200, 'OK')

  def tearDown(self):
    mock.patch.stopall()

  def testDescribe(self):
    self.assertEqual(dashboard_service.Describe('my_test'), 'OK')
    self.mock_request.assert_called_once_with(
        dashboard_service.SERVICE_URL + '/api/describe', method='POST',
        params={'test_suite': 'my_test'}, use_auth=True, accept='json')

  def testListTestPaths(self):
    self.assertEqual(
        dashboard_service.ListTestPaths('my_test', 'a_rotation'), 'OK')
    self.mock_request.assert_called_once_with(
        dashboard_service.SERVICE_URL + '/api/list_timeseries/my_test',
        method='POST', params={'sheriff': 'a_rotation'}, use_auth=True,
        accept='json')

  def testMatchTestPaths(self):
    self.assertEqual(
        dashboard_service.MatchTestPaths('*/*/benchmark/*'), 'OK')
    self.mock_request.assert_called_once_with(
        dashboard_service.SERVICE_URL + '/list_tests',
        method='POST', params={'type': 'pattern', 'p': '*/*/benchmark/*'},
        use_auth=True, accept='json')

  def testTimeseries2(self):
    response = dashboard_service.Timeseries2(
        test_suite='loading.mobile',
        measurement='timeToFirstContenrfulPaint',
        bot='ChromiumPerf:androd-go-perf',
        columns='revision,avg')
    self.assertEqual(response, 'OK')
    self.mock_request.assert_called_once_with(
        dashboard_service.SERVICE_URL + '/api/timeseries2',
        params={'test_suite': 'loading.mobile',
                'measurement': 'timeToFirstContenrfulPaint',
                'bot': 'ChromiumPerf:androd-go-perf',
                'columns': 'revision,avg'},
        method='POST', use_auth=True, accept='json')

  def testTimeseries2_notFoundRaisesKeyError(self):
    self.mock_request.side_effect = TestResponse(404, 'Not found')
    with self.assertRaises(KeyError):
      dashboard_service.Timeseries2(
          test_suite='loading.mobile',
          measurement='timeToFirstContenrfulPaint',
          bot='ChromiumPerf:androd-go-perf',
          columns='revision,avg')

  def testTimeseries2_missingArgsRaisesTypeError(self):
    with self.assertRaises(TypeError):
      dashboard_service.Timeseries2(
          test_suite='loading.mobile',
          measurement='timeToFirstContenrfulPaint')

  def testTimeseries(self):
    response = dashboard_service.Timeseries('some test path')
    self.assertEqual(response, 'OK')
    self.mock_request.assert_called_once_with(
        dashboard_service.SERVICE_URL + '/api/timeseries/some%20test%20path',
        params={'num_days': 30}, method='POST', use_auth=True, accept='json')

  def testTimeseries_notFoundRaisesKeyError(self):
    self.mock_request.side_effect = TestResponse(
        400, '{"error": "Invalid test_path"}')
    with self.assertRaises(KeyError):
      dashboard_service.Timeseries('some test path')

  def testBugs(self):
    self.assertEqual(dashboard_service.Bugs(123), 'OK')
    self.mock_request.assert_called_once_with(
        dashboard_service.SERVICE_URL + '/api/bugs/123', method='POST',
        use_auth=True, accept='json')

  def testIterAlerts(self):
    pages = {'page1': {'data': 'foo', 'next_cursor': 'page2'},
             'page2': {'data': 'bar'}}

    def RequestStub(endpoint, method=None, params=None, **kwargs):
      del kwargs  # Unused.
      self.assertEqual(endpoint, dashboard_service.SERVICE_URL + '/api/alerts')
      self.assertEqual(method, 'POST')
      self.assertDictContainsSubset(
          {'test_suite': 'loading.mobile', 'limit': 1000}, params)
      cursor = params.get('cursor', 'page1')
      return pages[cursor]

    self.mock_request.side_effect = RequestStub
    response = [
        resp['data']
        for resp in dashboard_service.IterAlerts(test_suite='loading.mobile')]
    self.assertEqual(response, ['foo', 'bar'])
    self.assertEqual(self.mock_request.call_count, 2)
