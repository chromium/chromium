// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/ash/common/util.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';

import {Splitter} from './splitter.js';


export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <div id="previous"></div>
    <div id="splitter"></div>
    <div id="next"></div>
  `;
}

export function testSplitterIgnoresRightMouse() {
  const splitter = getRequiredElement('splitter');
  crInjectTypeAndInit(splitter, Splitter);

  const downRight = new MouseEvent('mousedown', {button: 1, cancelable: true});
  assertTrue(splitter.dispatchEvent(downRight));
  assertFalse(downRight.defaultPrevented);

  const downLeft = new MouseEvent('mousedown', {button: 0, cancelable: true});
  assertFalse(splitter.dispatchEvent(downLeft));
  assertTrue(downLeft.defaultPrevented);
}

export function testSplitterResizePreviousElement() {
  const splitter = getRequiredElement('splitter') as Splitter;
  crInjectTypeAndInit(splitter, Splitter);

  splitter.resizeNextElement = false;

  const previousElement = document.getElementById('previous')!;
  previousElement.style.width = '0px';
  const beforeWidth = parseFloat(previousElement.style.width);

  const down =
      new MouseEvent('mousedown', {button: 0, cancelable: true, clientX: 0});
  splitter.dispatchEvent(down);

  let move =
      new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 50});
  splitter.dispatchEvent(move);

  move =
      new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 100});
  splitter.dispatchEvent(move);

  const up =
      new MouseEvent('mouseup', {button: 0, cancelable: true, clientX: 100});
  splitter.dispatchEvent(up);

  const afterWidth = parseFloat(previousElement.style.width);
  assertEquals(100, afterWidth - beforeWidth);
}

export function testSplitterResizeNextElement() {
  const splitter = getRequiredElement('splitter') as Splitter;
  crInjectTypeAndInit(splitter, Splitter);
  splitter.resizeNextElement = true;
  const nextElement = document.getElementById('next')!;
  nextElement.style.width = '0px';
  const beforeWidth = parseFloat(nextElement.style.width);

  const down =
      new MouseEvent('mousedown', {button: 0, cancelable: true, clientX: 100});
  splitter.dispatchEvent(down);

  let move =
      new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 50});
  splitter.dispatchEvent(move);

  move = new MouseEvent('mousemove', {button: 0, cancelable: true, clientX: 0});
  splitter.dispatchEvent(move);

  const up =
      new MouseEvent('mouseup', {button: 0, cancelable: true, clientX: 0});
  splitter.dispatchEvent(up);

  const afterWidth = parseFloat(nextElement.style.width);
  assertEquals(100, afterWidth - beforeWidth);
}
