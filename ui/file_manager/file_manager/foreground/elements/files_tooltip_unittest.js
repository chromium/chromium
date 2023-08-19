// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './files_tooltip.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {reportPromise} from '../../common/js/test_error_reporting.js';

/** @type {Element} */
let chocolateButton;

/** @type {Element} */
let cherriesButton;

/** @type {Element} */
let cheeseButton;

/** @type {Element} */
let otherButton;

/** @type {FilesTooltip|Element} */
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
  chocolateButton = document.querySelector('#chocolate');
  cherriesButton = document.querySelector('#cherries');
  cheeseButton = document.querySelector('#cheese');
  otherButton = document.querySelector('#other');

  tooltip = document.querySelector('files-tooltip');
  assertNotEquals('none', window.getComputedStyle(tooltip).display);
  assertEquals('0', window.getComputedStyle(tooltip).opacity);

  tooltip.addTargets([chocolateButton, cherriesButton, cheeseButton]);
}

function waitForMutation(target) {
  return new Promise((fulfill, reject) => {
    const observer = new MutationObserver(mutations => {
      observer.disconnect();
      fulfill();
    });
    observer.observe(target, {attributes: true});
  });
}

export function testFocus(callback) {
  chocolateButton.focus();

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Chocolate!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals('6px', tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            cherriesButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cherries!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));

            const expectedLeft = document.body.offsetWidth -
                tooltip.offsetWidth - windowEdgePadding + 'px';
            assertEquals(expectedLeft, tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            otherButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            assertFalse(!!tooltip.getAttribute('visible'));
          }),
      callback);
}

export function testFocusWithLink(callback) {
  cherriesButton.dataset.tooltipLinkHref = 'https://cherries.com';
  cherriesButton.dataset.tooltipLinkAriaLabel =
      'Click here to get more cherries';
  cherriesButton.dataset.tooltipLinkText = 'More cherries';

  chocolateButton.focus();

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Chocolate!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals('6px', tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            cherriesButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            // Check the label.
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cherries!', label.textContent.trim());
            // Check the link: it should be visible now.
            const link = tooltip.shadowRoot.querySelector('#link');
            assertEquals(link.getAttribute('aria-hidden'), 'false');
            assertEquals('More cherries', link.textContent.trim());
            assertEquals(
                'Click here to get more cherries',
                link.getAttribute('aria-label'));
            assertEquals('https://cherries.com', link.getAttribute('href'));

            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');
            assertTrue(tooltip.hasAttribute('visible'));

            const expectedLeft = document.body.offsetWidth -
                tooltip.offsetWidth - windowEdgePadding + 'px';
            assertEquals(expectedLeft, tooltip.style.left);
            assertEquals('78px', tooltip.style.top);

            chocolateButton.focus();
            return waitForMutation(tooltip);
          })
          .then(() => {
            // Check the label.
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Chocolate!', label.textContent.trim());
            // Check the link: it should be hidden and cleared out.
            const link = tooltip.shadowRoot.querySelector('#link');
            assertEquals(link.getAttribute('aria-hidden'), 'true');
            assertEquals('', link.textContent.trim());
            assertFalse(link.hasAttribute('aria-label'));
            assertEquals('#', link.getAttribute('href'));
          }),
      callback);
}

export function testFocusWithLabelChange(callback) {
  chocolateButton.focus();

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Chocolate!', label.textContent.trim());
            // Change the button's aria-label attribute and the tooltip should
            // also update.
            chocolateButton.setAttribute('aria-label', 'New chocolate!');

            tooltip.updateTooltipText(chocolateButton);
            return waitForMutation(tooltip);
          })
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('New chocolate!', label.textContent.trim());
          }),
      callback);
}

export function testHover(callback) {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
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
            const label = tooltip.shadowRoot.querySelector('#label');
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

export function testClickHides(callback) {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover', {bubbles: true}));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
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

export function testCardTooltipHover(callback) {
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cheese!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertEquals('card-tooltip', tooltip.className);
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

export function testCardTooltipRTL(callback) {
  document.documentElement.setAttribute('dir', 'rtl');
  document.body.setAttribute('dir', 'rtl');

  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cheese!', label.textContent.trim());
            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertEquals('card-tooltip', tooltip.className);
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

export function testCardTooltipWithLinkHover(callback) {
  cheeseButton.dataset.tooltipLinkHref = 'https://cheese.com';
  cheeseButton.dataset.tooltipLinkAriaLabel = 'Click here to get more cheese';
  cheeseButton.dataset.tooltipLinkText = 'More cheese';
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // Check the label.
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cheese!', label.textContent.trim());
            // Check the link: it should be visible now.
            const link = tooltip.shadowRoot.querySelector('#link');
            assertEquals(link.getAttribute('aria-hidden'), 'false');
            assertEquals('More cheese', link.textContent.trim());
            assertEquals(
                'Click here to get more cheese',
                link.getAttribute('aria-label'));
            assertEquals('https://cheese.com', link.getAttribute('href'));

            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertTrue(tooltip.classList.contains('card-tooltip'));
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

export function testTooltipWithIncompleteLinkHover(callback) {
  cheeseButton.dataset.tooltipLinkHref = 'https://cheese.com';
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  return reportPromise(
      waitForMutation(tooltip)
          .then(() => {
            // Check the label.
            const label = tooltip.shadowRoot.querySelector('#label');
            assertEquals('Cheese!', label.textContent.trim());
            // Check the link: it should be hidden since not all required
            // attributes are set.
            const link = tooltip.shadowRoot.querySelector('#link');
            assertEquals(link.getAttribute('aria-hidden'), 'true');
            assertEquals('', link.textContent.trim());
            assertFalse(link.hasAttribute('aria-label'));
            assertEquals('#', link.getAttribute('href'));

            assertTrue(tooltip.hasAttribute('visible'));
            assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

            assertTrue(tooltip.classList.contains('card-tooltip'));
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
