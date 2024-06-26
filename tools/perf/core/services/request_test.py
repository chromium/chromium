# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import pickle
import unittest
from unittest import mock

# TODO(crbug.com/40641687): Figure out how to get httplib2 hermetically.
import httplib2  # pylint: disable=import-error

from core.services import request


def Response(code, content):
  return httplib2.Response({'status': str(code)}), content


class TestRequest(unittest.TestCase):
  def setUp(self):
    self.http = mock.Mock()
    mock.patch('httplib2.Http', return_value=self.http).start()
    mock.patch('time.sleep').start()

  def tearDown(self):
    mock.patch.stopall()

  def testRequest_simple(self):
    self.http.request.return_value = Response(200, 'OK!')
    self.assertEqual(request.Request('http://example.com/'), 'OK!')
    self.http.request.assert_called_once_with(
        'http://example.com/', method='GET', body=None, headers=mock.ANY)

  def testRequest_acceptJson(self):
    self.http.request.return_value = Response(200, b'{"code": "ok!"}')
    self.assertEqual(request.Request('http://example.com/', accept='json'),
                     {'code': 'ok!'})
    self.http.request.assert_called_once_with(
        'http://example.com/', method='GET', body=None, headers=mock.ANY)

  def testRequest_acceptJsonWithSecurityPrefix(self):
    self.http.request.return_value = Response(200, b')]}\'{"code": "ok!"}')
    self.assertEqual(request.Request('http://example.com/', accept='json'),
                     {'code': 'ok!'})
    self.http.request.assert_called_once_with(
        'http://example.com/', method='GET', body=None, headers=mock.ANY)

  def testRequest_postWithParams(self):
    self.http.request.return_value = Response(200, 'OK!')
    self.assertEqual(request.Request(
        'http://example.com/', params={'q': 'foo'}, method='POST'), 'OK!')
    self.http.request.assert_called_once_with(
        'http://example.com/?q=foo', method='POST', body=None, headers=mock.ANY)

  def testRequest_postWithData(self):
    self.http.request.return_value = Response(200, 'OK!')
    self.assertEqual(request.Request(
        'http://example.com/', data={'q': 'foo'}, method='POST'), 'OK!')
    self.http.request.assert_called_once_with(
        'http://example.com/', method='POST', body='q=foo', headers=mock.ANY)

  def testRequest_postWithJsonData(self):
    self.http.request.return_value = Response(200, 'OK!')
    self.assertEqual(request.Request(
        'http://example.com/', data={'q': 'foo'}, content_type='json',
        method='POST'), 'OK!')
    self.http.request.assert_called_once_with(
        'http://example.com/', method='POST', body='{"q":"foo"}',
        headers=mock.ANY)

  def testRequest_retryOnServerError(self):
    self.http.request.side_effect = [
        Response(500, 'Oops. Something went wrong!'),
        Response(200, 'All is now OK.')
    ]
    self.assertEqual(request.Request('http://example.com/'), 'All is now OK.')

  def testRequest_failOnClientError(self):
    self.http.request.side_effect = [
        Response(400, 'Bad request!'),
        Response(200, 'This is not called.')
    ]
    with self.assertRaises(request.ClientError):
      request.Request('http://example.com/')

  @mock.patch('core.services.luci_auth.GetAccessToken')
  def testRequest_withLuciAuth(self, get_access_token):
    get_access_token.return_value = 'access-token'
    self.http.request.return_value = Response(200, 'OK!')
    self.assertEqual(
        request.Request('http://example.com/', use_auth=True), 'OK!')
    self.http.request.assert_called_once_with(
        'http://example.com/', method='GET', body=None, headers={
            'Content-Length': '0',
            'Authorization': 'Bearer access-token'})


class TestRequestErrors(unittest.TestCase):
  def testClientErrorPickleable(self):
    error = request.ClientError(
        'api', *Response(400, 'You made a bad request!'))
    error = pickle.loads(pickle.dumps(error))
    self.assertIsInstance(error, request.ClientError)
    self.assertEqual(error.request, 'api')
    self.assertEqual(error.response.status, 400)
    self.assertEqual(error.content, 'You made a bad request!')

  def testServerErrorPickleable(self):
    error = request.ServerError(
        'api', *Response(500, 'Oops, I had a problem!'))
    error = pickle.loads(pickle.dumps(error))
    self.assertIsInstance(error, request.ServerError)
    self.assertEqual(error.request, 'api')
    self.assertEqual(error.response.status, 500)
    self.assertEqual(error.content, 'Oops, I had a problem!')

  def testJsonErrorMessageToString(self):
    message = u'Something went wrong. That\u2019s all we know.'
    error = request.ServerError(
        '/endpoint', *Response(500, json.dumps({'error': message})))
    self.assertIn('Something went wrong.', str(error))

  def testErrorMessageToString(self):
    content = u'Something went wrong. That\u2019s all we know.'.encode('utf-8')
    error = request.ServerError('/endpoint', *Response(500, content))
    self.assertIn('Something went wrong.', str(error))
