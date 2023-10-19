// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {reportPromise} from '../../common/js/test_error_reporting.js';

import {FilesTooltip} from './files_tooltip.js';

/** @type {Element} */
let chocolateButton;

/** @type {Element} */
let cherriesButton;

/** @type {Element} */
let cheeseButton;

/** @type {Element} */
let otherButton;

/** @type {FilesTooltip} */
let tooltip;

const windowEdgePadding = 6;

export function setUp() {
  document.body.innerHTML = getTrustedHTML`
  <style type="text/css">
   button {
     display: flex;
     height: 32px;
     margin: 30px;
     width: 32px;
   }

   #container {
     display: flex;
     justify-content: space-between;
   }

   files-tooltip {
     background: yellow;
     box-sizing: border-box;
     position: absolute;
     text-align: center;
     width: 100px;
   }
  </style>

  <!-- Targets for tooltip testing. -->
  <div id="container">
    <button id="chocolate" aria-label="Chocolate!"></button>
    <button id="cherries" aria-label="Cherries!"></button>
  </div>

  <button id="cheese" aria-label="Cheese!" show-card-tooltip></button>

  <!-- Button without a tooltip. -->
  <button id="other"></button>

  <!-- Polymer files tooltip element. -->
  <files-tooltip></files-tooltip>
`;
  // @ts-ignore: error TS2322: Type 'Element | null' is not assignable to type
  // 'Element'.
  chocolateButton = document.querySelector('#chocolate');
  // @ts-ignore: error TS2322: Type 'Element | null' is not assignable to type
  // 'Element'.
  cherriesButton = document.querySelector('#cherries');
  // @ts-ignore: error TS2322: Type 'Element | null' is not assignable to type
  // 'Element'.
  cheeseButton = document.querySelector('#cheese');
  // @ts-ignore: error TS2322: Type 'Element | null' is not assignable to type
  // 'Element'.
  otherButton = document.querySelector('#other');

  // @ts-ignore: error TS2322: Type 'FilesTooltip | null' is not assignable to
  // type 'FilesTooltip'.
  tooltip = document.querySelector('files-tooltip');
  assertNotEquals('none', window.getComputedStyle(tooltip).display);
  assertEquals('0', window.getComputedStyle(tooltip).opacity);

  // @ts-ignore: error TS2345: Argument of type 'Element[]' is not assignable to
  // parameter of type 'NodeList'.
  tooltip.addTargets([chocolateButton, cherriesButton, cheeseButton]);
}

// @ts-ignore: error TS7006: Parameter 'target' implicitly has an 'any' type.
function waitForMutation(target) {
  // @ts-ignore: error TS6133: 'reject' is declared but its value is never read.
  return new Promise((fulfill, reject) => {
    // @ts-ignore: error TS6133: 'mutations' is declared but its value is never
    // read.
    const observer = new MutationObserver(mutations => {
      observer.disconnect();
      // @ts-ignore: error TS2810: Expected 1 argument, but got 0. 'new
      // Promise()' needs a JSDoc hint to produce a 'resolve' that can be called
      // without arguments.
      fulfill();
    });
    observer.observe(target, {attributes: true});
  });
}

/** @param {()=>void} callback */
export function testFocus(callback) {
  // @ts-ignore: error TS2339: Property 'focus' does not exist on type
  // 'Element'.
  chocolateButton.focus();

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Chocolate!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals('6px', tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            // @ts-ignore: error TS2339: Property 'focus' does not exist on type
            // 'Element'.
            cherriesButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Cherries!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));

            const expectedLeft = document.body.offsetWidth -
                tooltip.offsetWidth - windowEdgePadding + 'px';
            assertEquals(expectedLeft, tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            // @ts-ignore: error TS2339: Property 'focus' does not exist on type
            // 'Element'.
            otherButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(!!tooltip.getAttribute('visible'));
          }),
      callback);
}

/** @param {()=>void} callback */
export function testFocusWithLink(callback) {
  // @ts-ignore: error TS2339: Property 'dataset' does not exist on type
  // 'Element'.
  cherriesButton.dataset.tooltipLinkHref = 'https://cherries.com';
  // @ts-ignore: error TS2339: Property 'dataset' does not exist on type
  // 'Element'.
  cherriesButton.dataset.tooltipLinkAriaLabel =
      'Click here to get more cherries';
  // @ts-ignore: error TS2339: Property 'dataset' does not exist on type
  // 'Element'.
  cherriesButton.dataset.tooltipLinkText = 'More cherries';

  // @ts-ignore: error TS2339: Property 'focus' does not exist on type
  // 'Element'.
  chocolateButton.focus();

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Chocolate!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals('6px', tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            // @ts-ignore: error TS2339: Property 'focus' does not exist on type
            // 'Element'.
            cherriesButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            // Check the label.
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Cherries!', label.textContent.trim());
            // Check the link: it should be visible now.
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const link = tooltip.shadowRoot.querySelector('#link');
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertEquals(link.getAttribute('aria-hidden'), 'false');
            // @ts-ignore: error TS18047: 'link.textContent' is possibly 'null'.
            assertEquals('More cherries', link.textContent.trim());
            assertEquals(
                'Click here to get more cherries',
                // @ts-ignore: error TS18047: 'link' is possibly 'null'.
                link.getAttribute('aria-label'));
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertEquals('https://cherries.com', link.getAttribute('href'));

            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');
            assertTrue(tooltip.hasAttribute('visible'));

            const expectedLeft = document.body.offsetWidth -
                tooltip.offsetWidth - windowEdgePadding + 'px';
            assertEquals(expectedLeft, tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            // @ts-ignore: error TS2339: Property 'focus' does not exist on type
            // 'Element'.
            chocolateButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            // Check the label.
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Chocolate!', label.textContent.trim());
            // Check the link: it should be hidden and cleared out.
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const link = tooltip.shadowRoot.querySelector('#link');
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertEquals(link.getAttribute('aria-hidden'), 'true');
            // @ts-ignore: error TS18047: 'link.textContent' is possibly 'null'.
            assertEquals('', link.textContent.trim());
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertFalse(link.hasAttribute('aria-label'));
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertEquals('#', link.getAttribute('href'));
          }),
      callback);
}

/** @param {()=>void} callback */
export function testFocusWithLabelChange(callback) {
  // @ts-ignore: error TS2339: Property 'focus' does not exist on type
  // 'Element'.
  chocolateButton.focus();

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Chocolate!', label.textContent.trim());
            // Change the button's aria-label attribute and the tooltip should
            // also update.
            chocolateButton.setAttribute('aria-label', 'New chocolate!');

            // @ts-ignore: error TS2345: Argument of type 'Element' is not
            // assignable to parameter of type 'HTMLElement'.
            tooltip.updateTooltipText(chocolateButton);
            return waitForMutation(tooltip);
          })
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('New chocolate!', label.textContent.trim());
          }),
      callback);
}

/** @param {()=>void} callback */
export function testHover(callback) {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Chocolate!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertEquals('6px', tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            chocolateButton.dispatchEvent(new MouseEvent('mouseout'));
            cherriesButton.dispatchEvent(new MouseEvent('mouseover'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Cherries!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));

            const expectedLeft = document.body.offsetWidth -
                tooltip.offsetWidth - windowEdgePadding + 'px';
            assertEquals(expectedLeft, tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            cherriesButton.dispatchEvent(new MouseEvent('mouseout'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(!!tooltip.getAttribute('visible'));
          }),
      callback);
}

/** @param {()=>void} callback */
export function testClickHides(callback) {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover', {bubbles: true}));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Chocolate!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            // Hiding here is synchronous. Dispatch the event asynchronously,
            // so the mutation observer is started before hiding.
            setTimeout(() => {
              document.body.dispatchEvent(new MouseEvent('mousedown'));
            });
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'true');
          }),
      callback);
}

/** @param {()=>void} callback */
export function testCardTooltipHover(callback) {
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Cheese!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertEquals('card-tooltip', tooltip.className);
            // @ts-ignore: error TS18047: 'label' is possibly 'null'.
            assertEquals('card-label', label.className);

            assertEquals('38px', tooltip.style.left);
            assertEquals('162px', tooltip.style.top);

            cheeseButton.dispatchEvent(new MouseEvent('mouseout'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(!!tooltip.getAttribute('visible'));
          }),
      callback);
}

/** @param {()=>void} callback */
export function testCardTooltipRTL(callback) {
  document.documentElement.setAttribute('dir', 'rtl');
  document.body.setAttribute('dir', 'rtl');

  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Cheese!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertEquals('card-tooltip', tooltip.className);
            // @ts-ignore: error TS18047: 'label' is possibly 'null'.
            assertEquals('card-label', label.className);

            // A border with 1px insets (top=bottom=left=right=1px) will be
            // applied to the window when drak/light feature is enabled. See
            // more details at crrev.com/c/3656414.
            assertTrue(
                `962px` == tooltip.style.left || `960px` == tooltip.style.left);
            assertEquals('162px', tooltip.style.top);

            cheeseButton.dispatchEvent(new MouseEvent('mouseout'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            // revert back document direction to not impact other tests.
            document.documentElement.setAttribute('dir', 'ltr');
            document.body.setAttribute('dir', 'ltr');
          }),
      callback);
}

/** @param {()=>void} callback */
export function testCardTooltipWithLinkHover(callback) {
  // @ts-ignore: error TS2339: Property 'dataset' does not exist on type
  // 'Element'.
  cheeseButton.dataset.tooltipLinkHref = 'https://cheese.com';
  // @ts-ignore: error TS2339: Property 'dataset' does not exist on type
  // 'Element'.
  cheeseButton.dataset.tooltipLinkAriaLabel = 'Click here to get more cheese';
  // @ts-ignore: error TS2339: Property 'dataset' does not exist on type
  // 'Element'.
  cheeseButton.dataset.tooltipLinkText = 'More cheese';
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // Check the label.
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Cheese!', label.textContent.trim());
            // Check the link: it should be visible now.
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const link = tooltip.shadowRoot.querySelector('#link');
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertEquals(link.getAttribute('aria-hidden'), 'false');
            // @ts-ignore: error TS18047: 'link.textContent' is possibly 'null'.
            assertEquals('More cheese', link.textContent.trim());
            assertEquals(
                'Click here to get more cheese',
                // @ts-ignore: error TS18047: 'link' is possibly 'null'.
                link.getAttribute('aria-label'));
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertEquals('https://cheese.com', link.getAttribute('href'));

            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertTrue(tooltip.classList.contains('card-tooltip'));
            // @ts-ignore: error TS18047: 'label' is possibly 'null'.
            assertEquals('card-label', label.className);

            assertEquals('38px', tooltip.style.left);
            assertEquals('162px', tooltip.style.top);

            assertTrue(tooltip.classList.contains('link-tooltip'));

            cheeseButton.dispatchEvent(new MouseEvent('mouseout'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(!!tooltip.getAttribute('visible'));
          }),
      callback);
}

/** @param {()=>void} callback */
export function testTooltipWithIncompleteLinkHover(callback) {
  // @ts-ignore: error TS2339: Property 'dataset' does not exist on type
  // 'Element'.
  cheeseButton.dataset.tooltipLinkHref = 'https://cheese.com';
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // Check the label.
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const label = tooltip.shadowRoot.querySelector('#label');
            // @ts-ignore: error TS18047: 'label.textContent' is possibly
            // 'null'.
            assertEquals('Cheese!', label.textContent.trim());
            // Check the link: it should be hidden since not all required
            // attributes are set.
            // @ts-ignore: error TS18047: 'tooltip.shadowRoot' is possibly
            // 'null'.
            const link = tooltip.shadowRoot.querySelector('#link');
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertEquals(link.getAttribute('aria-hidden'), 'true');
            // @ts-ignore: error TS18047: 'link.textContent' is possibly 'null'.
            assertEquals('', link.textContent.trim());
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertFalse(link.hasAttribute('aria-label'));
            // @ts-ignore: error TS18047: 'link' is possibly 'null'.
            assertEquals('#', link.getAttribute('href'));

            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertTrue(tooltip.classList.contains('card-tooltip'));
            // @ts-ignore: error TS18047: 'label' is possibly 'null'.
            assertEquals('card-label', label.className);

            assertEquals('38px', tooltip.style.left);
            assertEquals('162px', tooltip.style.top);

            assertFalse(tooltip.classList.contains('link-tooltip'));

            cheeseButton.dispatchEvent(new MouseEvent('mouseout'));
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(!!tooltip.getAttribute('visible'));
          }),
      callback);
}
