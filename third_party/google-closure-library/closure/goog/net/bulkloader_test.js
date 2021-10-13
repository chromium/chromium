/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.BulkLoaderTest');
goog.setTestOnly();

const BulkLoader = goog.require('goog.net.BulkLoader');
const EventHandler = goog.require('goog.events.EventHandler');
const EventType = goog.require('goog.net.EventType');
const GoogEvent = goog.require('goog.events.Event');
const MockClock = goog.require('goog.testing.MockClock');
const testSuite = goog.require('goog.testing.testSuite');

/** Test interval between sending uri requests to the server. */
const DELAY_INTERVAL_BETWEEN_URI_REQUESTS = 5;

/** Test interval before a response is received for a URI request. */
const DELAY_INTERVAL_FOR_URI_LOAD = 15;

let clock;
let loadError;
let loadSuccess;

let successResponseTexts;
let errorStatus;

/**
 * Gets the successful bulkloader for the specified uris with some
 * modifications for testability.
 * <ul>
 *   <li> Added onSuccess methods to simulate success while loading uris.
 *   <li> The send function of the XhrManager used by the bulkloader
 *        calls the onSuccess function after a specified time interval.
 * </ul>
 * @param {Array<string>} uris The URIs.
 */
function getSuccessfulBulkLoader(uris) {
  const bulkLoader = new BulkLoader(uris);
  /** @suppress {globalThis} suppression added to enable type checking */
  bulkLoader.load = function() {
    /** @suppress {globalThis} suppression added to enable type checking */
    const uris = this.helper_.getUris();
    for (let i = 0; i < uris.length; i++) {
      // This clock tick simulates a delay for processing every URI.
      clock.tick(DELAY_INTERVAL_BETWEEN_URI_REQUESTS);
      // This timeout determines how many ticks after the send request
      // all the URIs will complete loading. This delays the load of
      // the first uri and every subsequent uri by 15 ticks.
      setTimeout(
          goog.bind(this.onSuccess, this, i, uris[i]),
          DELAY_INTERVAL_FOR_URI_LOAD);
    }
  };

  /**
   * @suppress {strictMissingProperties,globalThis} suppression added to enable
   * type checking
   */
  bulkLoader.onSuccess = function(id, uri) {
    const xhrIo = {
      getResponseText: function() {
        return uri;
      },
      isSuccess: function() {
        return true;
      },
      dispose: function() {},
    };
    this.handleEvent_(id, new GoogEvent(EventType.COMPLETE, xhrIo));
  };

  const eventHandler = new EventHandler();
  eventHandler.listen(bulkLoader, EventType.SUCCESS, handleSuccess);
  eventHandler.listen(bulkLoader, EventType.ERROR, handleError);

  return bulkLoader;
}

/**
 * Gets the non-successful bulkloader for the specified uris with some
 * modifications for testability.
 * <ul>
 *   <li> Added onSuccess and onError methods to simulate success and error
 *        while loading uris.
 *   <li> The send function of the XhrManager used by the bulkloader
 *        calls the onSuccess or onError function after a specified time
 *        interval.
 * </ul>
 * @param {Array<string>} uris The URIs.
 */
function getNonSuccessfulBulkLoader(uris) {
  const bulkLoader = new BulkLoader(uris);
  /** @suppress {globalThis} suppression added to enable type checking */
  bulkLoader.load = function() {
    /** @suppress {globalThis} suppression added to enable type checking */
    const uris = this.helper_.getUris();
    for (let i = 0; i < uris.length; i++) {
      // This clock tick simulates a delay for processing every URI.
      clock.tick(DELAY_INTERVAL_BETWEEN_URI_REQUESTS);

      // This timeout determines how many ticks after the send request
      // all the URIs will complete loading in error. This delays the load
      // of the first uri and every subsequent uri by 15 ticks. The URI
      // with id == 2 is in error.
      if (i != 2) {
        setTimeout(
            goog.bind(this.onSuccess, this, i, uris[i]),
            DELAY_INTERVAL_FOR_URI_LOAD);
      } else {
        setTimeout(
            goog.bind(this.onError, this, i, uris[i]),
            DELAY_INTERVAL_FOR_URI_LOAD);
      }
    }
  };

  /**
   * @suppress {strictMissingProperties,globalThis} suppression added to enable
   * type checking
   */
  bulkLoader.onSuccess = function(id, uri) {
    const xhrIo = {
      getResponseText: function() {
        return uri;
      },
      isSuccess: function() {
        return true;
      },
      dispose: function() {},
    };
    this.handleEvent_(id, new GoogEvent(EventType.COMPLETE, xhrIo));
  };

  /**
   * @suppress {strictMissingProperties,globalThis} suppression added to enable
   * type checking
   */
  bulkLoader.onError = function(id) {
    const xhrIo = {
      getResponseText: function() {
        return null;
      },
      isSuccess: function() {
        return false;
      },
      dispose: function() {},
      getStatus: function() {
        return 500;
      },
    };
    this.handleEvent_(id, new GoogEvent(EventType.ERROR, xhrIo));
  };

  const eventHandler = new EventHandler();
  eventHandler.listen(bulkLoader, EventType.SUCCESS, handleSuccess);
  eventHandler.listen(bulkLoader, EventType.ERROR, handleError);

  return bulkLoader;
}

function handleSuccess(e) {
  loadSuccess = true;
  successResponseTexts = e.target.getResponseTexts();
}

function handleError(e) {
  loadError = true;
  errorStatus = e.status;
}

testSuite({
  setUpPage() {
    clock = new MockClock(true);
  },

  tearDownPage() {
    clock.dispose();
  },

  setUp() {
    loadSuccess = false;
    loadError = false;
    successResponseTexts = [];
  },

  /** Test successful loading of URIs using the bulkloader. */
  testBulkLoaderLoadSuccess() {
    const uris = ['a', 'b', 'c'];
    const bulkLoader = getSuccessfulBulkLoader(uris);
    assertArrayEquals(uris, bulkLoader.getRequestUris());

    bulkLoader.load();

    clock.tick(2);
    assertFalse(
        'The bulk loader is not yet loaded (after 2 ticks)', loadSuccess);

    clock.tick(3);
    assertFalse(
        'The bulk loader is not yet loaded (after 5 ticks)', loadSuccess);

    clock.tick(5);
    assertFalse(
        'The bulk loader is not yet loaded (after 10 ticks)', loadSuccess);

    clock.tick(5);
    assertTrue('The bulk loader is loaded (after 15 ticks)', loadSuccess);

    assertArrayEquals(
        'Ensure that the response texts are present', successResponseTexts,
        uris);
  },

  /** Test error loading URIs using the bulkloader. */
  testBulkLoaderLoadError() {
    const uris = ['a', 'b', 'c'];
    const bulkLoader = getNonSuccessfulBulkLoader(uris);

    bulkLoader.load();

    clock.tick(2);
    assertFalse('The bulk loader is not yet loaded (after 2 ticks)', loadError);

    clock.tick(3);
    assertFalse('The bulk loader is not yet loaded (after 5 ticks)', loadError);

    clock.tick(5);
    assertFalse(
        'The bulk loader is not yet loaded (after 10 ticks)', loadError);

    clock.tick(5);
    assertFalse(
        'The bulk loader is not loaded successfully (after 15 ticks)',
        loadSuccess);
    assertTrue(
        'The bulk loader is loaded in error (after 15 ticks)', loadError);
    assertEquals('Ensure that the error status is present', 500, errorStatus);
  },
});
