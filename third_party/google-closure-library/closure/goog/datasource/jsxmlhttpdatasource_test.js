// Copyright 2008 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

goog.module('goog.ds.JsXmlHttpDataSourceTest');
goog.setTestOnly();

const JsXmlHttpDataSource = goog.require('goog.ds.JsXmlHttpDataSource');
const TestQueue = goog.require('goog.testing.TestQueue');
const XhrIo = goog.require('goog.testing.net.XhrIo');
const testSuite = goog.require('goog.testing.testSuite');

const TEXT_PREFIX = null;
const TEXT_POSTFIX = null;

const INDEX_OF_URI_ENTRY = 1;
const INDEX_OF_CONTENT_ENTRY = 3;

testSuite({
  setUp() {},

  tearDown() {},

  testLoad_WithPostAndQueryDataSet() {
    const USE_POST = true;
    const dataSource = new JsXmlHttpDataSource(
        'uri', 'namne', TEXT_PREFIX, TEXT_POSTFIX, USE_POST);

    const testQueue = new TestQueue();
    dataSource.xhr_ = new XhrIo(testQueue);

    const expectedContent = 'Some test content';
    dataSource.setQueryData(expectedContent);
    dataSource.load();

    assertFalse(testQueue.isEmpty());

    const actualRequest = testQueue.dequeue();
    assertEquals(expectedContent, actualRequest[INDEX_OF_CONTENT_ENTRY]);
    assertTrue(testQueue.isEmpty());
  },

  testLoad_WithPostAndNoQueryDataSet() {
    const USE_POST = true;
    const dataSource = new JsXmlHttpDataSource(
        'uri?a=1&b=2', 'namne', TEXT_PREFIX, TEXT_POSTFIX, USE_POST);

    const testQueue = new TestQueue();
    dataSource.xhr_ = new XhrIo(testQueue);

    dataSource.load();

    assertFalse(testQueue.isEmpty());

    const actualRequest = testQueue.dequeue();
    assertEquals('a=1&b=2', actualRequest[INDEX_OF_CONTENT_ENTRY].toString());
    assertTrue(testQueue.isEmpty());
  },

  testLoad_WithGet() {
    const USE_GET = false;
    const expectedUri = 'uri?a=1&b=2';
    const dataSource = new JsXmlHttpDataSource(
        expectedUri, 'namne', TEXT_PREFIX, TEXT_POSTFIX, USE_GET);

    const testQueue = new TestQueue();
    dataSource.xhr_ = new XhrIo(testQueue);

    dataSource.load();

    assertFalse(testQueue.isEmpty());

    const actualRequest = testQueue.dequeue();
    assertEquals(expectedUri, actualRequest[INDEX_OF_URI_ENTRY].toString());
    assertTrue(testQueue.isEmpty());
  },
});
