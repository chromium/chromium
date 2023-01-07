/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Closure user agent detection (Browser).
 * @see <a href="http://www.useragentstring.com/">User agent strings</a>
 * For more information on rendering engine, platform, or device see the other
 * sub-namespaces in goog.labs.userAgent, goog.labs.userAgent.platform,
 * goog.labs.userAgent.device respectively.)
 */

goog.module('goog.labs.userAgent.browser');
goog.module.declareLegacyNamespace();

const googArray = goog.require('goog.array');
const googObject = goog.require('goog.object');
const util = goog.require('goog.labs.userAgent.util');
const {compareVersions} = goog.require('goog.string.internal');

// TODO(nnaze): Refactor to remove excessive exclusion logic in matching
// functions.

/**
 * @return {boolean} Whether to use navigator.userAgentData to determine
 * browser's brand.
 */
function useUserAgentBrand() {
  const userAgentData = util.getUserAgentData();
  return !!userAgentData && userAgentData.brands.length > 0;
}

/**
 * @return {boolean} Whether the user's browser is Opera. Note: Chromium based
 *     Opera (Opera 15+) is detected as Chrome to avoid unnecessary special
 *     casing.
 */
function matchOpera() {
  if (util.ASSUME_CLIENT_HINTS_SUPPORT || util.getUserAgentData()) {
    // This will remain false for non Chromium based Opera.
    return false;
  }
  return util.matchUserAgent('Opera');
}

/** @return {boolean} Whether the user's browser is IE. */
function matchIE() {
  if (util.ASSUME_CLIENT_HINTS_SUPPORT || util.getUserAgentData()) {
    // This will remain false for IE.
    return false;
  }
  return util.matchUserAgent('Trident') || util.matchUserAgent('MSIE');
}

/**
 * @return {boolean} Whether the user's browser is Edge. This refers to
 *     EdgeHTML based Edge.
 */
function matchEdgeHtml() {
  if (util.ASSUME_CLIENT_HINTS_SUPPORT || util.getUserAgentData()) {
    // This will remain false for non chromium based Edge.
    return false;
  }
  return util.matchUserAgent('Edge');
}

/** @return {boolean} Whether the user's browser is Chromium based Edge. */
function matchEdgeChromium() {
  if (useUserAgentBrand()) {
    return util.matchUserAgentDataBrand('Edge');
  }
  return util.matchUserAgent('Edg/');
}

/** @return {boolean} Whether the user's browser is Chromium based Opera. */
function matchOperaChromium() {
  if (useUserAgentBrand()) {
    return util.matchUserAgentDataBrand('Opera');
  }
  return util.matchUserAgent('OPR');
}

/** @return {boolean} Whether the user's browser is Firefox. */
function matchFirefox() {
  if (useUserAgentBrand()) {
    return util.matchUserAgentDataBrand('Firefox');
  }
  return util.matchUserAgent('Firefox') || util.matchUserAgent('FxiOS');
}

/** @return {boolean} Whether the user's browser is Safari. */
function matchSafari() {
  if (useUserAgentBrand()) {
    // This will always be false before Safari adopt the Client Hint support.
    return util.matchUserAgentDataBrand('Safari');
  }
  return util.matchUserAgent('Safari') &&
      !(matchChrome() || matchCoast() || matchOpera() || matchEdgeHtml() ||
        matchEdgeChromium() || matchOperaChromium() || matchFirefox() ||
        isSilk() || util.matchUserAgent('Android'));
}

/**
 * @return {boolean} Whether the user's browser is Coast (Opera's Webkit-based
 *     iOS browser).
 */
function matchCoast() {
  if (util.ASSUME_CLIENT_HINTS_SUPPORT || util.getUserAgentData()) {
    // This will remain false for Coast.
    return false;
  }
  return util.matchUserAgent('Coast');
}

/** @return {boolean} Whether the user's browser is iOS Webview. */
function matchIosWebview() {
  // iOS Webview does not show up as Chrome or Safari. Also check for Opera's
  // WebKit-based iOS browser, Coast.
  return (util.matchUserAgent('iPad') || util.matchUserAgent('iPhone')) &&
      !matchSafari() && !matchChrome() && !matchCoast() && !matchFirefox() &&
      util.matchUserAgent('AppleWebKit');
}

/**
 * @return {boolean} Whether the user's browser is any Chromium browser. This
 *     returns true for Chrome, Opera 15+, and Edge Chromium.
 */
function matchChrome() {
  if (useUserAgentBrand()) {
    return util.matchUserAgentDataBrand('Chromium');
  }
  return (util.matchUserAgent('Chrome') || util.matchUserAgent('CriOS')) &&
      !matchEdgeHtml();
}

/** @return {boolean} Whether the user's browser is the Android browser. */
function matchAndroidBrowser() {
  // Android can appear in the user agent string for Chrome on Android.
  // This is not the Android standalone browser if it does.
  return util.matchUserAgent('Android') &&
      !(isChrome() || isFirefox() || isOpera() || isSilk());
}

/** @return {boolean} Whether the user's browser is Opera. */
const isOpera = matchOpera;

/** @return {boolean} Whether the user's browser is IE. */
const isIE = matchIE;

/** @return {boolean} Whether the user's browser is EdgeHTML based Edge. */
const isEdge = matchEdgeHtml;

/** @return {boolean} Whether the user's browser is Chromium based Edge. */
const isEdgeChromium = matchEdgeChromium;

/** @return {boolean} Whether the user's browser is Chromium based Opera. */
const isOperaChromium = matchOperaChromium;

/** @return {boolean} Whether the user's browser is Firefox. */
const isFirefox = matchFirefox;

/** @return {boolean} Whether the user's browser is Safari. */
const isSafari = matchSafari;

/**
 * @return {boolean} Whether the user's browser is Coast (Opera's Webkit-based
 *     iOS browser).
 */
const isCoast = matchCoast;

/** @return {boolean} Whether the user's browser is iOS Webview. */
const isIosWebview = matchIosWebview;

/**
 * @return {boolean} Whether the user's browser is any Chromium based browser (
 *     Chrome, Blink-based Opera (15+) and Edge Chromium).
 */
const isChrome = matchChrome;

/** @return {boolean} Whether the user's browser is the Android browser. */
const isAndroidBrowser = matchAndroidBrowser;

/**
 * For more information, see:
 * http://docs.aws.amazon.com/silk/latest/developerguide/user-agent.html
 * @return {boolean} Whether the user's browser is Silk.
 */
function isSilk() {
  if (useUserAgentBrand()) {
    return util.matchUserAgentDataBrand('Silk');
  }
  return util.matchUserAgent('Silk');
}

/**
 * @return {string} The browser version or empty string if version cannot be
 *     determined. Note that for Internet Explorer, this returns the version of
 *     the browser, not the version of the rendering engine. (IE 8 in
 *     compatibility mode will return 8.0 rather than 7.0. To determine the
 *     rendering engine version, look at document.documentMode instead. See
 *     http://msdn.microsoft.com/en-us/library/cc196988(v=vs.85).aspx for more
 *     details.)
 */
function getVersion() {
  const userAgentString = util.getUserAgent();
  // Special case IE since IE's version is inside the parenthesis and
  // without the '/'.
  if (isIE()) {
    return getIEVersion(userAgentString);
  }

  const versionTuples = util.extractVersionTuples(userAgentString);

  // Construct a map for easy lookup.
  const versionMap = {};
  versionTuples.forEach((tuple) => {
    // Note that the tuple is of length three, but we only care about the
    // first two.
    const key = tuple[0];
    const value = tuple[1];
    versionMap[key] = value;
  });

  const versionMapHasKey = goog.partial(googObject.containsKey, versionMap);

  // Gives the value with the first key it finds, otherwise empty string.
  function lookUpValueWithKeys(keys) {
    const key = googArray.find(keys, versionMapHasKey);
    return versionMap[key] || '';
  }

  // Check Opera before Chrome since Opera 15+ has "Chrome" in the string.
  // See
  // http://my.opera.com/ODIN/blog/2013/07/15/opera-user-agent-strings-opera-15-and-beyond
  if (isOpera()) {
    // Opera 10 has Version/10.0 but Opera/9.8, so look for "Version" first.
    // Opera uses 'OPR' for more recent UAs.
    return lookUpValueWithKeys(['Version', 'Opera']);
  }

  // Check Edge before Chrome since it has Chrome in the string.
  if (isEdge()) {
    return lookUpValueWithKeys(['Edge']);
  }

  // Check Chromium Edge before Chrome since it has Chrome in the string.
  if (isEdgeChromium()) {
    return lookUpValueWithKeys(['Edg']);
  }

  if (isChrome()) {
    return lookUpValueWithKeys(['Chrome', 'CriOS', 'HeadlessChrome']);
  }

  // Usually products browser versions are in the third tuple after "Mozilla"
  // and the engine.
  const tuple = versionTuples[2];
  return tuple && tuple[1] || '';
}

/**
 * @param {string|number} version The version to check.
 * @return {boolean} Whether the browser version is higher or the same as the
 *     given version.
 */
function isVersionOrHigher(version) {
  return compareVersions(getVersion(), version) >= 0;
}

/**
 * Determines IE version. More information:
 * http://msdn.microsoft.com/en-us/library/ie/bg182625(v=vs.85).aspx#uaString
 * http://msdn.microsoft.com/en-us/library/hh869301(v=vs.85).aspx
 * http://blogs.msdn.com/b/ie/archive/2010/03/23/introducing-ie9-s-user-agent-string.aspx
 * http://blogs.msdn.com/b/ie/archive/2009/01/09/the-internet-explorer-8-user-agent-string-updated-edition.aspx
 * @param {string} userAgent the User-Agent.
 * @return {string}
 */
function getIEVersion(userAgent) {
  // IE11 may identify itself as MSIE 9.0 or MSIE 10.0 due to an IE 11 upgrade
  // bug. Example UA:
  // Mozilla/5.0 (MSIE 9.0; Windows NT 6.1; WOW64; Trident/7.0; rv:11.0)
  // like Gecko.
  // See http://www.whatismybrowser.com/developers/unknown-user-agent-fragments.
  const rv = /rv: *([\d\.]*)/.exec(userAgent);
  if (rv && rv[1]) {
    return rv[1];
  }

  let version = '';
  const msie = /MSIE +([\d\.]+)/.exec(userAgent);
  if (msie && msie[1]) {
    // IE in compatibility mode usually identifies itself as MSIE 7.0; in this
    // case, use the Trident version to determine the version of IE. For more
    // details, see the links above.
    const tridentVersion = /Trident\/(\d.\d)/.exec(userAgent);
    if (msie[1] == '7.0') {
      if (tridentVersion && tridentVersion[1]) {
        switch (tridentVersion[1]) {
          case '4.0':
            version = '8.0';
            break;
          case '5.0':
            version = '9.0';
            break;
          case '6.0':
            version = '10.0';
            break;
          case '7.0':
            version = '11.0';
            break;
        }
      } else {
        version = '7.0';
      }
    } else {
      version = msie[1];
    }
  }
  return version;
}

exports = {
  getVersion,
  isAndroidBrowser,
  isChrome,
  isCoast,
  isEdge,
  isEdgeChromium,
  isFirefox,
  isIE,
  isIosWebview,
  isOpera,
  isOperaChromium,
  isSafari,
  isSilk,
  isVersionOrHigher,
};
