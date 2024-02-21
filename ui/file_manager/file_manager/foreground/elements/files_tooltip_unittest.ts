// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './files_tooltip.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {FilesTooltip} from './files_tooltip.js';

let chocolateButton: HTMLButtonElement;
let cherriesButton: HTMLButtonElement;
let cheeseButton: HTMLButtonElement;
let otherButton: HTMLButtonElement;
let tooltip: FilesTooltip;

const WINDOW_EDGE_PADDING = 6;

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
  chocolateButton = document.querySelector<HTMLButtonElement>('#chocolate')!;
  cherriesButton = document.querySelector<HTMLButtonElement>('#cherries')!;
  cheeseButton = document.querySelector<HTMLButtonElement>('#cheese')!;
  otherButton = document.querySelector<HTMLButtonElement>('#other')!;

  tooltip = document.querySelector<FilesTooltip>('files-tooltip')!;
  assertNotEquals('none', window.getComputedStyle(tooltip).display);
  assertEquals('0', window.getComputedStyle(tooltip).opacity);

  tooltip.addTargets([chocolateButton, cherriesButton, cheeseButton]);
}

function waitForMutation(target: FilesTooltip) {
  return new Promise<void>((fulfill) => {
    const observer = new MutationObserver(_ => {
      observer.disconnect();
      fulfill();
    });
    observer.observe(target, {attributes: true});
  });
}

export async function testFocus() {
  chocolateButton.focus();

  await waitForMutation(tooltip);
  const label1 = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Chocolate!', label1.textContent?.trim());
  assertTrue(tooltip.hasAttribute('visible'));
  assertEquals('6px', tooltip.style.left);
  assertEquals('78px', tooltip.style.top);

  cherriesButton.focus();
  await waitForMutation(tooltip);

  const label2 = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Cherries!', label2.textContent?.trim());
  assertTrue(tooltip.hasAttribute('visible'));

  const expectedLeft = document.body.offsetWidth - tooltip.offsetWidth -
      WINDOW_EDGE_PADDING + 'px';
  assertEquals(expectedLeft, tooltip.style.left);
  assertEquals('78px', tooltip.style.top);

  otherButton.focus();
  await waitForMutation(tooltip);
  assertFalse(!!tooltip.getAttribute('visible'));
}

export async function testFocusWithLink() {
  cherriesButton.dataset['tooltipLinkHref'] = 'https://cherries.com';
  cherriesButton.dataset['tooltipLinkAriaLabel'] =
      'Click here to get more cherries';
  cherriesButton.dataset['tooltipLinkText'] = 'More cherries';

  chocolateButton.focus();

  await waitForMutation(tooltip);

  const label1 = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Chocolate!', label1.textContent?.trim());
  assertTrue(tooltip.hasAttribute('visible'));
  assertEquals('6px', tooltip.style.left);
  assertEquals('78px', tooltip.style.top);

  cherriesButton.focus();
  await waitForMutation(tooltip);
  // Check the label.
  const label2 = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Cherries!', label2.textContent?.trim());
  // Check the link: it should be visible now.
  const link1 = tooltip.shadowRoot!.querySelector<HTMLLinkElement>('#link')!;
  assertEquals(link1.getAttribute('aria-hidden'), 'false');
  assertEquals('More cherries', link1.textContent?.trim());
  assertEquals(
      'Click here to get more cherries', link1.getAttribute('aria-label'));
  assertEquals('https://cherries.com', link1.getAttribute('href'));

  assertEquals(tooltip.getAttribute('aria-hidden'), 'false');
  assertTrue(tooltip.hasAttribute('visible'));

  const expectedLeft = document.body.offsetWidth - tooltip.offsetWidth -
      WINDOW_EDGE_PADDING + 'px';
  assertEquals(expectedLeft, tooltip.style.left);
  assertEquals('78px', tooltip.style.top);

  chocolateButton.focus();
  await waitForMutation(tooltip);
  // Check the label.
  const label3 = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Chocolate!', label3.textContent?.trim());
  // Check the link: it should be hidden and cleared out.
  const link2 = tooltip.shadowRoot!.querySelector<HTMLLinkElement>('#link')!;
  assertEquals(link2.getAttribute('aria-hidden'), 'true');
  assertEquals('', link2.textContent?.trim());
  assertFalse(link2.hasAttribute('aria-label'));
  assertEquals('#', link2.getAttribute('href'));
}

export async function testFocusWithLabelChange() {
  chocolateButton.focus();

  await waitForMutation(tooltip);
  const label1 = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Chocolate!', label1.textContent?.trim());
  // Change the button's aria-label attribute and the tooltip should
  // also update.
  chocolateButton.setAttribute('aria-label', 'New chocolate!');

  tooltip.updateTooltipText(chocolateButton);
  await waitForMutation(tooltip);

  const label2 = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('New chocolate!', label2.textContent?.trim());
}

export async function testHover() {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover'));

  await waitForMutation(tooltip);
  const label1 = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Chocolate!', label1.textContent?.trim());
  assertTrue(tooltip.hasAttribute('visible'));
  assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

  assertEquals('6px', tooltip.style.left);
  assertEquals('78px', tooltip.style.top);

  chocolateButton.dispatchEvent(new MouseEvent('mouseout'));
  cherriesButton.dispatchEvent(new MouseEvent('mouseover'));
  await waitForMutation(tooltip);

  const label2 = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Cherries!', label2.textContent?.trim());
  assertTrue(tooltip.hasAttribute('visible'));

  const expectedLeft = document.body.offsetWidth - tooltip.offsetWidth -
      WINDOW_EDGE_PADDING + 'px';
  assertEquals(expectedLeft, tooltip.style.left);
  assertEquals('78px', tooltip.style.top);

  cherriesButton.dispatchEvent(new MouseEvent('mouseout'));
  await waitForMutation(tooltip);

  assertFalse(!!tooltip.getAttribute('visible'));
}

export async function testClickHides() {
  chocolateButton.dispatchEvent(new MouseEvent('mouseover', {bubbles: true}));

  await waitForMutation(tooltip);

  const label = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Chocolate!', label.textContent?.trim());
  assertTrue(tooltip.hasAttribute('visible'));
  // Hiding here is synchronous. Dispatch the event asynchronously,
  // so the mutation observer is started before hiding.
  setTimeout(() => {
    document.body.dispatchEvent(new MouseEvent('mousedown'));
  });
  await waitForMutation(tooltip);

  assertFalse(tooltip.hasAttribute('visible'));
  assertEquals(tooltip.getAttribute('aria-hidden'), 'true');
}

export async function testCardTooltipHover() {
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  await waitForMutation(tooltip);

  const label = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Cheese!', label.textContent?.trim());
  assertTrue(tooltip.hasAttribute('visible'));
  assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

  assertEquals('card-tooltip', tooltip.className);
  assertEquals('card-label', label.className);

  assertEquals('38px', tooltip.style.left);
  assertEquals('162px', tooltip.style.top);

  cheeseButton.dispatchEvent(new MouseEvent('mouseout'));
  await waitForMutation(tooltip);

  assertFalse(!!tooltip.getAttribute('visible'));
}

export async function testCardTooltipRTL() {
  document.documentElement.setAttribute('dir', 'rtl');
  document.body.setAttribute('dir', 'rtl');

  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  await waitForMutation(tooltip);

  const label = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Cheese!', label.textContent?.trim());
  assertTrue(tooltip.hasAttribute('visible'));
  assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

  assertEquals('card-tooltip', tooltip.className);
  assertEquals('card-label', label.className);

  // A border with 1px insets (top=bottom=left=right=1px) will be
  // applied to the window when drak/light feature is enabled. See
  // more details at crrev.com/c/3656414.
  assertTrue(`962px` === tooltip.style.left || `960px` === tooltip.style.left);
  assertEquals('162px', tooltip.style.top);

  cheeseButton.dispatchEvent(new MouseEvent('mouseout'));
  await waitForMutation(tooltip);

  // revert back document direction to not impact other tests.
  document.documentElement.setAttribute('dir', 'ltr');
  document.body.setAttribute('dir', 'ltr');
}

export async function testCardTooltipWithLinkHover() {
  cheeseButton.dataset['tooltipLinkHref'] = 'https://cheese.com';
  cheeseButton.dataset['tooltipLinkAriaLabel'] =
      'Click here to get more cheese';
  cheeseButton.dataset['tooltipLinkText'] = 'More cheese';
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  await waitForMutation(tooltip);

  // Check the label.
  const label = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Cheese!', label.textContent?.trim());
  // Check the link: it should be visible now.
  const link = tooltip.shadowRoot!.querySelector<HTMLLinkElement>('#link')!;
  assertEquals(link.getAttribute('aria-hidden'), 'false');
  assertEquals('More cheese', link.textContent?.trim());
  assertEquals(
      'Click here to get more cheese', link.getAttribute('aria-label'));
  assertEquals('https://cheese.com', link.getAttribute('href'));

  assertTrue(tooltip.hasAttribute('visible'));
  assertEquals(tooltip.getAttribute('aria-hidden'), 'false');

  assertTrue(tooltip.classList.contains('card-tooltip'));
  assertEquals('card-label', label.className);

  assertEquals('38px', tooltip.style.left);
  assertEquals('162px', tooltip.style.top);

  assertTrue(tooltip.classList.contains('link-tooltip'));

  cheeseButton.dispatchEvent(new MouseEvent('mouseout'));
  await waitForMutation(tooltip);

  assertFalse(!!tooltip.getAttribute('visible'));
}

export async function testTooltipWithIncompleteLinkHover() {
  cheeseButton.dataset['tooltipLinkHref'] = 'https://cheese.com';
  cheeseButton.dispatchEvent(new MouseEvent('mouseover'));

  await waitForMutation(tooltip);

  // Check the label.
  const label = tooltip.shadowRoot!.querySelector<HTMLDivElement>('#label')!;
  assertEquals('Cheese!', label.textContent?.trim());
  // Check the link: it should be hidden since not all required
  // attributes are set.
  const link = tooltip.shadowRoot!.querySelector<HTMLLinkElement>('#link')!;
  assertEquals(link.getAttribute('aria-hidden'), 'true');
  assertEquals('', link.textContent?.trim());
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
  await waitForMutation(tooltip);

  assertFalse(!!tooltip.getAttribute('visible'));
}
