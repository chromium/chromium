// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';

import {XfSelect} from './xf_select.js';

/**
 * Creates new <xf-search-options> element for each test.
 */
export function setUp(): void {
  document.body.innerHTML = getTrustedHTML`
  <xf-select></xf-select>
`;
}

/**
 * Returns the <xf-search-options> element.
 */
function getSearchOptionsElement(): XfSelect {
  return document.querySelector<XfSelect>('xf-select')!;
}

export function testElementCreated(): void {
  const element = getSearchOptionsElement();
  assertEquals('XF-SELECT', element.tagName);
}

export async function testSetOptions(): Promise<void> {
  const element = getSearchOptionsElement();

  // Test 1: Expect the option with default: true to be the selected one.
  element.options = [
    {value: 'value-a', text: 'Text A'},
    {value: 'value-b', text: 'Text B', default: true},
  ];

  await waitForElementUpdate(element);
  const want1 = {index: 1, value: 'value-b', text: 'Text B'};
  const got1 = element.getSelectedOption();

  assertDeepEquals(
      want1, got1, `${JSON.stringify(want1)} !== ${JSON.stringify(got1)}`);

  // Test 2: No option has default: true; expect the first one to be selected.
  element.options = [
    {value: 'value-c', text: 'Text C'},
    {value: 'value-d', text: 'Text D'},
  ];

  await waitForElementUpdate(element);
  const want2 = {index: 0, value: 'value-c', text: 'Text C'};
  const got2 = element.getSelectedOption();

  assertDeepEquals(
      want2, got2, `${JSON.stringify(want2)} !== ${JSON.stringify(got2)}`);
}

export async function testEvents(): Promise<void> {
  const element = getSearchOptionsElement();
  element.options = [
    {value: 'value-a', text: 'Text A'},
    {value: 'value-b', text: 'Text B', default: true},
  ];

  const selectionChangedPromise =
      eventToPromise(XfSelect.events.SELECTION_CHANGED, element);

  element.value = 'value-a';
  const event = await selectionChangedPromise;
  const want = {index: 0, value: 'value-a', text: 'Text A'};
  const got = event.detail;
  assertDeepEquals(
      want, got, `${JSON.stringify(want)} !== ${JSON.stringify(got)}`);
}

export async function testInteractionViaUI(): Promise<void> {
  const element = getSearchOptionsElement();
  element.options = [
    {value: 'value-a', text: 'Text A'},
    {value: 'value-b', text: 'Text B', default: true},
  ];

  const selectionChangedDueOptionsPromise =
      eventToPromise(XfSelect.events.SELECTION_CHANGED, element);
  await selectionChangedDueOptionsPromise;

  const selectionChangedDueMenuClickPromise =
      eventToPromise(XfSelect.events.SELECTION_CHANGED, element);

  // Open.
  element.click();
  // Click the first option.
  const menuButtons: HTMLButtonElement[] = Array.from(
      element.shadowRoot!.querySelectorAll('cr-action-menu cr-button'));
  assertEquals(2, menuButtons.length);
  menuButtons[0]?.click();

  const event = await selectionChangedDueMenuClickPromise;
  const want = {index: 0, value: 'value-a', text: 'Text A'};
  const got = event.detail;
  assertDeepEquals(
      want, got, `${JSON.stringify(want)} !== ${JSON.stringify(got)}`);
}
