// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';

import type {CanExecuteEvent} from './command.js';
import {Command} from './command.js';


export function setUp() {
  const cmd = document.createElement('command');
  cmd.setAttribute('shortcut', 'n|Ctrl');
  document.body.appendChild(cmd);
}

export function testCommandDefaultPrevented() {
  let calls = 0;
  document.addEventListener('canExecute', (event) => {
    const e = event as CanExecuteEvent;
    ++calls;
    assertFalse(e.defaultPrevented);
    e.canExecute = true;
    assertTrue(e.defaultPrevented);
  });
  const command = document.querySelector<Command>('command')!;
  crInjectTypeAndInit(command, Command);
  command.canExecuteChange();
  assertEquals(1, calls);
}

function createEvent(key: string, code: string, keyCode: number): Event {
  return {
    key: key,
    code: code,
    keyCode: keyCode,
    altKey: false,
    ctrlKey: true,
    metaKey: false,
    shiftKey: false,
  } as unknown as Event;
}

export function testShortcuts() {
  const cmd = document.querySelector<Command>('command')!;
  crInjectTypeAndInit(cmd, Command);
  // US keyboard - qwerty-N should work.
  assertTrue(cmd.matchesEvent(createEvent('n', 'KeyN', 0x4e)));
  // DV keyboard - qwerty-L (dvorak-N) should work.
  assertTrue(cmd.matchesEvent(createEvent('n', 'KeyL', 0x4e)));
  // DV keyboard - qwerty-N (dvorak-B) should not work.
  assertFalse(cmd.matchesEvent(createEvent('b', 'KeyN', 0x42)));
  // RU keyboard - qwerty-N (Cyrillic Te) should work.
  assertTrue(cmd.matchesEvent(createEvent('т', 'KeyN', 0x4e)));
}
