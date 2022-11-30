/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.uri');

const Const = goog.require('goog.string.Const');
const TagName = goog.require('goog.dom.TagName');
const uncheckedconversions = goog.require('goog.html.uncheckedconversions');
const {createElement} = goog.require('goog.dom');
const {setAnchorHref} = goog.require('goog.dom.safe');

/**
 * Normalizes a URL by assigning it to an anchor element and reading back href.
 *
 * This converts relative URLs to absolute, and cleans up whitespace.
 * @param {string} uri A string containing a URI.
 * @return {string} Normalized, absolute form of uri.
 */
function normalizeUri(uri) {
  const anchor = createElement(TagName.A);
  // This is safe even though the URL might be untrustworthy.
  // The SafeURL is only used to set the href of an HTMLAnchorElement
  // that is never added to the DOM. Therefore, the user cannot navigate
  // to this URL.
  const safeUrl =
      uncheckedconversions.safeUrlFromStringKnownToSatisfyTypeContract(
          Const.from('This URL is never added to the DOM'), uri);
  setAnchorHref(anchor, safeUrl);
  return anchor.href;
}
exports.normalizeUri = normalizeUri;

/**
 * Gets the href property of an anchor element, suppressing exceptions coming
 * from certain URLs in IE.
 * @param {!HTMLAnchorElement} element
 * @return {?string}
 * @deprecated This format is deprecated in RFC 3986. Use this function only for
 * legacy behavior, and avoid accepting such URLs in new code.
 */
function getHref(element) {
  try {
    return element.href || null;
  } catch (x) {
    // IE throws a security exception for urls including username/password:
    // http://user:password@example.com/
    return null;
  }
}
exports.getHref = getHref;
