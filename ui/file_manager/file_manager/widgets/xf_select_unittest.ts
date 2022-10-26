// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {SELECTION_CHANGED, XfSelect} from './xf_select.js';

/**
 * Creates new <xf-search-options> element for each test.
 */
export function setUp() {
  document.body.innerHTML = '<xf-select></xf-select>';
}

/**
 * Returns the <xf-search-options> element.
 */
function getSearchOptionsElement(): XfSelect {
  return document.querySelector<XfSelect>('xf-select')!;
}

export function testElementCreated() {
  const element = getSearchOptionsElement();
  assertEquals('XF-SELECT', element.tagName);
}

export function testSetOptions() {
  const element = getSearchOptionsElement();
  element.setOptions([
    {value: 'value-a', text: 'Text A'},
    {value: 'value-b', text: 'Text B', default: true},
  ]);

  const want = {index: 1, value: 'value-b', text: 'Text B'};
  const got = element.getSelectedOption();

  assertDeepEquals(
      want, got, `${JSON.stringify(want)} != ${JSON.stringify(got)}`);
}

export async function testEvents(done: () => void) {
  const element = getSearchOptionsElement();
  element.setOptions([
    {value: 'value-a', text: 'Text A'},
    {value: 'value-b', text: 'Text B', default: true},
  ]);

  element.addEventListener(SELECTION_CHANGED, (event) => {
    const want = {index: 0, value: 'value-a', text: 'Text A'};
    const got = event.detail;
    assertDeepEquals(
        want, got, `${JSON.stringify(want)} != ${JSON.stringify(got)}`);
    done();
  });

  element.value = 'value-a';
}
