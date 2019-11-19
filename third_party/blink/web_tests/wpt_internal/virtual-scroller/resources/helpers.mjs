/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Helpers for testing virtual-scroller code.
 * @package
 */

const ONE_SECOND_MS = 1000;

/**
 * Creates a DIV with id and textContent set to |id|.
 */
export function div(id) {
  const d = document.createElement('div');
  d.id = id;
  d.textContent = id;
  return d;
}

/**
 * Creates a 5000px DIV with a solid border with id and textContent
 * set to |id|.
 */
export function largeDiv(id) {
  const large = div(id);
  large.style.height = '5000px';
  large.style.border = 'solid';
  return large;
}

/**
 * Creates a container DIV with |n| child DIVs with their margin=|margin|.
 */
export function appendDivs(container, n, margin) {
  for (let i = 0; i < n; i++) {
    const d = div('d' + i);
    d.style.margin = margin;
    container.append(d);
  }
}

/*
 * Creates an element, appends it to the BODY, passes it to |callback| and
 * removes it in a finally.
 */
export function withElement(name, callback) {
  const element = document.createElement(name);
  try {
    document.body.appendChild(element);
    callback(element);
  } finally {
    element.remove();
  }
}

/**
 * Remove the reftest-wait class from the HTML element.
 *
 * This includes a hack to wait 1s to give the virtual-scroller
 * elements time to settle.
 */
export function stopWaiting() {
  setTimeout(() => {
    document.documentElement.classList.remove('reftest-wait');
  }, ONE_SECOND_MS);
}

/**
 * Generate a string with |n| words.
 */
export function words(n) {
  let w = '';
  for (let i = 0; i < n; i++) {
    w += 'word ';
  }
  return w;
}

/**
 * Allow the next |n| frames to end and then call |callback| ASAP in
 * the following frame.
 */
export function inNFrames(n, callback) {
  if (n == 0) {
    window.setTimeout(callback, 0);
  } else {
    window.requestAnimationFrame(() => {
      inNFrames(n - 1, callback);
    });
  }
}

/**
 * Returns a promise which will resolve as soon as possible after |n|
 * RAFs have completed. So if |n| is 1, this will resolve ASAP in the
 * next frame.
 */
export function nFrames(n) {
  return new Promise(resolve => {
    inNFrames(n, () => resolve());
  });
}
