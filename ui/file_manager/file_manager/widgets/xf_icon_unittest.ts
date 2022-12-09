// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';

import {XfIcon} from './xf_icon.js';

export function setUp() {
  document.body.innerHTML = '<xf-icon></xf-icon>';
}

async function getIcon(): Promise<XfIcon> {
  const element = document.querySelector('xf-icon');
  assertNotEquals(null, element);
  await waitForElementUpdate(element!);
  return element!;
}

function getSpanFromIcon(icon: XfIcon): HTMLSpanElement {
  return icon.shadowRoot!.querySelector<HTMLSpanElement>('span')!;
}

export async function testIconType(done: () => void) {
  const icon = await getIcon();
  const span = getSpanFromIcon(icon);

  // Check for all office icons, there should be a keep-color class.
  icon.type = XfIcon.types.WORD;
  await waitForElementUpdate(icon);
  assertTrue(span.classList.contains('keep-color'));

  icon.type = XfIcon.types.EXCEL;
  await waitForElementUpdate(icon);
  assertTrue(span.classList.contains('keep-color'));

  icon.type = XfIcon.types.POWERPOINT;
  await waitForElementUpdate(icon);
  assertTrue(span.classList.contains('keep-color'));

  // Check no keep-color class for other icon types.
  icon.type = XfIcon.types.ANDROID_FILES;
  await waitForElementUpdate(icon);
  assertFalse(span.classList.contains('keep-color'));

  done();
}

export async function testIconSize(done: () => void) {
  const icon = await getIcon();
  const span = getSpanFromIcon(icon);

  // By default the size should be small.
  assertEquals(XfIcon.sizes.SMALL, icon.size);
  assertEquals('20px', window.getComputedStyle(span).width);
  assertEquals('20px', window.getComputedStyle(span).height);

  // Check large size should change the width/height.
  icon.size = XfIcon.sizes.LARGE;
  await waitForElementUpdate(icon);
  assertEquals('48px', window.getComputedStyle(span).width);
  assertEquals('48px', window.getComputedStyle(span).height);

  done();
}
