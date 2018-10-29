// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!FileTapHandler} handler the handler. */
var handler;

/**
 * @type {!Element}
 */
var dummyTarget;

/**
 * @type {!Array<!Object>}
 */
var events;

/**
 * @type {function(!Event, number, !FileTapHandler.TapEvent)}
 */
var handleTap = function(e, index, eventType) {
  events.push({index: index, eventType: eventType});
  return false;
};

/**
 * @param {number} identifier
 * @param {number} clientX
 * @param {number} clientY
 */
function createTouch(identifier, clientX, clientY) {
  return new Touch({
    identifier: identifier,
    clientX: clientX,
    clientY: clientY,
    target: dummyTarget
  });
}

function setUp() {
  handler = new FileTapHandler();
  dummyTarget = document.body;
  events = [];
}

function testTap() {
  var touch = createTouch(0, 300, 400);
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {targetTouches: [touch], touches: [touch]}),
      0, handleTap);
  // Callback should be called after touchend.
  assertEquals(0, events.length);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        targetTouches: [],
        touches: [],
      }),
      0, handleTap);
  assertEquals(1, events.length);
  assertEquals(FileTapHandler.TapEvent.TAP, events[0].eventType);
  assertEquals(0, events[0].index);
}

function testIgnoreSlide() {
  var touch0 = createTouch(0, 300, 400);
  var touch1 = createTouch(0, 320, 450);
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        targetTouches: [touch0],
        touches: [touch0],
        changedTouches: [touch0],
      }),
      0, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        targetTouches: [touch1],
        touches: [touch1],
        changedTouches: [touch1],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent(
          'touchend',
          {changedTouches: [touch1], targetTouches: [], touches: []}),
      1, handleTap);
  assertEquals(0, events.length);

  // Next touch should be accepted.
  var touch2 = createTouch(0, touch0.clientX + 1, touch0.clientY + 2);
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        targetTouches: [touch0],
        touches: [touch0],
        changedTouches: [touch0],
      }),
      0, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        targetTouches: [touch2],
        touches: [touch2],
        changedTouches: [touch2],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent(
          'touchend',
          {changedTouches: [touch2], targetTouches: [], touches: []}),
      1, handleTap);
  assertEquals(1, events.length);
  assertEquals(FileTapHandler.TapEvent.TAP, events[0].eventType);
}

function testTapMoveTolerance() {
  var touch0 = createTouch(0, 300, 400);
  var touch1 = createTouch(0, 300, 405);  // moved slightly
  var touch2 = createTouch(0, 302, 405);  // moved slightly
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        changedTouches: [touch0],
        targetTouches: [touch0],
        touches: [touch0],
      }),
      0, handleTap);
  // Emulate touching another item in the list due to the small slide.
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        changedTouches: [touch1],
        targetTouches: [touch1],
        touches: [touch1],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        changedTouches: [touch2],
        targetTouches: [touch2],
        touches: [touch2],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        changedTouches: [touch2],
        targetTouches: [],
        touches: [],
      }),
      1, handleTap);
  assertEquals(1, events.length);
  assertEquals(FileTapHandler.TapEvent.TAP, events[0].eventType);
  assertEquals(0, events[0].index);
}

function testLongTap(callback) {
  var touch0 = createTouch(0, 300, 400);
  var touch1 = createTouch(0, 303, 404);
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        changedTouches: [touch0],
        targetTouches: [touch0],
        touches: [touch0]
      }),
      0, handleTap);
  assertEquals(0, events.length);
  reportPromise(
      new Promise(function(resolve) {
        setTimeout(resolve, 250);
      })
          .then(function() {
            // Move slightly, but still touching.
            handler.handleTouchEvents(
                new TouchEvent('touchmove', {
                  changedTouches: [touch1],
                  targetTouches: [touch1],
                  touches: [touch1]
                }),
                0, handleTap);
            return new Promise(function(resolve) {
              // Exceeds the threshold (500ms) when added with the one above.
              setTimeout(resolve, 300);
            });
          })
          .then(function() {
            assertEquals(1, events.length);
            assertEquals(
                FileTapHandler.TapEvent.LONG_PRESS, events[0].eventType);
            assertEquals(0, events[0].index);
            handler.handleTouchEvents(
                new TouchEvent('touchend', {
                  changedTouches: [touch1],
                  targetTouches: [],
                  touches: [],
                }),
                1, handleTap);
            assertEquals(2, events.length);
            assertEquals(FileTapHandler.TapEvent.LONG_TAP, events[1].eventType);
            assertEquals(0, events[1].index);
          }),
      callback);
}

function testCancelLongTapBySlide(callback) {
  var touch0 = createTouch(0, 300, 400);
  var touch1 = createTouch(0, 330, 450);
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        changedTouches: [touch0],
        targetTouches: [touch0],
        touches: [touch0]
      }),
      0, handleTap);
  assertEquals(0, events.length);
  reportPromise(
      new Promise(function(resolve) {
        setTimeout(resolve, 250);
      })
          .then(function() {
            handler.handleTouchEvents(
                new TouchEvent('touchmove', {
                  changedTouches: [touch1],
                  targetTouches: [touch1],
                  touches: [touch1]
                }),
                0, handleTap);
            return new Promise(function(resolve) {
              // Exceeds the threshold (500ms) when added with the one above.
              setTimeout(resolve, 300);
            });
          })
          .then(function() {
            assertEquals(0, events.length);
          }),
      callback);
}

function testTwoFingerTap() {
  var touch0_0 = createTouch(0, 300, 400);
  var touch0_1 = createTouch(0, 303, 404);
  var touch1_0 = createTouch(1, 350, 400);
  var touch1_1 = createTouch(1, 354, 402);
  // case 1: Release the second touch point first.
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        changedTouches: [touch0_0],
        targetTouches: [touch0_0],
        touches: [touch0_0],
      }),
      0, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        changedTouches: [touch1_0],
        targetTouches: [touch0_0, touch1_0],
        touches: [touch0_0, touch1_0],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        changedTouches: [touch1_1],
        targetTouches: [touch0_0, touch1_1],
        touches: [touch0_0, touch1_1],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        changedTouches: [touch1_1],
        targetTouches: [touch0_0],
        touches: [touch0_0]
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        changedTouches: [touch0_1],
        targetTouches: [touch0_1],
        touches: [touch0_1],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent(
          'touchend',
          {changedTouches: [touch0_1], targetTouches: [], touches: []}),
      2, handleTap);
  assertEquals(1, events.length);
  assertEquals(FileTapHandler.TapEvent.TWO_FINGER_TAP, events[0].eventType);
  assertEquals(0, events[0].index);

  // case 2: Release the first touch point first.
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        changedTouches: [touch0_0],
        targetTouches: [touch0_0],
        touches: [touch0_0],
      }),
      10, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        changedTouches: [touch1_0],
        targetTouches: [touch0_0, touch1_0],
        touches: [touch0_0, touch1_0],
      }),
      11, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        changedTouches: [touch0_0],
        targetTouches: [touch1_0],
        touches: [touch1_0]
      }),
      11, handleTap);
  handler.handleTouchEvents(
      new TouchEvent(
          'touchend',
          {changedTouches: [touch1_0], targetTouches: [], touches: []}),
      10, handleTap);
  assertEquals(2, events.length);
  assertEquals(FileTapHandler.TapEvent.TWO_FINGER_TAP, events[1].eventType);
  assertEquals(10, events[1].index);
}
