/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.streams.XhrStreamReaderTest');
goog.setTestOnly('goog.net.streams.XhrStreamReaderTest');

const Base64PbStreamParser = goog.require('goog.net.streams.Base64PbStreamParser');
const ErrorCode = goog.require('goog.net.ErrorCode');
const HttpStatus = goog.require('goog.net.HttpStatus');
const JsonStreamParser = goog.require('goog.net.streams.JsonStreamParser');
const PbJsonStreamParser = goog.require('goog.net.streams.PbJsonStreamParser');
const PbStreamParser = goog.require('goog.net.streams.PbStreamParser');
const TestingNetXhrIo = goog.require('goog.testing.net.XhrIo');
const XhrIo = goog.require('goog.net.XhrIo');
const XmlHttp = goog.require('goog.net.XmlHttp');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');
const {XhrStreamReader, XhrStreamReaderStatus} = goog.require('goog.net.streams.xhrStreamReader');
const {getStreamParser} = goog.require('goog.net.streams.streamParsers');


let xhrReader;
let xhrIo;

const ReadyState = XmlHttp.ReadyState;

const CONTENT_TYPE_HEADER = XhrIo.CONTENT_TYPE_HEADER;
const CONTENT_TRANSFER_ENCODING = XhrIo.CONTENT_TRANSFER_ENCODING;


function shouldRunTests() {
  return XhrStreamReader.isStreamingSupported();
}

testSuite({
  setUp() {
    xhrIo = new TestingNetXhrIo();
    xhrReader = new XhrStreamReader(xhrIo);
  },


  tearDown() {
    xhrIo = null;
    xhrReader = null;
  },


  /** @suppress {visibility} suppression added to enable type checking */
  testGetParserByResponseHeader() {
    xhrIo.getStreamingResponseHeader = function() {
      return null;
    };
    assertNull(getStreamParser(/** @type {!XhrIo} */ (xhrIo)));

    xhrIo.getStreamingResponseHeader = function() {
      return '';
    };
    assertNull(getStreamParser(/** @type {!XhrIo} */ (xhrIo)));

    xhrIo.getStreamingResponseHeader = function() {
      return 'xxxxx';
    };
    assertNull(getStreamParser(/** @type {!XhrIo} */ (xhrIo)));

    xhrIo.getStreamingResponseHeader = function(key) {
      if (key == CONTENT_TYPE_HEADER) return 'application/json';
      return null;
    };
    assertTrue(
        getStreamParser(/** @type {!XhrIo} */ (xhrIo)) instanceof
        JsonStreamParser);

    xhrIo.getStreamingResponseHeader = function(key) {
      if (key == CONTENT_TYPE_HEADER) return 'application/json; charset=utf-8';
      return null;
    };
    assertTrue(
        getStreamParser(/** @type {!XhrIo} */ (xhrIo)) instanceof
        JsonStreamParser);

    xhrIo.getStreamingResponseHeader = function(key) {
      if (key == CONTENT_TYPE_HEADER) return 'application/x-protobuf';
      return null;
    };
    assertTrue(
        getStreamParser(/** @type {!XhrIo} */ (xhrIo)) instanceof
        PbStreamParser);

    xhrIo.getStreamingResponseHeader = function(key) {
      if (key == CONTENT_TYPE_HEADER) return 'application/x-protobuf';
      if (key == CONTENT_TRANSFER_ENCODING) return 'BASE64';
      return null;
    };
    assertTrue(
        getStreamParser(/** @type {!XhrIo} */ (xhrIo)) instanceof
        Base64PbStreamParser);

    xhrIo.getStreamingResponseHeader = function(key) {
      if (key == CONTENT_TYPE_HEADER) return 'application/json+protobuf';
      return null;
    };
    assertTrue(
        getStreamParser(/** @type {!XhrIo} */ (xhrIo)) instanceof
        PbJsonStreamParser);
  },


  testNoData() {
    xhrReader.setDataHandler(function(messages) {
      fail('Received unexpected messages: ' + messages);
    });

    const streamStatus = [];
    const httpStatus = [];
    xhrReader.setStatusHandler(function() {
      streamStatus.push(xhrReader.getStatus());
      httpStatus.push(xhrIo.getStatus());
    });

    const headers = {'Content-Type': 'application/json'};
    xhrIo.send('/foo/bar');
    xhrIo.simulateResponse(HttpStatus.OK, '', headers);

    assertElementsEquals([XhrStreamReaderStatus.NO_DATA], streamStatus);
    assertElementsEquals([HttpStatus.OK], httpStatus);
  },


  testRetrieveHttpStatusInStatusHandler() {
    const received = [];
    xhrReader.setDataHandler(function(messages) {
      received.push(messages);
    });

    const streamStatus = [];
    const httpStatus = [];
    xhrReader.setStatusHandler(function() {
      streamStatus.push(xhrReader.getStatus());
      httpStatus.push(xhrIo.getStatus());
    });

    const headers = {'Content-Type': 'application/json'};
    xhrIo.send('/foo/bar');
    xhrIo.simulateResponse(HttpStatus.OK, '[{"1" : "b"}]', headers);

    assertEquals(1, received.length);
    assertEquals(1, received[0].length);
    assertElementsEquals(['1'], googObject.getKeys(received[0][0]));
    assertElementsEquals('b', received[0][0][1]);

    assertElementsEquals(
        [XhrStreamReaderStatus.ACTIVE, XhrStreamReaderStatus.SUCCESS],
        streamStatus);
    assertElementsEquals([HttpStatus.OK, HttpStatus.OK], httpStatus);
  },


  testParsingSingleMessage() {
    const received = [];
    xhrReader.setDataHandler(function(messages) {
      received.push(messages);
    });

    const body = 'CgX__gABdw==';
    const headers = {
      'Content-Type': 'application/x-protobuf',
      'Content-Transfer-Encoding': 'BASE64',
    };
    xhrIo.send('/foo/bar');
    xhrIo.simulateResponse(HttpStatus.OK, body, headers);

    assertEquals(1, received.length);
    assertEquals(1, received[0].length);
    assertElementsEquals(['1'], googObject.getKeys(received[0][0]));
    assertElementsEquals([0xFF, 0xFE, 0x00, 0x01, 0x77], received[0][0][1]);
  },


  testParsingMultipleMessages() {
    /**
     * Pass the following protobuf messages
     *    0x0a, 0x03, 0x61, 0x62, 0x63,
     *    0x0a, 0x03, 0x64, 0x65, 0x66,
     *    0x12, 0x03, 0x67, 0x68, 0x69,
     *    0x0a, 0x03, 0x6a, 0x6b, 0x6c,
     */

    const chunk1 = 'CgNh';
    const chunk2 = 'YmMKA';
    const chunk3 = '2RlZhIDZ2hpCg';
    const chunk4 = 'Nqa2w=';

    let received = [];
    xhrReader.setDataHandler(function(messages) {
      received.push(messages);
    });

    const headers = {
      'Content-Type': 'application/x-protobuf',
      'Content-Transfer-Encoding': 'BASE64',
    };

    xhrIo.send('/foo/bar');

    xhrIo.simulatePartialResponse(chunk1, headers);
    assertEquals(0, received.length);

    xhrIo.simulatePartialResponse(chunk2);
    assertEquals(1, received.length);
    assertEquals(1, received[0].length);
    assertElementsEquals(['1'], googObject.getKeys(received[0][0]));
    assertElementsEquals([0x61, 0x62, 0x63], received[0][0][1]);

    received = [];
    xhrIo.simulatePartialResponse(chunk3);
    assertEquals(1, received.length);
    assertEquals(2, received[0].length);
    assertElementsEquals(['1'], googObject.getKeys(received[0][0]));
    assertElementsEquals([0x64, 0x65, 0x66], received[0][0][1]);
    assertElementsEquals(['2'], googObject.getKeys(received[0][1]));
    assertElementsEquals([0x67, 0x68, 0x69], received[0][1][2]);

    received = [];
    xhrIo.simulatePartialResponse(chunk4);
    assertEquals(1, received.length);
    assertEquals(1, received[0].length);
    assertElementsEquals(['1'], googObject.getKeys(received[0][0]));
    assertElementsEquals([0x6a, 0x6b, 0x6c], received[0][0][1]);

    xhrIo.simulateReadyStateChange(ReadyState.COMPLETE);
    assertEquals(XhrStreamReaderStatus.SUCCESS, xhrReader.getStatus());
  },

  testParsingBinaryChunksBase64Encoded() {
    /**
     * Pass the following protobuf messages
     *    0x0a, 0x03, 0x61, 0x62, 0x63,
     *    0x0a, 0x03, 0x64, 0x65, 0x66,
     *    0x12, 0x03, 0x67, 0x68, 0x69,
     *    0x0a, 0x03, 0x6a, 0x6b, 0x6c,
     */
    if (typeof TextEncoder === 'undefined') {
      return;
    }
    const responseBase64 = 'CgNhYmMKA2RlZhIDZ2hpCgNqa2w=';
    const response = (new TextEncoder()).encode(responseBase64);
    const responseChunks = [response.slice(0, 5), response.slice(5)];
    // Override the original XhrIo.send();
    xhrIo.getResponse = () => responseChunks;

    let received = [];
    xhrReader.setDataHandler(function(messages) {
      received.push(messages);
    });

    const headers = {
      'Content-Type': 'application/x-protobuf',
      'Content-Transfer-Encoding': 'BASE64',
    };

    xhrIo.send('/foo/bar');
    xhrIo.simulateResponse(HttpStatus.OK, 'testing', headers);
    assertElementsEquals([0x61, 0x62, 0x63], received[0][0][1]);
    assertElementsEquals([0x64, 0x65, 0x66], received[0][1][1]);
    assertElementsEquals([0x67, 0x68, 0x69], received[0][2][2]);
    assertElementsEquals([0x6a, 0x6b, 0x6c], received[0][3][1]);
  },


  testXhrTimeout() {
    xhrIo.send('/test', null, 'GET', null, null, 1000, false);

    xhrIo.simulatePartialResponse(
        '', {'Content-Type': 'application/x-protobuf'});
    xhrIo.getLastErrorCode = function() {
      return ErrorCode.TIMEOUT;
    };
    xhrIo.simulateReadyStateChange(ReadyState.COMPLETE);

    assertEquals(XhrStreamReaderStatus.TIMEOUT, xhrReader.getStatus());
  },


  // TODO: more mocked/networked tests, testing.xhrio not useful for streaming
  // states
});
