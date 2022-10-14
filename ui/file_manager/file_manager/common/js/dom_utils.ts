// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Function to be used as event listener for `mouseenter`, it sets the `title`
 * attribute in the event's element target, when the text content is clipped due
 * to CSS overflow, as in showing `...`.
 *
 * NOTE: This should be used with `mouseenter` because this event triggers less
 * frequent than `mouseover` and `mouseenter` doesn't bubble from the children
 * element to the listener (see mouseenter on MDN for more details).
 */
export function mouseEnterMaybeShowTooltip(event: MouseEvent) {
  const target = event.composedPath()[0] as HTMLElement;
  if (!target) {
    return;
  }
  maybeShowTooltip(target, target.innerText);
}

/**
 * Sets the `title` attribute in the event's element target, when the text
 * content is clipped due to CSS overflow, as in showing `...`.
 */
export function maybeShowTooltip(target: HTMLElement, title: string) {
  if (hasOverflowEllipsis(target)) {
    target.setAttribute('title', title);
  } else {
    target.removeAttribute('title');
  }
}

/**
 * Whether the text content is clipped due to CSS overflow, as in showing `...`.
 */
export function hasOverflowEllipsis(element: HTMLElement) {
  return element.offsetWidth < element.scrollWidth;
}
