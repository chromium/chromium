/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.style.cursorTest');
goog.setTestOnly();

const cursor = goog.require('goog.style.cursor');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

const baseCursorUrl = '/images/2/';
let origWindowsUserAgentValue;
let origGeckoUserAgentValue;
let origWebkitUserAgentValue;

testSuite({
  setUp() {
    origWindowsUserAgentValue = userAgent.WINDOWS;
    origGeckoUserAgentValue = userAgent.GECKO;
    origWebkitUserAgentValue = userAgent.WEBKIT;
  },

  tearDown() {
    userAgent.WINDOWS = origWindowsUserAgentValue;
    userAgent.GECKO = origGeckoUserAgentValue;
    userAgent.WEBKIT = origWebkitUserAgentValue;
  },

  testGetCursorStylesWebkit() {
    userAgent.GECKO = false;
    userAgent.WEBKIT = true;

    assertEquals(
        'Webkit should get a cursor style with moved hot-spot.',
        'url("/images/2/openhand.cur") 7 5, default',
        cursor.getDraggableCursorStyle(baseCursorUrl));
    assertEquals(
        'Webkit should get a cursor style with moved hot-spot.',
        'url("/images/2/openhand.cur") 7 5, default',
        cursor.getDraggableCursorStyle(baseCursorUrl, true));

    assertEquals(
        'Webkit should get a cursor style with moved hot-spot.',
        'url("/images/2/closedhand.cur") 7 5, move',
        cursor.getDraggingCursorStyle(baseCursorUrl));
    assertEquals(
        'Webkit should get a cursor style with moved hot-spot.',
        'url("/images/2/closedhand.cur") 7 5, move',
        cursor.getDraggingCursorStyle(baseCursorUrl, true));
  },

  testGetCursorStylesFireFoxNonWin() {
    userAgent.GECKO = true;
    userAgent.WEBKIT = false;
    userAgent.WINDOWS = false;

    assertEquals(
        'FireFox on non Windows should get a custom cursor style.', '-moz-grab',
        cursor.getDraggableCursorStyle(baseCursorUrl));
    assertEquals(
        'FireFox on non Windows should get a custom cursor style and ' +
            'no !important modifier.',
        '-moz-grab', cursor.getDraggableCursorStyle(baseCursorUrl, true));

    assertEquals(
        'FireFox on non Windows should get a custom cursor style.',
        '-moz-grabbing', cursor.getDraggingCursorStyle(baseCursorUrl));
    assertEquals(
        'FireFox on non Windows should get a custom cursor style and ' +
            'no !important modifier.',
        '-moz-grabbing', cursor.getDraggingCursorStyle(baseCursorUrl, true));
  },

  testGetCursorStylesFireFoxWin() {
    userAgent.GECKO = true;
    userAgent.WEBKIT = false;
    userAgent.WINDOWS = true;

    assertEquals(
        'FireFox should get a cursor style with URL.',
        'url("/images/2/openhand.cur"), default',
        cursor.getDraggableCursorStyle(baseCursorUrl));
    assertEquals(
        'FireFox should get a cursor style with URL and no !important' +
            ' modifier.',
        'url("/images/2/openhand.cur"), default',
        cursor.getDraggableCursorStyle(baseCursorUrl, true));

    assertEquals(
        'FireFox should get a cursor style with URL.',
        'url("/images/2/closedhand.cur"), move',
        cursor.getDraggingCursorStyle(baseCursorUrl));
    assertEquals(
        'FireFox should get a cursor style with URL and no !important' +
            ' modifier.',
        'url("/images/2/closedhand.cur"), move',
        cursor.getDraggingCursorStyle(baseCursorUrl, true));
  },

  testGetCursorStylesOther() {
    userAgent.GECKO = false;
    userAgent.WEBKIT = false;

    assertEquals(
        'Other browsers (IE) should get a cursor style with URL.',
        'url("/images/2/openhand.cur"), default',
        cursor.getDraggableCursorStyle(baseCursorUrl));
    assertEquals(
        'Other browsers (IE) should get a cursor style with URL.',
        'url("/images/2/openhand.cur"), default',
        cursor.getDraggableCursorStyle(baseCursorUrl, true));

    assertEquals(
        'Other browsers (IE) should get a cursor style with URL.',
        'url("/images/2/closedhand.cur"), move',
        cursor.getDraggingCursorStyle(baseCursorUrl));
    assertEquals(
        'Other browsers (IE) should get a cursor style with URL.',
        'url("/images/2/closedhand.cur"), move',
        cursor.getDraggingCursorStyle(baseCursorUrl, true));
  },
});
