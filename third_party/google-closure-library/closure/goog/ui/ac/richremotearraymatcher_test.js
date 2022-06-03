/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} Stubbing goog.net.XhrIo
 */

goog.module('goog.ui.ac.RichRemoteArrayMatcherTest');
goog.setTestOnly();

const ArgumentMatcher = goog.require('goog.testing.mockmatchers.ArgumentMatcher');
const MockControl = goog.require('goog.testing.MockControl');
const NetXhrIo = goog.require('goog.testing.net.XhrIo');
const RichRemoteArrayMatcher = goog.require('goog.ui.ac.RichRemoteArrayMatcher');
/** @suppress {extraRequire} */
const XhrIo = goog.require('goog.net.XhrIo');
const testSuite = goog.require('goog.testing.testSuite');

const url = 'http://www.google.com';
const token = 'goog';
const maxMatches = 5;

const responseJsonText =
    '[["type1", {"name":"eric"}, {"name":"larry"}, {"name":"sergey"}]]';
// This matcher is used to match the value used in the `matchHandler` callback
// in tests.
// The `RichRemoteArrayMatcher` takes in the parsed `responseJsonTest`
// above and augments each object within the array with methods that it defines.
// By default mocks do === comparison between the expected and actual value,
// so to avoid copying those method implementations into the test, we instead
// implement a matcher that checks to see that the value given to the callback
// is roughly what we expected it to be: an array whose objects have the names
// listed above.
// Effectively, this is structurally matching the following:
// [{name: 'eric'},{name:'larry'},{name:'sergey'}]
const ignoresRenderAndSelectMatcher = new ArgumentMatcher((arg) => {
  if (!Array.isArray(arg)) {
    return false;
  }
  return arg[0].name === 'eric' && arg[1].name === 'larry' &&
      arg[2].name === 'sergey';
}, 'matchesType1');

let mockControl;
let mockMatchHandler;

/**
 * Callback for type1 responses.
 * @param {string} response
 * @return {string}
 */
function type1(response) {
  return response;
}

testSuite({
  setUp() {
    goog.net.XhrIo = /** @type {?} */ (NetXhrIo);
    mockControl = new MockControl();
    mockMatchHandler = mockControl.createFunctionMock();
  },

  /**
     @suppress {checkTypes,visibility,strictMissingProperties} suppression
     added to enable type checking
   */
  testRequestMatchingRows() {
    const matcher = new RichRemoteArrayMatcher(url);
    mockMatchHandler(token, ignoresRenderAndSelectMatcher);
    mockControl.$replayAll();
    matcher.requestMatchingRows(token, maxMatches, mockMatchHandler);
    matcher.xhr_.simulateResponse(200, responseJsonText);
    mockControl.$verifyAll();
    mockControl.$resetAll();
  },

  /**
     @suppress {checkTypes,visibility,strictMissingProperties} suppression
     added to enable type checking
   */
  testSetRowBuilder() {
    const matcher = new RichRemoteArrayMatcher(url);
    matcher.setRowBuilder(/**
                             @suppress {checkTypes} suppression added to enable
                             type checking
                           */
                          (type, response) => {
                            assertEquals('type1', type);
                            return response;
                          });
    mockMatchHandler(token, ignoresRenderAndSelectMatcher);
    mockControl.$replayAll();
    matcher.requestMatchingRows(token, maxMatches, mockMatchHandler);
    matcher.xhr_.simulateResponse(200, responseJsonText);
    mockControl.$verifyAll();
    mockControl.$resetAll();
  },
});
