// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Shortens a class name to be displayed in the svg.
 *
 * @param {string} name The full class name to shorten.
 * @return {string} The shortened class name.
 */
function shortenClassName(name) {
  return name.substring(name.lastIndexOf('.') + 1);
}

/**
 * Shortens a package name to be displayed in the svg.
 *
 * @param {string} name The full package name to shorten.
 * @return {string} The shortened package name.
 */
function shortenPackageName(name) {
  return name.replace('org.chromium.', '.').replace('chrome.browser.', 'c.b.');
}

/**
 * Shortens a target name to be displayed in the svg.
 *
 * Since the target name is always in GN format (e.g. //base:base_java), we can
 * assume that it always has at least two forward slashes.
 *
 * @param {string} name The full target name to shorten.
 * @return {string} The shortened package name.
 */
function shortenTargetName(name) {
  const lastSlashIdx = name.lastIndexOf('/');
  const secondLastSlashIdx = name.lastIndexOf('/', lastSlashIdx - 1);
  if (secondLastSlashIdx < 2) {
    // This is if we are matching into the first two //.
    return name;
  }
  return name.substring(secondLastSlashIdx + 1);
}

/**
 * Splits a full class name into its package and class name.
 *
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
  shortenClassName,
  shortenPackageName,
  shortenTargetName,
  splitClassName,
};
