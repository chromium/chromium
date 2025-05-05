/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.userAgent.testAgentData');
goog.setTestOnly();

const googArray = goog.require('goog.array');
const {deepFreeze} = goog.require('goog.debug.deepFreeze');

/**
 * Based on UADataValues
 * @record
 */
class PartialUADataValues {
  constructor() {
    /** @type {string|undefined} */
    this.platform;

    /** @type {string|undefined} */
    this.platformVersion;

    /** @type {string|undefined} */
    this.architecture;

    /** @type {string|undefined} */
    this.model;

    /** @type {string|undefined} */
    this.uaFullVersion;

    /** @type {string|undefined} */
    this.bitness;

    /** @type {!Array<!NavigatorUABrandVersion>|undefined} */
    this.fullVersionList;
  }
}

/**
 * Shuffles a copy of the input array and returns it.
 * @param {!Array<T>} arr
 * @return {!Array<T>}
 * @template T
 */
function shuffle(arr) {
  const result = [...arr];
  googArray.shuffle(result);
  return result;
}

/**
 * Returns a copy of the specified userAgentData object with functional
 * getHighEntropyValues that return the specified high-entropy values.
 * @param {!NavigatorUAData} userAgentData The userAgentData whose synchronously
 *     available values should be present on the return value.
 * @param {!PartialUADataValues} highEntropyData The values that should be
 *     returned when high-entropy values are requested. If a value is missing,
 *     getHighEntropyValues will return a rejected Promise if that value is
 *     requested.
 * @return {!NavigatorUAData}
 */
function withHighEntropyData(userAgentData, highEntropyData) {
  return /** @type {!NavigatorUAData} */ (Object.freeze({
    ...userAgentData,
    getHighEntropyValues: (hints) => {
      const result = {};
      for (const hint of hints) {
        if (/** @type {*} */ (highEntropyData)[hint] === undefined) {
          return Promise.reject(
              new Error(`High-entropy value not available: ${hint}`));
        } else {
          result[hint] = /** @type {*} */ (highEntropyData)[hint];
        }
      }
      return Promise.resolve(result);
    }
  }));
}
exports.withHighEntropyData = withHighEntropyData;

/**
 * The default value used for getHighEntropyValues.
 * @param {!Array<string>} hints
 * @return {!Promise<!UADataValues>}
 */
async function getHighEntropyValuesMock(hints) {
  throw new Error('High-entropy values not available.');
}

/** @const {!NavigatorUAData} */
exports.INCOMPLETE_USERAGENT_DATA = Object.freeze({
  brands: Object.freeze([]),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'Unknown',
});

/** @const {!NavigatorUAData} */
exports.CHROME_USERAGENT_DATA_MOBILE = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Not; A Brand', version: '99'},
    {brand: 'Google Chrome', version: '101'},
    {brand: 'Chromium', version: '101'},
  ])),
  mobile: true,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'Android',
});

/** @const {!NavigatorUAData} */
exports.CHROME_USERAGENT_DATA_LINUX = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Not; A Brand', version: '0'},
    {brand: 'Google Chrome', version: '101'},
    {brand: 'Chromium', version: '101'},
  ])),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'Linux',
});

/** @const {!NavigatorUAData} */
exports.CHROME_USERAGENT_DATA_MACOS = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Not; A Brand', version: '0'},
    {brand: 'Google Chrome', version: '101'},
    {brand: 'Chromium', version: '101'},
  ])),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'macOS',
});

/** @const {!NavigatorUAData} */
exports.CHROME_USERAGENT_DATA_WINDOWS = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Not; A Brand', version: '0'},
    {brand: 'Google Chrome', version: '101'},
    {brand: 'Chromium', version: '101'},
  ])),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'Windows',
});

/** @const {!NavigatorUAData} */
exports.CHROME_USERAGENT_DATA_CROS = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Not; A Brand', version: '0'},
    {brand: 'Google Chrome', version: '94'},
    {brand: 'Chromium', version: '94'},
  ])),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'Chrome OS',
});

/** @const {!NavigatorUAData} */
exports.CHROME_USERAGENT_DATA_UNKNOWN = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Not; A Brand', version: '0'},
    {brand: 'Google Chrome', version: '101'},
    {brand: 'Chromium', version: '101'},
  ])),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'Unknown',
});

/** @const {!NavigatorUAData} */
exports.CHROME_USERAGENT_DATA_NO_PLATFORM = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Not; A Brand', version: '0'},
    {brand: 'Google Chrome', version: '92'},
    {brand: 'Chromium', version: '92'},
  ])),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'Unknown',
});

/** @const {!NavigatorUAData} */
exports.CHROME_NO_FULLVERSIONLIST_USERAGENT_DATA = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Not; A Brand', version: '0'},
    {brand: 'Google Chrome', version: '91'},
    {brand: 'Chromium', version: '91'},
  ])),
  mobile: false,
  getHighEntropyValues: async (fields) => {
    // Simulate an environment where fullVersionList is not yet implemented,
    // which is true as of Chrome 91.
    if (fields.length !== 1 && fields[0] !== 'fullVersionList') {
      throw new Error('High-entropy values not available.');
    } else {
      return {};
    }
  },
  platform: 'macOS',
});

/** @const {!NavigatorUAData} */
exports.OPERACHROMIUM_USERAGENT_DATA = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Opera', version: '87'},
    {brand: 'Chromium', version: '101'},
    {brand: ';Not A Brand', version: '99'},
  ])),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'macOS',
});

/** @const {!NavigatorUAData} */
exports.EDGECHROMIUM_USERAGENT_DATA = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Chromium', version: '101'},
    {brand: 'Microsoft Edge', version: '101'},
    {brand: 'GREASE', version: '99'},
  ])),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'Windows',
});

/** @const {!NavigatorUAData} */
exports.SILK_USERAGENT_DATA = Object.freeze({
  brands: deepFreeze(shuffle([
    {brand: 'Not;A Brand', version: '99'},
    {brand: 'Chromium', version: '93'},
  ])),
  mobile: false,
  getHighEntropyValues: getHighEntropyValuesMock,
  platform: 'Android',
});
