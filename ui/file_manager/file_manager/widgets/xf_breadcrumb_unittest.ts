// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertEquals, assertFalse, assertGE, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {waitUntil} from '../common/js/test_error_reporting.js';

import {BREADCRUMB_CLICKED, BreadcrumbClickedEvent, XfBreadcrumb} from './xf_breadcrumb.js';

/**
 * Creates new <xf-breadcrumb> element for each test. Asserts it has no initial
 * path using the element.path getter.
 */
export function setUp() {
  document.body.innerHTML = '<xf-breadcrumb></xf-breadcrumb>';
  const breadcrumb = document.querySelector('xf-breadcrumb');
  assertEquals('', breadcrumb!.path);
}

/** Returns the <xf-breadcrumb> element. */
function getBreadcrumb(): XfBreadcrumb {
  const element = document.querySelector('xf-breadcrumb');
  assertNotEquals('none', window.getComputedStyle(element!).display);
  assertFalse(element!.hasAttribute('hidden'));
  return element!;
}

/**
 * Returns the <xf-breadcrumb> child button elements. There are 4 main buttons
 * and one elider button (so at least 5) plus optional drop-down menu buttons.
 */
function getAllBreadcrumbButtons(): HTMLButtonElement[] {
  const buttons = getBreadcrumb().shadowRoot!.querySelectorAll('button');
  assertGE(buttons.length, 5, 'too few buttons');
  return Array.from(buttons) as HTMLButtonElement[];
}

/**
 * Returns the not-hidden <xf-breadcrumb> main button elements. The breadcrumb
 * main buttons have an id, all other breadcrumb buttons do not.
 */
function getVisibleBreadcrumbMainButtons(): HTMLButtonElement[] {
  const notHiddenMain = 'button[id]:not([hidden])';
  const buttons = getBreadcrumb().shadowRoot!.querySelectorAll(notHiddenMain);
  return Array.from(buttons) as HTMLButtonElement[];
}

/** Returns the last not-hidden <xf-breadcrumb> main button element. */
function getLastVisibleBreadcrumbMainButton(): HTMLButtonElement {
  return getVisibleBreadcrumbMainButtons().pop() as HTMLButtonElement;
}

/** Returns the <xf-breadcrumb> elider button element. */
function getBreadcrumbEliderButton(): HTMLButtonElement {
  const elider = 'button[elider]';
  const button = getBreadcrumb().shadowRoot!.querySelectorAll(elider);
  assertEquals(1, button.length, 'invalid elider button');
  return button[0] as HTMLButtonElement;
}

/** Returns the <xf-breadcrumb> drop-down menu button elements. */
function getBreadcrumbMenuButtons(): HTMLButtonElement[] {
  const menuButton = 'cr-action-menu button';
  const buttons = getBreadcrumb().shadowRoot!.querySelectorAll(menuButton);
  return Array.from(buttons) as HTMLButtonElement[];
}

/**
 * Returns <xf-breadcrumb> main button visual state.
 * @param button Main button (these have an id).
 * @param i Number to assign to the button.
 */
function getMainButtonState(button: HTMLButtonElement, i: number): string {
  const display = window.getComputedStyle(button).display;

  let result = i + ': display:' + display + ' id=' + button.id;
  if (!button.hasAttribute('hidden')) {
    result += ' text=[' + button.textContent + ']';
  } else {
    assertEquals('none', display);
    result += ' hidden';
  }

  assertTrue(!!(button.id));
  return result;
}

/**
 * Returns <xf-breadcrumb> elider button visual state.
 * @param button Elider button.
 * @param i Number to assign to the button.
 */
function getEliderButtonState(button: HTMLButtonElement, i: number): string {
  const display = window.getComputedStyle(button).display;

  const result = i + ': display:' + display;
  const attributes: string[] = [];
  for (const value of button.getAttributeNames().values()) {
    if (value === 'aria-expanded') {  // drop-down menu: opened || closed
      attributes.push(value + '=' + button.getAttribute('aria-expanded'));
    } else if (value === 'hidden') {
      assertEquals('none', display);
      attributes.push(value);
    } else if (value !== 'elider') {
      attributes.push(value);
    }
  }

  assertFalse(!!button.id, 'elider button should not have an id');
  assertTrue(button.hasAttribute('elider'));
  return result + ' elider[' + attributes.sort() + ']';
}

/**
 * Returns <xf-breadcrumb> drop-down menu button visual state.
 * @param button Drop-down menu button.
 */
function getDropDownMenuButtonState(button: HTMLButtonElement): string {
  const display = window.getComputedStyle(button).display;

  let result = `${button.classList.toString()}: display:` + display;
  if (!button.hasAttribute('hidden')) {
    result += ' text=[' + button.textContent + ']';
  } else {
    assertEquals('none', display);
    result += ' hidden';
  }

  assertFalse(!!button.id, 'drop-down buttons should not have an id');
  assertTrue(button.classList.contains('dropdown-item'));
  return result;
}

/** Returns the <xf-breadcrumb> buttons visual state. */
function getBreadcrumbButtonState(): string {
  const parts: string[] = [];
  const menus: string[] = [];

  const buttons = getAllBreadcrumbButtons();
  let number = 0;
  buttons.forEach((button) => {
    if (button.id) {  // Main buttons have an id.
      parts.push(getMainButtonState(button, ++number));
    } else if (button.hasAttribute('elider')) {  // Elider button.
      parts.push(getEliderButtonState(button, ++number));
    } else {  // A drop-down menu button.
      menus.push(getDropDownMenuButtonState(button));
    }
  });

  // Elider should only display for paths with more than 4 parts.
  if (!getBreadcrumbEliderButton().hasAttribute('hidden')) {
    assertGT(getBreadcrumb().parts.length, 4);
  }

  // The 'last' main button displayed should always be [disabled].
  const last = getLastVisibleBreadcrumbMainButton();
  if (getBreadcrumb().path !== '') {
    assertTrue(last.hasAttribute('disabled'));
  }

  if (menus.length) {
    return [parts[0], parts[1]].concat(menus, parts.slice(2)).join(' ');
  }

  return parts.join(' ');
}

/** Sets and Waits for the path to updated in the DOM. */
async function setAndWaitPath(path: string): Promise<void> {
  const element = getBreadcrumb();
  element.path = path;
  return waitUntil(() => element.getAttribute('path')! === path);
}

/**
 * Tests rendering an empty path.
 */
export async function testBreadcrumbEmptyPath(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('');

  // clang-format off
  const expect = element.path +
      ' 1: display:none id=first hidden' +
      ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
      ' 3: display:none id=second hidden' +
      ' 4: display:none id=third hidden' +
      ' 5: display:none id=fourth hidden';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  done();
}

/**
 * Tests rendering a one element path.
 */
export async function testBreadcrumbOnePartPath(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A');

  // clang-format off
  const expect = element.path +
    ' 1: display:block id=first text=[A]' +
    ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
    ' 3: display:none id=second hidden' +
    ' 4: display:none id=third hidden' +
    ' 5: display:none id=fourth hidden';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  done();
}

/** Tests rendering a two element path.  */
export async function testBreadcrumbTwoPartPath(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B');

  // clang-format off
  const expect = element.path +
    ' 1: display:block id=first text=[A]' +
    ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
    ' 3: display:none id=second hidden' +
    ' 4: display:none id=third hidden' +
    ' 5: display:block id=fourth text=[B]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  done();
}

/** Tests rendering a three element path.  */
export async function testBreadcrumbThreePartPath(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C');

  // clang-format off
  const expect = element.path +
    ' 1: display:block id=first text=[A]' +
    ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
    ' 3: display:none id=second hidden' +
    ' 4: display:block id=third text=[B]' +
    ' 5: display:block id=fourth text=[C]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  done();
}

/**
 * Tests rendering a four element path.
 */
export async function testBreadcrumbFourPartPath(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D');

  // clang-format off
  const expect = element.path +
    ' 1: display:block id=first text=[A]' +
    ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
    ' 3: display:block id=second text=[B]' +
    ' 4: display:block id=third text=[C]' +
    ' 5: display:block id=fourth text=[D]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  done();
}

/**
 * Tests rendering a path of more than four parts. The elider button should be
 * visible (not hidden and have display).
 *
 * The drop-down menu button should contain the elided path parts and can have
 * display, but are invisible because the elider drop-down menu is closed.
 */
export async function testBreadcrumbMoreThanFourElementPathsElide(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E/F');

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadcrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // clang-format off
  const expect = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=false,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' dropdown-item: display:block text=[D]' +
     ' 3: display:none id=second hidden' +
     ' 4: display:block id=third text=[E]' +
     ' 5: display:block id=fourth text=[F]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  done();
}

/**
 * Tests rendering a path where the path parts have escaped characters. Again,
 * the elider should be visible (not hidden and have display) because the path
 * has more than four parts.
 *
 * The drop-down menu button should contain the elided path parts and can have
 * display, but are invisible because the elider drop-down menu is closed.
 */
export async function testBreadcrumbRendersEscapedPathParts(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath(
      'A%2FA/B%2FB/C %2F/%2FD /%2F%2FE/Nexus%2FPixel %28MTP%29');

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadcrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // clang-format off
  const expect = element.path +
     ' 1: display:block id=first text=[A/A]' +
     ' 2: display:flex elider[aria-expanded=false,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B/B]' +
     ' dropdown-item: display:block text=[C /]' +
     ' dropdown-item: display:block text=[/D ]' +
     ' 3: display:none id=second hidden' +
     ' 4: display:block id=third text=[//E]' +
     ' 5: display:block id=fourth text=[Nexus/Pixel (MTP)]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  done();
}

/**
 * Tests rendering a path of more than four parts. The elider button should be
 * visible and clicking it should 'open' and 'close' its drop-down menu.
 */
export async function
testBreadcrumbElidedPathEliderButtonClicksOpenDropDownMenu(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E');

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadcrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Clicking the elider button should 'open' its drop-down menu.
  assertFalse(elider.hasAttribute('hidden'));
  elider.click();
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // clang-format off
  const opened = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=true,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' 3: display:none id=second hidden' +
     ' 4: display:block id=third text=[D]' +
     ' 5: display:block id=fourth text=[E]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(opened, path + ' ' + getBreadcrumbButtonState());

  // Clicking the elider again should 'close' the drop-down menu.
  assertFalse(elider.hasAttribute('hidden'));
  elider.click();
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // clang-format off
  const closed = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=false,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' 3: display:none id=second hidden' +
     ' 4: display:block id=third text=[D]' +
     ' 5: display:block id=fourth text=[E]';
  // clang-format on

  assertEquals(closed, path + ' ' + getBreadcrumbButtonState());

  done();
}

/**
 * Tests that clicking on the main buttons emits a signal that indicates which
 * part of the breadcrumb path was clicked.
 */
export async function testBreadcrumbMainButtonClicksEmitNumberSignal(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E/F');

  // clang-format off
  const expect = element.path +
     ' 1: display:block id=first text=[A]' +  // 1st main button
     ' 2: display:flex elider[aria-expanded=false,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' dropdown-item: display:block text=[D]' +
     ' 3: display:none id=second hidden' +
     ' 4: display:block id=third text=[E]' +  // 2nd main button
     ' 5: display:block id=fourth text=[F]';  // 3rd main button
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  let signal: number|null = null;
  element.addEventListener(
      BREADCRUMB_CLICKED, (event: BreadcrumbClickedEvent) => {
        const index = Number(event.detail.partIndex);
        assertEquals(typeof index, 'number');
        signal = index;
      });

  const buttons = getVisibleBreadcrumbMainButtons();
  assertEquals(3, buttons.length, 'three main buttons should be visible');

  assert(buttons[0]);
  assert(buttons[1]);
  assert(buttons[2]);

  signal = null;
  assertEquals('A', buttons[0].textContent);
  assertFalse(buttons[0].hasAttribute('disabled'));
  buttons[0].click();
  assertEquals(element.parts.indexOf('A'), signal);

  signal = null;
  assertEquals('E', buttons[1].textContent);
  assertFalse(buttons[1].hasAttribute('disabled'));
  buttons[1].click();
  assertEquals(element.parts.indexOf('E'), signal);

  signal = null;
  assertEquals('F', buttons[2].textContent);
  assertTrue(buttons[2].hasAttribute('disabled'));
  buttons[2].click();  // Ignored: the last main button is always disabled.
  assertEquals(null, signal);

  done();
}

/**
 * Tests that clicking on the menu buttons emits a signal that indicates which
 * part of the breadcrumb path was clicked.
 */
export async function testBreadcrumbMenuButtonClicksEmitNumberSignal(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E');

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadcrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Clicking the elider button should 'open' its drop-down menu.
  assertFalse(elider.hasAttribute('hidden'));
  elider.click();
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // clang-format off
  const opened = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=true,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' 3: display:none id=second hidden' +
     ' 4: display:block id=third text=[D]' +
     ' 5: display:block id=fourth text=[E]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(opened, path + ' ' + getBreadcrumbButtonState());

  let signal: number|null = null;
  element.addEventListener(
      BREADCRUMB_CLICKED, (event: BreadcrumbClickedEvent) => {
        const index = Number(event.detail.partIndex);
        assertEquals(typeof index, 'number');
        signal = index;
      });

  const buttons = getBreadcrumbMenuButtons();
  assertEquals(2, buttons.length, 'there should be two drop-down items');

  assert(buttons[0]);
  assert(buttons[1]);

  signal = null;
  assertEquals('B', buttons[0].textContent);
  assertFalse(buttons[0].hasAttribute('disabled'));
  buttons[0].click();
  assertEquals(element.parts.indexOf('B'), signal);

  signal = null;
  assertEquals('C', buttons[1].textContent);
  assertFalse(buttons[1].hasAttribute('disabled'));
  buttons[1].click();
  assertEquals(element.parts.indexOf('C'), signal);

  done();
}

/**
 * Tests that setting the path closes the the drop-down menu.
 */
export async function testBreadcrumbSetPathClosesEliderButtonDropDownMenu(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E');

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadcrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Clicking the elider button should 'open' its drop-down menu.
  assertFalse(elider.hasAttribute('hidden'));
  elider.click();
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // clang-format off
  const opened = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=true,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' 3: display:none id=second hidden' +
     ' 4: display:block id=third text=[D]' +
     ' 5: display:block id=fourth text=[E]';
  // clang-format on

  const first = element.parts.join('/');
  assertEquals(opened, first + ' ' + getBreadcrumbButtonState());

  // Changing the path should 'close' the drop-down menu.
  await setAndWaitPath('F/G/H');

  assertEquals('false', elider.getAttribute('aria-expanded'));

  // clang-format off
  const closed = element.path +
    ' 1: display:block id=first text=[F]' +
    ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
    ' 3: display:none id=second hidden' +
    ' 4: display:block id=third text=[G]' +
    ' 5: display:block id=fourth text=[H]';
  // clang-format on

  const second = element.parts.join('/');
  assertEquals(closed, second + ' ' + getBreadcrumbButtonState());

  done();
}

/**
 * Tests that setting the path updates the <xf-breadcrumb path> attribute.
 */
export async function testBreadcrumbSetPathChangesElementPath(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E/F');
  assertEquals(element.path, element.getAttribute('path'));

  // Change path.
  await setAndWaitPath('G/H/I');
  assertEquals(element.path, element.getAttribute('path'));

  done();
}

/**
 * Tests that opening and closing the elider button drop-down menu adds and
 * removes <xf-breadcrumb checked> attribute.
 */
export async function testBreadcrumbEliderButtonOpenCloseChangesElementChecked(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E/F');

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadcrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));
  assertFalse(element.hasAttribute('checked'));

  // Clicking the elider button should 'open' its drop-down menu.
  assertFalse(elider.hasAttribute('hidden'));
  elider.click();
  assertEquals('true', elider.getAttribute('aria-expanded'));
  assertTrue(element.hasAttribute('checked'));

  // Change path.
  await setAndWaitPath('G/H/I/J/K');

  // Changing the path should 'close' the drop-down menu.
  assertEquals('false', elider.getAttribute('aria-expanded'));
  assertFalse(element.hasAttribute('checked'));

  done();
}

/**
 * Tests that opening and closing the elider button drop-down menu adds and
 * removes global <html> element state.
 */
export async function testBreadcrumbEliderButtonOpenCloseChangesGlobalState(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E/F');

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadcrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Clicking the elider button should 'open' its drop-down menu.
  assertFalse(elider.hasAttribute('hidden'));
  elider.click();
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // And also change the global element state.
  const root = document.documentElement;
  assertTrue(root.classList.contains('breadcrumb-elider-expanded'));

  // Change path.
  await setAndWaitPath('G/H/I/J/K');

  // Changing the path should 'close' the drop-down menu.
  assertEquals('false', elider.getAttribute('aria-expanded'));
  assertFalse(element.hasAttribute('checked'));

  // And clear the global element state.
  assertFalse(root.classList.contains('breadcrumb-elider-expanded'));

  done();
}

/**
 * Tests that wide text path components are rendered elided with ellipsis ...
 * an opportunity for adding a tooltip.
 */
export async function testBreadcrumbPartPartsEllipsisElide(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/VERYVERYVERYVERYWIDEPATHPART');

  // clang-format off
  const expect = element.path +
      ' 1: display:block id=first text=[A]' +
      ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
      ' 3: display:none id=second hidden' +
      ' 4: display:none id=third hidden' +
      ' 5: display:block id=fourth text=[VERYVERYVERYVERYWIDEPATHPART]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  // The wide part should render its text with ellipsis.
  let ellipsis = element.getEllipsisButtons();
  const parts = element.parts;
  assert(ellipsis[0]);
  assert(parts[1]);

  assertEquals(1, ellipsis.length);
  assertEquals(element.parts[1], ellipsis[0].textContent);

  // Add has-tooltip attribute to this ellipsis button.
  ellipsis[0].setAttribute('has-tooltip', '');
  const tooltip = element.getToolTipButtons();
  assert(tooltip[0]);
  assertEquals(ellipsis[0], tooltip[0]);

  // getEllipsisButtons() should ignore [has-tooltip] buttons.
  ellipsis = element.getEllipsisButtons();
  assertEquals(0, ellipsis.length);

  done();
}

/**
 * Tests that wide text path components in the drop-down menu are rendered
 * elided with ellipsis ... an opportunity for adding a tooltip.
 */
export async function testBreadcrumbDropDownMenuPathPartsEllipsisElide(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/VERYVERYVERYVERYWIDEPATHPARTINDEED/C/D');

  // clang-format off
  const expect = element.path +
      ' 1: display:block id=first text=[A]' +
      ' 2: display:flex elider[aria-expanded=false,aria-haspopup,aria-label]' +
      ' dropdown-item: display:block text=[B]' +
      ' dropdown-item: display:block' +
      ' text=[VERYVERYVERYVERYWIDEPATHPARTINDEED]' +
      ' 3: display:none id=second hidden' +
      ' 4: display:block id=third text=[C]' +
      ' 5: display:block id=fourth text=[D]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  // Display the dropdown menu.
  const elider = getBreadcrumbEliderButton();
  elider.click();

  const parts = element.parts;
  assert(parts[2]);

  // The wide part button should render its text with ellipsis.
  let ellipsis = element.getEllipsisButtons();
  assertEquals(1, ellipsis.length);
  assert(ellipsis[0]);
  assertEquals(parts[2], ellipsis[0].textContent);

  // Add a has-tooltip attribute to the ellipsis button.
  ellipsis[0].setAttribute('has-tooltip', '');
  const tooltip = element.getToolTipButtons();
  assertEquals(1, tooltip.length);
  assert(tooltip[0]);
  assertEquals(ellipsis[0], tooltip[0]);

  // getEllipsisButtons() should ignore [has-tooltip] buttons.
  ellipsis = element.getEllipsisButtons();
  assertEquals(0, ellipsis.length);

  done();
}

/**
 * Tests that breadcrumb getToolTipButtons() service returns all buttons that
 * have a [has-tooltip] attribute.
 */
export async function testBreadcrumbButtonHasToolTipAttribute(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E');

  // Add a tool tip to the visible main buttons.
  getVisibleBreadcrumbMainButtons().forEach((button) => {
    button.setAttribute('has-tooltip', '');
  });

  // getToolTipButtons() should return those main buttons.
  let tooltips = element.getToolTipButtons();

  assertEquals(3, tooltips.length);
  assert(tooltips[0]);
  assert(tooltips[1]);
  assert(tooltips[2]);
  assertEquals('A', tooltips[0].textContent);
  assertEquals('D', tooltips[1].textContent);
  assertEquals('E', tooltips[2].textContent);

  // Changing the path should clear all tool tips.
  await setAndWaitPath('G/H/I/J/K');
  assertEquals(0, element.getToolTipButtons().length);

  // Add tool tips to the drop-down menu buttons.
  getBreadcrumbMenuButtons().forEach((button) => {
    button.setAttribute('has-tooltip', '');
  });

  // getToolTipButtons() should return those menu buttons.
  tooltips = element.getToolTipButtons();
  assertEquals(2, tooltips.length);
  assert(tooltips[0]);
  assert(tooltips[1]);
  assertEquals('H', tooltips[0].textContent);
  assertEquals('I', tooltips[1].textContent);

  // Note: tool tips can be enabled for the elider button.
  const elider = getBreadcrumbEliderButton();
  elider.setAttribute('has-tooltip', '');

  // But getToolTipButtons() must exclude the elider (i18n).
  tooltips = element.getToolTipButtons();
  assertEquals(2, tooltips.length);
  assert(tooltips[0]);
  assert(tooltips[1]);
  assertEquals('H', tooltips[0].textContent);
  assertEquals('I', tooltips[1].textContent);

  // And changing path should not clear its tool tip (i18n).
  await setAndWaitPath('since/the/elider/has/an/i18n/tooltip/aria-label');
  assertEquals(0, element.getToolTipButtons().length);
  assertTrue(elider.hasAttribute('has-tooltip'));

  // getEllipsisButtons() must exclude the elider button.
  await setAndWaitPath(elider.getAttribute('aria-label')!);
  const ellipsis = element.getEllipsisButtons();
  assertEquals(getVisibleBreadcrumbMainButtons()[0], ellipsis[0]);
  assertEquals(1, ellipsis.length);
  assert(ellipsis[0]);
  assertNotEquals(elider, ellipsis[0]);

  done();
}
