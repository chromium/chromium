// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {FileTapHandler, TapEvent} from './file_tap_handler.js';

let handler: FileTapHandler;

let dummyTarget: Element;

let events: Array<{index: number, eventType: TapEvent}>;

const handleTap =
    (_e: TouchEvent, index: number, eventType: TapEvent): boolean => {
      events.push({index, eventType});
      return false;
    };

function createTouch(identifier: number, clientX: number, clientY: number) {
  return new Touch({
    identifier,
    clientX,
    clientY,
    target: dummyTarget,
  });
}

export function setUp() {
  handler = new FileTapHandler();
  dummyTarget = document.body;
  events = [];
}

export function testTap() {
  const touch = createTouch(0, 300, 400);

  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        cancelable: true,
        targetTouches: [touch],
        touches: [touch],
      }),
      0, handleTap);
  // Callback should be called after touchend.
  assertEquals(0, events.length);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        cancelable: true,
        targetTouches: [],
        touches: [],
      }),
      0, handleTap);

  // A tap event should be emitted for a single tap.
  assertEquals(1, events.length);
  assertEquals(TapEvent.TAP, events[0]!.eventType);
  assertEquals(0, events[0]!.index);
}

export function testTapMoveTolerance() {
  const touch0 = createTouch(0, 300, 400);
  const touch1 = createTouch(0, 303, 404);  // moved slightly

  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        cancelable: true,
        changedTouches: [touch0],
        targetTouches: [touch0],
        touches: [touch0],
      }),
      0, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        cancelable: true,
        changedTouches: [touch1],
        targetTouches: [touch1],
        touches: [touch1],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        cancelable: true,
        changedTouches: [touch1],
        targetTouches: [],
        touches: [],
      }),
      1, handleTap);

  // No tap event should be emitted for a single tap with movement.
  assertEquals(0, events.length);
}

export async function testLongTap() {
  const touch0 = createTouch(0, 300, 400);
  const touch1 = createTouch(0, 300, 400);  // no movement.

  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        cancelable: true,
        changedTouches: [touch0],
        targetTouches: [touch0],
        touches: [touch0],
      }),
      0, handleTap);
  assertEquals(0, events.length);
  await new Promise(resolve => {
    // Wait for the long press threshold (500ms). No movement.
    setTimeout(resolve, 550);
  });
  // A long press should be emitted if there was no movement.
  assertEquals(1, events.length);
  assertEquals(TapEvent.LONG_PRESS, events[0]!.eventType);
  assertEquals(0, events[0]!.index);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        cancelable: true,
        changedTouches: [touch1],
        targetTouches: [],
        touches: [],
      }),
      1, handleTap);
  // A long tap should be emitted if there was no movement.
  assertEquals(2, events.length);
  assertEquals(TapEvent.LONG_TAP, events[1]!.eventType);
  assertEquals(0, events[1]!.index);
}

export async function testLongTapMoveTolerance() {
  const touch0 = createTouch(0, 300, 400);
  const touch1 = createTouch(0, 303, 404);  // moved slightly

  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        cancelable: true,
        changedTouches: [touch0],
        targetTouches: [touch0],
        touches: [touch0],
      }),
      0, handleTap);
  assertEquals(0, events.length);
  await new Promise(resolve => {
    setTimeout(resolve, 250);
  });
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        cancelable: true,
        changedTouches: [touch1],
        targetTouches: [touch1],
        touches: [touch1],
      }),
      0, handleTap);
  await new Promise(resolve => {
    // Exceeds the threshold (500ms) when added with the one above.
    setTimeout(resolve, 300);
  });

  // No tap event should be emitted for a long tap with movement.
  assertEquals(0, events.length);
}

export function testTwoFingerTap() {
  const touch0_0 = createTouch(0, 300, 400);
  const touch0_1 = createTouch(0, 303, 404);
  const touch1_0 = createTouch(1, 350, 400);
  const touch1_1 = createTouch(1, 354, 402);

  // case 1: Release the second touch point first.
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        cancelable: true,
        changedTouches: [touch0_0],
        targetTouches: [touch0_0],
        touches: [touch0_0],
      }),
      0, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        cancelable: true,
        changedTouches: [touch1_0],
        targetTouches: [touch0_0, touch1_0],
        touches: [touch0_0, touch1_0],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        cancelable: true,
        changedTouches: [touch1_1],
        targetTouches: [touch0_0, touch1_1],
        touches: [touch0_0, touch1_1],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        cancelable: true,
        changedTouches: [touch1_1],
        targetTouches: [touch0_0],
        touches: [touch0_0],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchmove', {
        cancelable: true,
        changedTouches: [touch0_1],
        targetTouches: [touch0_1],
        touches: [touch0_1],
      }),
      1, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        cancelable: true,
        changedTouches: [touch0_1],
        targetTouches: [],
        touches: [],
      }),
      2, handleTap);

  // A two-finger tap event should be emitted, allowing for slight movement.
  assertEquals(1, events.length);
  assertEquals(TapEvent.TWO_FINGER_TAP, events[0]!.eventType);
  assertEquals(0, events[0]!.index);

  // case 2: Release the first touch point first.
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        cancelable: true,
        changedTouches: [touch0_0],
        targetTouches: [touch0_0],
        touches: [touch0_0],
      }),
      10, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchstart', {
        cancelable: true,
        changedTouches: [touch1_0],
        targetTouches: [touch0_0, touch1_0],
        touches: [touch0_0, touch1_0],
      }),
      11, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        cancelable: true,
        changedTouches: [touch0_0],
        targetTouches: [touch1_0],
        touches: [touch1_0],
      }),
      11, handleTap);
  handler.handleTouchEvents(
      new TouchEvent('touchend', {
        cancelable: true,
        changedTouches: [touch1_0],
        targetTouches: [],
        touches: [],
      }),
      10, handleTap);

  // A two-finger tap event should be emitted.
  assertEquals(2, events.length);
  assertEquals(TapEvent.TWO_FINGER_TAP, events[1]!.eventType);
  assertEquals(10, events[1]!.index);
}
