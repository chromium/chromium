// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {XfBase} from '../../widgets/xf_base.js';

/**
 * Wait for the update (render/re-render) of the `element` to be finished
 * in unit test.
 *
 */
export async function waitForElementUpdate(element: HTMLElement):
    Promise<void> {
  // For LitElement we explicitly await the internal updateComplete promise.
  if (element instanceof XfBase) {
    await element.updateComplete;
    // Wait for nested LitElements to finish rendering. Assumes that all nested
    // elements' render() complete in microtasks.
    return new Promise(resolve => setTimeout(resolve, 0));
  }
  // For others, wait for the next animation frame.
  return new Promise(resolve => window.requestAnimationFrame(() => resolve()));
}
