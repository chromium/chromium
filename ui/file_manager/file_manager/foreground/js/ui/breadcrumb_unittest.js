// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {BreadCrumb} from './breadcrumb.js';

/**
 * Creates new <bread-drumb> element for each test. Asserts it has no initial
 * path using the element.path getter.
 */
export function setUp() {
  document.body.innerHTML = '<bread-crumb></bread-crumb>';
  const path = assert(document.querySelector('bread-crumb')).path;
  assertEquals('', path);
}

/**
 * Returns the <bread-crumb> element.
 * @return {!BreadCrumb}
 */
function getBreadCrumb() {
  const element = assert(document.querySelector('bread-crumb'));
  assertNotEquals('none', window.getComputedStyle(element).display);
  assertFalse(element.hasAttribute('hidden'));
  return /** @type {!BreadCrumb} */ (element);
}

/**
 * Returns the <bread-crumb> child button elements. There are 4 main buttons
 * and one elider button (so at least 5) plus optional drop-down menu buttons.
 * @return {!Array<!HTMLButtonElement>}
 */
function getAllBreadCrumbButtons() {
  const buttons = getBreadCrumb().shadowRoot.querySelectorAll('button');
  assert(buttons) && assert(buttons.length >= 5, 'too few buttons');
  return Array.from(buttons);
}

/**
 * Returns the not-hidden <bread-crumb> main button elements. The breadcrumb
 * main buttons have an id, all other breadcrumb buttons do not.
 * @return {!Array<HTMLButtonElement>}
 */
function getVisibleBreadCrumbMainButtons() {
  const notHiddenMain = 'button[id]:not([hidden])';
  const buttons = getBreadCrumb().shadowRoot.querySelectorAll(notHiddenMain);
  return Array.from(buttons);
}

/**
 * Returns the last not-hidden <bread-crumb> main button element.
 * @return {HTMLButtonElement}
 */
function getLastVisibleBreadCrumbMainButton() {
  return getVisibleBreadCrumbMainButtons().pop();
}

/**
 * Returns the <bread-crumb> elider button element.
 * @return {!HTMLButtonElement}
 */
function getBreadCrumbEliderButton() {
  const elider = 'button[elider]';
  const button = getBreadCrumb().shadowRoot.querySelectorAll(elider);
  assert(button) && assert(button.length === 1, 'invalid elider button');
  return /** @type {!HTMLButtonElement} */ (button[0]);
}

/**
 * Returns the <bread-crumb> drop-down menu button elements.
 * @return {!Array<!HTMLButtonElement>}
 */
function getBreadCrumbMenuButtons() {
  const menuButton = 'cr-action-menu button';
  const buttons = getBreadCrumb().shadowRoot.querySelectorAll(menuButton);
  return Array.from(assert(buttons, 'no menu buttons'));
}

/**
 * Returns <bread-crumb> main button visual state.
 * @param {!HTMLButtonElement} button Main button (these have an id).
 * @param {number} i Number to assign to the button.
 * @return {string}
 */
function getMainButtonState(button, i) {
  const display = window.getComputedStyle(button).display;

  let result = i + ': display:' + display + ' id=' + button.id;
  if (!button.hasAttribute('hidden')) {
    result += ' text=[' + button.textContent + ']';
  } else {
    assertEquals('none', display);
    result += ' hidden';
  }

  assert(button.id, 'main buttons should have an id');
  return result;
}

/**
 * Returns <bread-crumb> elider button visual state.
 * @param {!HTMLButtonElement} button Elider button.
 * @param {number} i Number to assign to the button.
 * @return {string}
 */
function getEliderButtonState(button, i) {
  const display = window.getComputedStyle(button).display;

  const result = i + ': display:' + display;
  const attributes = [];
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
  assert(button.hasAttribute('elider'));
  return result + ' elider[' + attributes.sort() + ']';
}

/**
 * Returns <bread-crumb> drop-down menu button visual state.
 * @param {!HTMLButtonElement} button Drop-down menu button.
 * @return {string}
 */
function getDropDownMenuButtonState(button) {
  const display = window.getComputedStyle(button).display;

  let result = `${button.classList.toString()}: display:` + display;
  if (!button.hasAttribute('hidden')) {
    result += ' text=[' + button.textContent + ']';
  } else {
    assertEquals('none', display);
    result += ' hidden';
  }

  assertFalse(!!button.id, 'drop-down buttons should not have an id');
  assert(button.classList.contains('dropdown-item'));
  return result;
}

/**
 * Returns the <bread-crumb> buttons visual state.
 * @return {string}
 */
function getBreadCrumbButtonState() {
  const parts = [];
  const menus = [];

  const buttons = getAllBreadCrumbButtons();
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
  if (!getBreadCrumbEliderButton().hasAttribute('hidden')) {
    assertTrue(getBreadCrumb().parts.length > 4);
  }

  // The 'last' main button displayed should always be [disabled].
  const last = getLastVisibleBreadCrumbMainButton();
  if (getBreadCrumb().path !== '') {
    assert(last.hasAttribute('disabled'));
  }

  if (menus.length) {
    return [parts[0], parts[1]].concat(menus, parts.slice(2)).join(' ');
  }

  return parts.join(' ');
}

/**
 * Tests rendering an empty path.
 */
export function testBreadcrumbEmptyPath() {
  const element = getBreadCrumb();

  // Set path.
  element.path = '';

  // clang-format off
  const expect = element.path +
      ' 1: display:none id=first hidden' +
      ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
      ' 3: display:none id=second hidden' +
      ' 4: display:none id=third hidden' +
      ' 5: display:none id=fourth hidden';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());
}

/**
 * Tests rendering a one element path.
 */
export function testBreadcrumbOnePartPath() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A';

  // clang-format off
  const expect = element.path +
    ' 1: display:block id=first text=[A]' +
    ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
    ' 3: display:none id=second hidden' +
    ' 4: display:none id=third hidden' +
    ' 5: display:none id=fourth hidden';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());
}

/**
 * Tests rendering a two element path.
 */
export function testBreadcrumbTwoPartPath() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B';

  // clang-format off
  const expect = element.path +
    ' 1: display:block id=first text=[A]' +
    ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
    ' 3: display:none id=second hidden' +
    ' 4: display:none id=third hidden' +
    ' 5: display:block id=fourth text=[B]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());
}

/**
 * Tests rendering a three element path.
 */
export function testBreadcrumbThreePartPath() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C';

  // clang-format off
  const expect = element.path +
    ' 1: display:block id=first text=[A]' +
    ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
    ' 3: display:none id=second hidden' +
    ' 4: display:block id=third text=[B]' +
    ' 5: display:block id=fourth text=[C]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());
}

/**
 * Tests rendering a four element path.
 */
export function testBreadcrumbFourPartPath() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D';

  // clang-format off
  const expect = element.path +
    ' 1: display:block id=first text=[A]' +
    ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
    ' 3: display:block id=second text=[B]' +
    ' 4: display:block id=third text=[C]' +
    ' 5: display:block id=fourth text=[D]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());
}

/**
 * Tests rendering a path of more than four parts. The elider button should be
 * visible (not hidden and have display).
 *
 * The drop-down menu button should contain the elided path parts and can have
 * display, but are invisible because the elider drop-down menu is closed.
 */
export function testBreadcrumbMoreThanFourElementPathsElide() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E/F';

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadCrumbEliderButton();
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
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());
}

/**
 * Tests rendering a path where the path parts have escaped characters. Again,
 * the elider should be visible (not hidden and have display) because the path
 * has more than four parts.
 *
 * The drop-down menu button should contain the elided path parts and can have
 * display, but are invisible because the elider drop-down menu is closed.
 */
export function testBreadcrumbRendersEscapedPathParts() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A%2FA/B%2FB/C %2F/%2FD /%2F%2FE/Nexus%2FPixel %28MTP%29';

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadCrumbEliderButton();
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
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());
}

/**
 * Tests rendering a path of more than four parts. The elider button should be
 * visible and clicking it should 'open' and 'close' its drop-down menu.
 */
export function testBreadcrumbElidedPathEliderButtonClicksOpenDropDownMenu() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E';

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadCrumbEliderButton();
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
  assertEquals(opened, path + ' ' + getBreadCrumbButtonState());

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

  assertEquals(closed, path + ' ' + getBreadCrumbButtonState());
}

/**
 * Tests that clicking on the main buttons emits a signal that indicates which
 * part of the breadcrumb path was clicked.
 */
export async function testBreadcrumbMainButtonClicksEmitNumberSignal(done) {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E/F';

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
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());

  // Set the BreadCrumb signals callback.
  let signal = null;
  element.setSignalCallback((number) => {
    assert(typeof number === 'number');
    signal = number;
  });

  const buttons = getVisibleBreadCrumbMainButtons();
  assertEquals(3, buttons.length, 'three main buttons should be visible');

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
  assert(buttons[2].hasAttribute('disabled'));
  buttons[2].click();  // Ignored: the last main button is always disabled.
  assertEquals(null, signal);

  done();
}

/**
 * Tests that clicking on the menu buttons emits a signal that indicates which
 * part of the breadcrumb path was clicked.
 */
export async function testBreadcrumbMenuButtonClicksEmitNumberSignal(done) {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E';

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadCrumbEliderButton();
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
  assertEquals(opened, path + ' ' + getBreadCrumbButtonState());

  // Set the BreadCrumb signals callback.
  let signal = null;
  element.setSignalCallback((number) => {
    assert(typeof number === 'number');
    signal = number;
  });

  const buttons = getBreadCrumbMenuButtons();
  assertEquals(2, buttons.length, 'there should be two drop-down items');

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
 * Tests that setting the path emits a signal when the rendering of the new
 * path begins, and when it ends.
 */
export async function testBreadcrumbSetPathEmitsRenderSignals(done) {
  const element = getBreadCrumb();

  // Set the BreadCrumb signals callback.
  const signal = [];
  element.setSignalCallback((rendered) => {
    assert(typeof rendered === 'string');
    signal.push(rendered);
  });

  // Set path.
  element.path = 'A/B/C/D/E';

  // Begin: element.path has changed, the buttons have not changed.
  assertEquals(signal[0], 'path-updated');
  // End: the element path has been rendered into the main buttons.
  assertEquals(signal[1], 'path-rendered');
  assertEquals(2, signal.length);

  done();
}

/**
 * Tests that opening the elider button drop-down menu emits a render signal
 * to indicate that the elided menu items were rendered.
 */
export async function testBreadcrumbEliderButtonOpenEmitsRenderSignal(done) {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E/F';

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadCrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Set the BreadCrumb signals callback.
  const signal = [];
  element.setSignalCallback((rendered) => {
    assert(typeof rendered === 'string');
    signal.push(rendered);
  });

  // Clicking the elider button should 'open' its drop-down menu.
  assertFalse(elider.hasAttribute('hidden'));
  elider.click();
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // Signal the elided parts were rendered to the drop-down buttons.
  assertEquals(signal[0], 'path-rendered');
  assertEquals(1, signal.length);

  done();
}

/**
 * Tests that setting the path closes the the drop-down menu.
 */
export function testBreadcrumbSetPathClosesEliderButtonDropDownMenu() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E';

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadCrumbEliderButton();
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
  assertEquals(opened, first + ' ' + getBreadCrumbButtonState());

  // Changing the path should 'close' the drop-down menu.
  element.path = 'F/G/H';
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
  assertEquals(closed, second + ' ' + getBreadCrumbButtonState());
}

/**
 * Tests that setting the path updates the <bread-crumb path> attribute.
 */
export function testBreadcrumbSetPathChangesElementPath() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E/F';
  assertEquals(element.path, element.getAttribute('path'));

  // Change path.
  element.path = 'G/H/I';
  assertEquals(element.path, element.getAttribute('path'));
}

/**
 * Tests that opening and closing the elider button drop-down menu adds and
 * removes <bread-crumb checked> attribute.
 */
export function testBreadcrumbEliderButtonOpenCloseChangesElementChecked() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E/F';

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadCrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));
  assertFalse(element.hasAttribute('checked'));

  // Clicking the elider button should 'open' its drop-down menu.
  assertFalse(elider.hasAttribute('hidden'));
  elider.click();
  assertEquals('true', elider.getAttribute('aria-expanded'));
  assert(element.hasAttribute('checked'));

  // Change path.
  element.path = 'G/H/I/J/K';

  // Changing the path should 'close' the drop-down menu.
  assertEquals('false', elider.getAttribute('aria-expanded'));
  assertFalse(element.hasAttribute('checked'));
}

/**
 * Tests that opening and closing the elider button drop-down menu adds and
 * removes global <html> element state.
 */
export function testBreadcrumbEliderButtonOpenCloseChangesGlobalState() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E/F';

  // Elider button drop-down menu should be in the 'closed' state.
  const elider = getBreadCrumbEliderButton();
  assertEquals('false', elider.getAttribute('aria-expanded'));

  // Clicking the elider button should 'open' its drop-down menu.
  assertFalse(elider.hasAttribute('hidden'));
  elider.click();
  assertEquals('true', elider.getAttribute('aria-expanded'));

  // And also change the global element state.
  const root = document.documentElement;
  assertTrue(root.classList.contains('breadcrumb-elider-expanded'));

  // Change path.
  element.path = 'G/H/I/J/K';

  // Changing the path should 'close' the drop-down menu.
  assertEquals('false', elider.getAttribute('aria-expanded'));
  assertFalse(element.hasAttribute('checked'));

  // And clear the global element state.
  assertFalse(root.classList.contains('breadcrumb-elider-expanded'));
}

/**
 * Tests that wide text path components are rendered elided with ellipsis ...
 * an opportunity for adding a tooltip.
 */
export function testBreadcrumbPartPartsEllipsisElide() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/VERYVERYVERYVERYWIDEPATHPART';

  // clang-format off
  const expect = element.path +
      ' 1: display:block id=first text=[A]' +
      ' 2: display:none elider[aria-expanded=false,aria-haspopup,aria-label,hidden]' +
      ' 3: display:none id=second hidden' +
      ' 4: display:none id=third hidden' +
      ' 5: display:block id=fourth text=[VERYVERYVERYVERYWIDEPATHPART]';
  // clang-format on

  const path = element.parts.join('/');
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());

  // The wide part should render its text with ellipsis.
  let ellipsis = element.getEllipsisButtons();
  assertEquals(1, ellipsis.length);
  assertEquals(element.parts[1], ellipsis[0].textContent);

  // Add has-tooltip attribute to this ellipsis button.
  ellipsis[0].setAttribute('has-tooltip', '');
  const tooltip = element.getToolTipButtons();
  assertEquals(ellipsis[0], tooltip[0]);

  // getEllipsisButtons() should ignore [has-tooltip] buttons.
  ellipsis = element.getEllipsisButtons();
  assertEquals(0, ellipsis.length);
}

/**
 * Tests that wide text path components in the drop-down menu are rendered
 * elided with ellipsis ... an opportunity for adding a tooltip.
 * @suppress {accessControls} to be able to access private properties.
 */
export function testBreadcrumbDropDownMenuPathPartsEllipsisElide() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/VERYVERYVERYVERYWIDEPATHPARTINDEED/C/D';

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
  assertEquals(expect, path + ' ' + getBreadCrumbButtonState());

  // Display the dropdown menu.
  element.toggleMenu_();

  // The wide part button should render its text with ellipsis.
  let ellipsis = element.getEllipsisButtons();
  assertEquals(1, ellipsis.length);
  assertEquals(element.parts[2], ellipsis[0].textContent);

  // Add a has-tooltip attribute to the ellipsis button.
  ellipsis[0].setAttribute('has-tooltip', '');
  const tooltip = element.getToolTipButtons();
  assertEquals(ellipsis[0], tooltip[0]);

  // getEllipsisButtons() should ignore [has-tooltip] buttons.
  ellipsis = element.getEllipsisButtons();
  assertEquals(0, ellipsis.length);
}

/**
 * Tests that breadcrumb getToolTipButtons() service returns all buttons that
 * have a [has-tooltip] attribute.
 */
export function testBreadcrumbButtonHasToolTipAttribute() {
  const element = getBreadCrumb();

  // Set path.
  element.path = 'A/B/C/D/E';

  // Add a tool tip to the visible main buttons.
  getVisibleBreadCrumbMainButtons().forEach((button) => {
    button.setAttribute('has-tooltip', '');
  });

  // getToolTipButtons() should return those main buttons.
  let tooltips = element.getToolTipButtons();
  assertEquals('A', tooltips[0].textContent);
  assertEquals('D', tooltips[1].textContent);
  assertEquals('E', tooltips[2].textContent);
  assertEquals(3, tooltips.length);

  // Changing the path should clear all tool tips.
  element.path = 'G/H/I/J/K';
  assertEquals(0, element.getToolTipButtons().length);

  // Add tool tips to the drop-down menu buttons.
  getBreadCrumbMenuButtons().forEach((button) => {
    button.setAttribute('has-tooltip', '');
  });

  // getToolTipButtons() should return those menu buttons.
  tooltips = element.getToolTipButtons();
  assertEquals('H', tooltips[0].textContent);
  assertEquals('I', tooltips[1].textContent);
  assertEquals(2, tooltips.length);

  // Note: tool tips can be enabled for the elider button.
  const elider = getBreadCrumbEliderButton();
  elider.setAttribute('has-tooltip', '');

  // But getToolTipButtons() must exclude the elider (i18n).
  tooltips = element.getToolTipButtons();
  assertEquals('H', tooltips[0].textContent);
  assertEquals('I', tooltips[1].textContent);
  assertEquals(2, tooltips.length);

  // And changing path should not clear its tool tip (i18n).
  element.path = 'since/the/elider/has/an/i18n/tooltip/aria-label';
  assertEquals(0, element.getToolTipButtons().length);
  assertTrue(elider.hasAttribute('has-tooltip'));

  // getEllipsisButtons() must exclude the elider button.
  element.path = elider.getAttribute('aria-label');
  const ellipsis = element.getEllipsisButtons();
  assertEquals(getVisibleBreadCrumbMainButtons()[0], ellipsis[0]);
  assertNotEquals(elider, ellipsis[0]);
  assertEquals(1, ellipsis.length);
}
