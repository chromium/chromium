// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assert} from './assert.js';
import {isIOS} from './platform.js';
// clang-format on


let hideInk = false;

assert(!isIOS, 'pointerdown doesn\'t work on iOS');

document.addEventListener('pointerdown', function() {
  hideInk = true;
}, true);

document.addEventListener('keydown', function() {
  hideInk = false;
}, true);

/**
 * Attempts to track whether focus outlines should be shown, and if they
 * shouldn't, removes the "ink" (ripple) from a control while focusing it.
 * This is helpful when a user is clicking/touching, because it's not super
 * helpful to show focus ripples in that case. This is Polymer-specific.
 */
export function focusWithoutInk(toFocus: HTMLElement) {
  // |toFocus| does not have a 'noink' property, so it's unclear whether the
  // element has "ink" and/or whether it can be suppressed. Just focus().
  if (!('noink' in toFocus) || !hideInk) {
    toFocus.focus();
    return;
  }

  const toFocusWithNoInk = toFocus as HTMLElement & {noink: boolean};

  // Make sure the element is in the document we're listening to events on.
  assert(document === toFocusWithNoInk.ownerDocument);
  const {noink} = toFocusWithNoInk;
  toFocusWithNoInk.noink = true;
  toFocusWithNoInk.focus();
  toFocusWithNoInk.noink = noink;
}

