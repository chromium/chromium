// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {hasOverflowEllipsis} from '../common/js/dom_utils.js';
import {waitUntil} from '../common/js/test_error_reporting.js';

import {type BreadcrumbClickedEvent, XfBreadcrumb} from './xf_breadcrumb.js';

/**
 * Creates new <xf-breadcrumb> element for each test. Asserts it has no initial
 * path using the element.path getter.
 */
export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <xf-breadcrumb></xf-breadcrumb>
  `;
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
 * Returns the <xf-breadcrumb> child button elements.
 */
function getAllBreadcrumbButtons(): HTMLButtonElement[] {
  const buttons = getBreadcrumb().shadowRoot!.querySelectorAll('button');
  return Array.from(buttons) as HTMLButtonElement[];
}

/**
 * Returns the not-hidden <xf-breadcrumb> main button elements. The breadcrumb
 * main buttons have an id, all other breadcrumb buttons do not.
 */
function getVisibleBreadcrumbMainButtons(): HTMLButtonElement[] {
  const notHiddenMain = 'button[id]';
  const buttons = getBreadcrumb().shadowRoot!.querySelectorAll(notHiddenMain);
  return Array.from(buttons) as HTMLButtonElement[];
}

/** Returns the last not-hidden <xf-breadcrumb> main button element. */
function getLastVisibleBreadcrumbMainButton(): HTMLButtonElement {
  return getVisibleBreadcrumbMainButtons().pop() as HTMLButtonElement;
}

/** Returns the <xf-breadcrumb> elider button element. */
function getBreadcrumbEliderButton(): HTMLButtonElement|null {
  const elider = 'button[elider]';
  const button = getBreadcrumb().shadowRoot!.querySelectorAll(elider);
  if (button.length > 0) {
    return button[0] as HTMLButtonElement;
  }
  return null;
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

  const result = i + ': display:' + display + ' id=' + button.id + ' text=[' +
      button.textContent + ']';

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

  const result = `${button.classList.toString()}: display:` + display +
      ' text=[' + button.textContent + ']';

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
  if (getBreadcrumbEliderButton()) {
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
  await element.updateComplete;
}

function simulateMouseEnter(element: HTMLElement) {
  const ev = new MouseEvent('mouseenter', {
    view: window,
    bubbles: true,
    cancelable: true,
  });

  element.dispatchEvent(ev);
}

/** Returns the visible buttons rendered with CSS overflow ellipsis.  */
function getEllipsisButtons(breadcrumb: XfBreadcrumb): HTMLButtonElement[] {
  const pathButtons = Array.from(
      breadcrumb.shadowRoot!.querySelectorAll<HTMLButtonElement>('button[id]'),
  );
  if (breadcrumb.parts.length <= 4) {
    return pathButtons.filter(hasOverflowEllipsis);
  }

  const elidedButtons =
      Array.from(breadcrumb.shadowRoot!.querySelectorAll<HTMLButtonElement>(
          'cr-action-menu button'));
  const allButtons =
      [pathButtons[0]].concat(elidedButtons, pathButtons.slice(1)) as
      HTMLButtonElement[];
  return allButtons.filter(hasOverflowEllipsis);
}

/**
 * Tests rendering an empty path.
 */
export async function testBreadcrumbEmptyPath(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('');

  const path = element.path;
  assertEquals('', path);
  // An empty string is rendered if the path is empty.
  assertEquals('', getBreadcrumbButtonState());

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
    ' 1: display:block id=first text=[A]';
  // clang-format on

  const path = element.path;
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
    ' 2: display:block id=second text=[B]';
  // clang-format on

  const path = element.path;
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
    ' 2: display:block id=second text=[B]' +
    ' 3: display:block id=third text=[C]';
  // clang-format on

  const path = element.path;
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
    ' 2: display:block id=second text=[B]' +
    ' 3: display:block id=third text=[C]' +
    ' 4: display:block id=fourth text=[D]';
  // clang-format on

  const path = element.path;
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
  const elider = getBreadcrumbEliderButton()!;
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // clang-format off
  const expect = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=false,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' dropdown-item: display:block text=[D]' +
     ' 3: display:block id=second text=[E]' +
     ' 4: display:block id=third text=[F]';
  // clang-format on

  const path = element.path;
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
  const elider = getBreadcrumbEliderButton()!;
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // clang-format off
  const expect = element.path +
     ' 1: display:block id=first text=[A/A]' +
     ' 2: display:flex elider[aria-expanded=false,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B/B]' +
     ' dropdown-item: display:block text=[C /]' +
     ' dropdown-item: display:block text=[/D ]' +
     ' 3: display:block id=second text=[//E]' +
     ' 4: display:block id=third text=[Nexus/Pixel (MTP)]';
  // clang-format on

  const path = element.path;
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
  const elider = getBreadcrumbEliderButton()!;
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Clicking the elider button should 'open' its drop-down menu.
  elider.click();
  await element.updateComplete;
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // clang-format off
  const opened = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=true,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' 3: display:block id=second text=[D]' +
     ' 4: display:block id=third text=[E]';
  // clang-format on

  const path = element.path;
  assertEquals(opened, path + ' ' + getBreadcrumbButtonState());

  // Clicking the elider again should 'close' the drop-down menu.
  elider.click();
  await element.updateComplete;
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // clang-format off
  const closed = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=false,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' 3: display:block id=second text=[D]' +
     ' 4: display:block id=third text=[E]';
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
     ' 3: display:block id=second text=[E]' +  // 2nd main button
     ' 4: display:block id=third text=[F]';  // 3rd main button
  // clang-format on

  const path = element.path;
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  let signal: number|null = null;
  element.addEventListener(
      XfBreadcrumb.events.BREADCRUMB_CLICKED,
      (event: BreadcrumbClickedEvent) => {
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
  const elider = getBreadcrumbEliderButton()!;
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Clicking the elider button should 'open' its drop-down menu.
  elider.click();
  await element.updateComplete;
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // clang-format off
  const opened = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=true,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' 3: display:block id=second text=[D]' +
     ' 4: display:block id=third text=[E]';
  // clang-format on

  const path = element.path;
  assertEquals(opened, path + ' ' + getBreadcrumbButtonState());

  let signal: number|null = null;
  element.addEventListener(
      XfBreadcrumb.events.BREADCRUMB_CLICKED,
      (event: BreadcrumbClickedEvent) => {
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
  const elider = getBreadcrumbEliderButton()!;
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Clicking the elider button should 'open' its drop-down menu.
  elider.click();
  await element.updateComplete;
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // clang-format off
  const opened = element.path +
     ' 1: display:block id=first text=[A]' +
     ' 2: display:flex elider[aria-expanded=true,aria-haspopup,aria-label]' +
     ' dropdown-item: display:block text=[B]' +
     ' dropdown-item: display:block text=[C]' +
     ' 3: display:block id=second text=[D]' +
     ' 4: display:block id=third text=[E]';
  // clang-format on

  const first = element.path;
  assertEquals(opened, first + ' ' + getBreadcrumbButtonState());

  // Changing the path should remove the drop-down menu.
  await setAndWaitPath('F/G/H');

  assertEquals(null, getBreadcrumbEliderButton());

  // clang-format off
  const closed = element.path +
    ' 1: display:block id=first text=[F]' +
    ' 2: display:block id=second text=[G]' +
    ' 3: display:block id=third text=[H]';
  // clang-format on

  const second = element.path;
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
 * removes global <html> element state.
 */
export async function testBreadcrumbEliderButtonOpenCloseChangesGlobalState(
    done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('A/B/C/D/E/F');

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadcrumbEliderButton()!;
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Clicking the elider button should 'open' its drop-down menu.
  elider.click();
  await element.updateComplete;
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // And also change the global element state.
  const root = document.documentElement;
  assertTrue(root.classList.contains('breadcrumb-elider-expanded'));

  // Change path.
  await setAndWaitPath('G/H/I/J/K');

  // Changing the path should 'close' the drop-down menu.
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // And clear the global element state.
  assertFalse(root.classList.contains('breadcrumb-elider-expanded'));

  done();
}

/**
 * Tests that wide text path components are rendered elided with ellipsis ...
 * and hovering over the button sets the `title` attribute which is used by the
 * browser to render the native tooltip.
 */
export async function testBreadcrumbPartPartsEllipsisElide(done: () => void) {
  const element = getBreadcrumb();

  // Set path.
  await setAndWaitPath('VERYVERYVERYVERYWIDEPATHPART/A');

  // clang-format off
  const expect = element.path +
      ' 1: display:block id=first text=[VERYVERYVERYVERYWIDEPATHPART]' +
      ' 2: display:block id=second text=[A]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  // The wide part should render its text with ellipsis.
  const ellipsis = getEllipsisButtons(element);
  const parts = element.parts;
  assert(ellipsis[0]);
  assert(parts[0]);

  assertEquals(1, ellipsis.length);
  const button = ellipsis[0];
  assertEquals(element.parts[0], button.textContent);

  // Simulate the mouseenter that sets the title.
  simulateMouseEnter(button);
  await waitUntil(() => button.getAttribute('title')! === button.innerText);

  done();
}

/**
 * Tests that wide text path components in the drop-down menu are rendered
 * elided with ellipsis ... and hovering over the button sets the `title`
 * attribute which is used by the browser to render the native tooltip.
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
      ' 3: display:block id=second text=[C]' +
      ' 4: display:block id=third text=[D]';
  // clang-format on
  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadcrumbButtonState());

  // Display the dropdown menu.
  const elider = getBreadcrumbEliderButton()!;
  elider.click();
  await element.updateComplete;

  const parts = element.parts;
  assert(parts[2]);

  // The wide part button should render its text with ellipsis.
  const ellipsis = getEllipsisButtons(element);
  assertEquals(1, ellipsis.length);
  assert(ellipsis[0]);
  assertEquals(parts[2], ellipsis[0].textContent);

  // Simulate the mouseenter that sets the title.
  const button = ellipsis[0];
  simulateMouseEnter(button);
  await waitUntil(() => button.getAttribute('title')! === button.innerText);

  done();
}
