// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The different possible absolute pathnames for the visualization page.
 *
 * @readonly
 * @enum {string}
 */
const PagePathName = {
  CLASS: '/class_view.html',
  PACKAGE: '/package_view.html',
  TARGET: '/target_view.html',
};

// Keys for identifying URL params.
const URL_PARAM_KEYS = {
  // Common keys:
  DISPLAY_SETTINGS_PRESET: 'dsp',
  FILTER_NAMES: 'fn',
  FILTER_CHECKED: 'fc',
  INBOUND_DEPTH: 'ibd',
  OUTBOUND_DEPTH: 'obd',
  CURVE_EDGES: 'ce',
  COLOR_ONLY_ON_HOVER: 'cooh',
  EDGE_COLOR: 'ec',
  // Class page-specific keys:
  HULL_DISPLAY: 'hd',
};

/** Helper class for generating and parsing the page URL. */
class UrlProcessor {
  /** @param {*} searchParams The base URLSearchParams for the processor. */
  constructor(searchParams) {
    this.searchParams = searchParams;
  }

  /**
   * @return {!UrlProcessor} a new UrlProcessor with no existing search params.
   */
  static createForOutput() {
    return new UrlProcessor(new URLSearchParams());
  }

  /**
   * Creates a URL using the current search params.
   *
   * @param {string} originUrl The URL to use as the origin for the generated
   *   URL.
   * @param {PagePathName} pathName The pathname for the generated URL.
   * @return {string} The new URL containing the search params.
   */
  getUrl(originUrl, pathName) {
    const pageUrl = new URL(originUrl);
    return `${pageUrl.origin}${pathName}?${this.searchParams.toString()}`;
  }

  /**
   * @param {string} key
   * @param {string|number|boolean} value
   */
  append(key, value) {
    this.searchParams.append(key, value);
  }

  /**
   * @param {string} key
   * @param {!Array<string>} valueArr
   */
  appendArray(key, valueArr) {
    this.searchParams.append(key, valueArr.join(','));
  }

  /**
   * @param {string} key
   * @param {!Array<string>} defaultVal
   * @return {!Array<string>}
   */
  getArray(key, defaultVal) {
    const arrayStr = this.searchParams.get(key);
    return (arrayStr === null) ? defaultVal : arrayStr.split(',');
  }

  /**
   * @param {string} key
   * @param {string} defaultVal
   * @return {string}
   */
  getString(key, defaultVal) {
    const str = this.searchParams.get(key);
    return (str === null) ? defaultVal : str;
  }

  /**
   * @param {string} key
   * @param {number} defaultVal
   * @return {number}
   */
  getInt(key, defaultVal) {
    const intStr = this.searchParams.get(key);
    return (intStr === null) ? defaultVal : parseInt(intStr, 10);
  }

  /**
   * @param {string} key
   * @param {boolean} defaultVal
   * @return {boolean}
   */
  getBool(key, defaultVal) {
    const boolStr = this.searchParams.get(key);
    return (boolStr === null) ? defaultVal : (boolStr === 'true');
  }
}

export {
  PagePathName,
  URL_PARAM_KEYS,
  UrlProcessor,
};
