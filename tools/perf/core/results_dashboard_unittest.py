# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import logging
import unittest
from unittest import mock
from unittest.mock import call

from core import results_dashboard


class ResultsDashboardTest(unittest.TestCase):

  def setUp(self):
    self.dummy_token_generator = lambda: 'Arthur-Merlin'
    self.perf_data = {'foo': 1, 'bar': 2}
    self.dashboard_url = 'https://chromeperf.appspot.com'
    self.dummy_token = {'token': 'dummy1234', 'file': 'path/to/file'}

  def testRetryForSendResultRetryException(self):

    def raise_retry_exception(url, histogramset_json, token_generator_callback):
      del url, histogramset_json  # unused
      del token_generator_callback  # unused
      raise results_dashboard.SendResultsRetryException('Should retry')

    with mock.patch('core.results_dashboard.time.sleep') as sleep_mock:
      with mock.patch(
          'core.results_dashboard._SendHistogramJson',
          side_effect=raise_retry_exception) as m:
        upload_result = results_dashboard.SendResults(
            self.perf_data,
            'dummy_benchmark',
            self.dashboard_url,
            send_as_histograms=True,
            token_generator_callback=self.dummy_token_generator,
            num_retries=5)
        self.assertFalse(upload_result)
        self.assertEqual(m.call_count, 5)
        self.assertEqual(
            sleep_mock.mock_calls,
            [call(15), call(30),
             call(60), call(120),
             call(240)])

  def testNoRetryForSendResultFatalException(self):

    def raise_retry_exception(url, histogramset_json, token_generator_callback):
      del url, histogramset_json  # unused
      del token_generator_callback  # unused
      raise results_dashboard.SendResultsFatalException('Do not retry')

    with mock.patch('core.results_dashboard.time.sleep') as sleep_mock:
      with mock.patch(
          'core.results_dashboard._SendHistogramJson',
          side_effect=raise_retry_exception) as m:
        upload_result = results_dashboard.SendResults(
            self.perf_data,
            'dummy_benchmark',
            self.dashboard_url,
            send_as_histograms=True,
            token_generator_callback=self.dummy_token_generator,
            num_retries=5)
        self.assertFalse(upload_result)
        self.assertEqual(m.call_count, 1)
        self.assertFalse(sleep_mock.mock_calls)

  def testNoRetryForSuccessfulSendResult(self):
    with mock.patch('core.results_dashboard.time.sleep') as sleep_mock:
      with mock.patch('core.results_dashboard._SendHistogramJson') as m:
        upload_result = results_dashboard.SendResults(
            self.perf_data,
            'dummy_benchmark',
            self.dashboard_url,
            send_as_histograms=True,
            token_generator_callback=self.dummy_token_generator,
            num_retries=5)
        self.assertTrue(upload_result)
        self.assertEqual(m.call_count, 1)
        self.assertFalse(sleep_mock.mock_calls)

  def testNoRetryAfterSucessfulSendResult(self):
    counter = [0]

    def raise_retry_exception_first_two_times(url, histogramset_json,
                                              token_generator_callback):
      del url, histogramset_json  # unused
      del token_generator_callback  # unused
      counter[0] += 1
      if counter[0] <= 2:
        raise results_dashboard.SendResultsRetryException('Please retry')

    with mock.patch('core.results_dashboard.time.sleep') as sleep_mock:
      with mock.patch(
          'core.results_dashboard._SendHistogramJson',
          side_effect=raise_retry_exception_first_two_times) as m:
        upload_result = results_dashboard.SendResults(
            self.perf_data,
            'dummy_benchmark',
            self.dashboard_url,
            send_as_histograms=True,
            token_generator_callback=self.dummy_token_generator,
            num_retries=5)
        self.assertTrue(upload_result)
        self.assertEqual(m.call_count, 3)
        self.assertEqual(sleep_mock.mock_calls, [call(15), call(30)])

  def testSendHistogramJsonRetryException(self):

    def request(*args, **kwargs):
      del args, kwargs
      content = json.dumps(self.dummy_token)

      class Response(object):
        status = 403
        reason = 'dummy auth error'

      return Response(), content

    with mock.patch('core.results_dashboard.time.sleep') as sleep_mock:
      with mock.patch.object(
          results_dashboard.httplib2.Http, 'request', new=request):
        upload_result = results_dashboard.SendResults(
            self.perf_data,
            'dummy_benchmark',
            self.dashboard_url,
            send_as_histograms=True,
            token_generator_callback=self.dummy_token_generator,
            num_retries=5)
        self.assertFalse(upload_result)
        self.assertEqual(
            sleep_mock.mock_calls,
            [call(15), call(30),
             call(60), call(120),
             call(240)])

  def testSendHistogramJsonTypeWarning(self):

    def request(*args, **kwargs):
      del args, kwargs
      content = 'non json response'

      class Response(object):
        status = 200

      return Response(), content

    with mock.patch('core.results_dashboard.time.sleep') as sleep_mock:
      with mock.patch.object(
          results_dashboard.httplib2.Http, 'request', new=request):

        upload_result = results_dashboard.SendResults(
            self.perf_data,
            'dummy_benchmark',
            self.dashboard_url,
            send_as_histograms=True,
            token_generator_callback=self.dummy_token_generator,
            num_retries=5)

        # upload should still be successful, despite failing to fetch token.
        self.assertTrue(upload_result)
        self.assertFalse(sleep_mock.mock_calls)

  def testSendHistogramJsonValueWarning(self):

    def request(*args, **kwargs):
      del args, kwargs
      content = json.dumps(None)

      class Response(object):
        status = 200

      return Response(), content

    with mock.patch('core.results_dashboard.time.sleep') as sleep_mock:
      with mock.patch.object(
          results_dashboard.httplib2.Http, 'request', new=request):

        upload_result = results_dashboard.SendResults(
            self.perf_data,
            'dummy_benchmark',
            self.dashboard_url,
            send_as_histograms=True,
            token_generator_callback=self.dummy_token_generator,
            num_retries=5)

        # upload should still be successful, despite failing to fetch token.
        self.assertTrue(upload_result)
        self.assertFalse(sleep_mock.mock_calls)

  def testSuccessfulSendHistogramJson(self):

    def request(*args, **kwargs):
      del args, kwargs
      content = json.dumps(self.dummy_token)

      class Response(object):
        status = 200

      return Response(), content

    with mock.patch('core.results_dashboard.time.sleep') as sleep_mock:
      with mock.patch.object(
          results_dashboard.httplib2.Http, 'request', new=request):
        with mock.patch.object(logging, 'info') as log_mock:

          upload_result = results_dashboard.SendResults(
              self.perf_data,
              'dummy_benchmark',
              self.dashboard_url,
              send_as_histograms=True,
              token_generator_callback=self.dummy_token_generator,
              num_retries=5)

          self.assertTrue(upload_result)
          self.assertFalse(sleep_mock.mock_calls)

          # check token id is logged to info.
          found_token = False
          for log_call in log_mock.call_args_list:
            log = log_call[0][0] % tuple(log_call[0][1:])
            if 'dummy1234' in log:
              found_token = True
              break
          self.assertTrue(
              found_token, msg='Upload completion token not found in logs.')
