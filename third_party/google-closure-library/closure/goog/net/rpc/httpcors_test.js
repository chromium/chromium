/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.rpc.HttpCorsTest');
goog.setTestOnly('goog.net.rpc.HttpCorsTest');

const GoogUri = goog.require('goog.Uri');
const HttpCors = goog.require('goog.net.rpc.HttpCors');
const testSuite = goog.require('goog.testing.testSuite');


testSuite({
  testSingleHeader: function() {
    const headers = {'foo': 'bar'};
    const value = HttpCors.generateHttpHeadersOverwriteParam(headers);
    assertEquals('foo:bar\r\n', value);
    const encoded_value =
        HttpCors.generateEncodedHttpHeadersOverwriteParam(headers);
    assertEquals('foo%3Abar%0D%0A', encoded_value);
  },

  testMultipleHeaders: function() {
    const headers = {'foo1': 'bar1', 'foo2': 'bar2'};
    const value = HttpCors.generateHttpHeadersOverwriteParam(headers);
    assertEquals('foo1:bar1\r\nfoo2:bar2\r\n', value);
    const encoded_value =
        HttpCors.generateEncodedHttpHeadersOverwriteParam(headers);
    assertEquals('foo1%3Abar1%0D%0Afoo2%3Abar2%0D%0A', encoded_value);
  },

  testSetUrl: function() {
    const headers = {'foo': 'bar'};
    const urlString = '/example.com/';
    const newUrlString = HttpCors.setHttpHeadersWithOverwriteParam(
        urlString, '$httpHeaders', headers);
    assertEquals('/example.com/?%24httpHeaders=foo%3Abar%0D%0A', newUrlString);

    const url = new GoogUri(urlString);
    const newUrl =
        HttpCors.setHttpHeadersWithOverwriteParam(url, '$httpHeaders', headers);
    assertEquals(
        '/example.com/?%24httpHeaders=foo%3Abar%0D%0A', newUrl.toString());
  },

  testSetUrlAppend: function() {
    const headers = {'foo': 'bar'};
    const urlString = '/example.com/?abc=12';
    const newUrlString = HttpCors.setHttpHeadersWithOverwriteParam(
        urlString, '$httpHeaders', headers);
    assertEquals(
        '/example.com/?abc=12&%24httpHeaders=foo%3Abar%0D%0A', newUrlString);

    const url = new GoogUri(urlString);
    const newUrl =
        HttpCors.setHttpHeadersWithOverwriteParam(url, '$httpHeaders', headers);
    assertEquals(
        '/example.com/?abc=12&%24httpHeaders=foo%3Abar%0D%0A',
        newUrl.toString());
  },

  testSetUrlMultiHeaders: function() {
    const headers = {'foo1': 'bar1', 'foo2': 'bar2'};
    const urlString = '/example.com/';
    const newUrlString = HttpCors.setHttpHeadersWithOverwriteParam(
        urlString, '$httpHeaders', headers);
    assertEquals(
        '/example.com/?%24httpHeaders=foo1%3Abar1%0D%0Afoo2%3Abar2%0D%0A',
        newUrlString);

    const url = new GoogUri(urlString);
    const newUrl =
        HttpCors.setHttpHeadersWithOverwriteParam(url, '$httpHeaders', headers);
    assertEquals(
        '/example.com/?%24httpHeaders=foo1%3Abar1%0D%0Afoo2%3Abar2%0D%0A',
        newUrl.toString());
  },

  testSetUrlEmptyHeaders: function() {
    const headers = {};
    const urlString = '/example.com/';
    const newUrlString = HttpCors.setHttpHeadersWithOverwriteParam(
        urlString, '$httpHeaders', headers);
    assertEquals('/example.com/', newUrlString);

    const url = new GoogUri(urlString);
    const newUrl =
        HttpCors.setHttpHeadersWithOverwriteParam(url, '$httpHeaders', headers);
    assertEquals('/example.com/', newUrl.toString());
  },
});
