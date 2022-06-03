// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Shortens a package name to be displayed in the svg.
 * @param {string} name The full package name to shorten.
 * @return {string} The shortened package name.
 */
function shortenPackageName(name) {
  return name
      .replace('org.chromium.', '.')
      .replace('chrome.browser.', 'c.b.');
}

/**
 * Shortens a class name to be displayed in the svg.
 * @param {string} name The full class name to shorten.
 * @return {string} The shortened class name.
 */
function shortenClassName(name) {
  return name.substring(name.lastIndexOf('.') + 1);
}

/**
 * Splits a full class name into its package and class name.
 * @param {string} name The full class name to split.
 * @return {!Array<string>} An array of [packageName, className].
 */
function splitClassName(name) {
  const lastDotIdx = name.lastIndexOf('.');
  const packageName = name.substring(0, lastDotIdx);
  const className = name.substring(lastDotIdx + 1);
  return [packageName, className];
}

export {
  shortenPackageName,
  shortenClassName,
  splitClassName,
};
