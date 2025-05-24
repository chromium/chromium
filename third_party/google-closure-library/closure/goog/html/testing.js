/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utilities to create arbitrary values of goog.html types for
 * testing purposes. These utility methods perform no validation, and the
 * resulting instances may violate type contracts.
 *
 * These methods are useful when types are constructed in a manner where using
 * the production API is too inconvenient. Please do use the production API
 * whenever possible; there is value in having tests reflect common usage and it
 * avoids, by design, non-contract complying instances from being created.
 */


goog.provide('goog.html.testing');
goog.setTestOnly();

goog.require('goog.html.SafeHtml');
goog.require('goog.html.SafeScript');
goog.require('goog.html.SafeStyle');
goog.require('goog.html.SafeStyleSheet');
goog.require('goog.html.SafeUrl');
goog.require('goog.html.TrustedResourceUrl');
goog.require('goog.testing.mockmatchers.ArgumentMatcher');


/**
 * Creates a SafeHtml wrapping the given value. No validation is performed.
 *
 * This function is for use in tests only and must never be used in production
 * code.
 *
 * @param {string} html The string to wrap into a SafeHtml.
 * @return {!goog.html.SafeHtml}
 */
goog.html.testing.newSafeHtmlForTest = function(html) {
  'use strict';
  return goog.html.SafeHtml.createSafeHtmlSecurityPrivateDoNotAccessOrElse(
      html);
};


/**
 * Creates a SafeScript wrapping the given value. No validation is performed.
 *
 * This function is for use in tests only and must never be used in production
 * code.
 *
 * @param {string} script The string to wrap into a SafeScript.
 * @return {!goog.html.SafeScript}
 */
goog.html.testing.newSafeScriptForTest = function(script) {
  'use strict';
  return goog.html.SafeScript.createSafeScriptSecurityPrivateDoNotAccessOrElse(
      script);
};


/**
 * Creates a SafeStyle wrapping the given value. No validation is performed.
 *
 * This function is for use in tests only and must never be used in production
 * code.
 *
 * @param {string} style String to wrap into a SafeStyle.
 * @return {!goog.html.SafeStyle}
 */
goog.html.testing.newSafeStyleForTest = function(style) {
  'use strict';
  return goog.html.SafeStyle.createSafeStyleSecurityPrivateDoNotAccessOrElse(
      style);
};


/**
 * Creates a SafeStyleSheet wrapping the given value. No validation is
 * performed.
 *
 * This function is for use in tests only and must never be used in production
 * code.
 *
 * @param {string} styleSheet String to wrap into a SafeStyleSheet.
 * @return {!goog.html.SafeStyleSheet}
 */
goog.html.testing.newSafeStyleSheetForTest = function(styleSheet) {
  'use strict';
  return goog.html.SafeStyleSheet
      .createSafeStyleSheetSecurityPrivateDoNotAccessOrElse(styleSheet);
};


/**
 * Creates a SafeUrl wrapping the given value. No validation is performed.
 *
 * This function is for use in tests only and must never be used in production
 * code.
 *
 * @param {string} url String to wrap into a SafeUrl.
 * @return {!goog.html.SafeUrl}
 */
goog.html.testing.newSafeUrlForTest = function(url) {
  'use strict';
  return goog.html.SafeUrl.createSafeUrlSecurityPrivateDoNotAccessOrElse(url);
};


/**
 * Creates a TrustedResourceUrl wrapping the given value. No validation is
 * performed.
 *
 * This function is for use in tests only and must never be used in production
 * code.
 *
 * @param {string} url String to wrap into a TrustedResourceUrl.
 * @return {!goog.html.TrustedResourceUrl}
 */
goog.html.testing.newTrustedResourceUrlForTest = function(url) {
  'use strict';
  return goog.html.TrustedResourceUrl
      .createTrustedResourceUrlSecurityPrivateDoNotAccessOrElse(url);
};


/**
 * Creates an argument matcher for SafeHtml.
 * @param {string|!goog.html.SafeHtml} expected
 * @return {!goog.testing.mockmatchers.ArgumentMatcher}
 */
goog.html.testing.matchSafeHtml = function(expected) {
  'use strict';
  if (expected instanceof goog.html.SafeHtml) {
    expected = goog.html.SafeHtml.unwrap(expected);
  }
  return new goog.testing.mockmatchers.ArgumentMatcher(function(actual) {
    'use strict';
    return goog.html.SafeHtml.unwrap(actual) == expected;
  });
};


/**
 * Creates an argument matcher for SafeScript.
 * @param {string|!goog.html.SafeScript} expected
 * @return {!goog.testing.mockmatchers.ArgumentMatcher}
 */
goog.html.testing.matchSafeScript = function(expected) {
  'use strict';
  if (expected instanceof goog.html.SafeScript) {
    expected = goog.html.SafeScript.unwrap(expected);
  }
  return new goog.testing.mockmatchers.ArgumentMatcher(function(actual) {
    'use strict';
    return goog.html.SafeScript.unwrap(actual) == expected;
  });
};


/**
 * Creates an argument matcher for SafeStyle.
 * @param {string|!goog.html.SafeStyle} expected
 * @return {!goog.testing.mockmatchers.ArgumentMatcher}
 */
goog.html.testing.matchSafeStyle = function(expected) {
  'use strict';
  if (expected instanceof goog.html.SafeStyle) {
    expected = goog.html.SafeStyle.unwrap(expected);
  }
  return new goog.testing.mockmatchers.ArgumentMatcher(function(actual) {
    'use strict';
    return goog.html.SafeStyle.unwrap(actual) == expected;
  });
};


/**
 * Creates an argument matcher for SafeStyleSheet.
 * @param {string|!goog.html.SafeStyleSheet} expected
 * @return {!goog.testing.mockmatchers.ArgumentMatcher}
 */
goog.html.testing.matchSafeStyleSheet = function(expected) {
  'use strict';
  if (expected instanceof goog.html.SafeStyleSheet) {
    expected = goog.html.SafeStyleSheet.unwrap(expected);
  }
  return new goog.testing.mockmatchers.ArgumentMatcher(function(actual) {
    'use strict';
    return goog.html.SafeStyleSheet.unwrap(actual) == expected;
  });
};


/**
 * Creates an argument matcher for SafeUrl.
 * @param {string|!goog.html.SafeUrl} expected
 * @return {!goog.testing.mockmatchers.ArgumentMatcher}
 */
goog.html.testing.matchSafeUrl = function(expected) {
  'use strict';
  if (expected instanceof goog.html.SafeUrl) {
    expected = goog.html.SafeUrl.unwrap(expected);
  }
  return new goog.testing.mockmatchers.ArgumentMatcher(function(actual) {
    'use strict';
    return goog.html.SafeUrl.unwrap(actual) == expected;
  });
};


/**
 * Creates an argument matcher for TrustedResourceUrl.
 * @param {string|!goog.html.TrustedResourceUrl} expected
 * @return {!goog.testing.mockmatchers.ArgumentMatcher}
 */
goog.html.testing.matchTrustedResourceUrl = function(expected) {
  'use strict';
  if (expected instanceof goog.html.TrustedResourceUrl) {
    expected = goog.html.TrustedResourceUrl.unwrap(expected);
  }
  return new goog.testing.mockmatchers.ArgumentMatcher(function(actual) {
    'use strict';
    return goog.html.TrustedResourceUrl.unwrap(actual) == expected;
  });
};


/**
 * Equality tester to be used in Jasmine tests. Example:
 *
 *     beforeEach(function() {
 *       jasmine.addCustomEqualityTester(
 *           goog.html.testing.checkTypedStringEquality);
 *     });
 *
 *     it('typed string value matches same string', function() {
 *       expect(f).toHaveBeenCalledWith('expected');
 *     });
 *
 *     it('typed string value matches same type and string', function() {
 *       expect(f).toHaveBeenCalledWith(goog.string.Const.from('expected'));
 *     });
 *
 * @param {*} actual Handles goog.string.TypedString.
 * @param {*} expected Handles goog.string.TypedString or string.
 * @return {boolean|undefined} Undefined if not called with
 *     goog.string.TypedString, true if typed strings equal, false if not.
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.html.testing.checkTypedStringEquality = function(actual, expected) {
  'use strict';
  if (actual && actual.implementsGoogStringTypedString) {
    if (expected != null && expected.implementsGoogStringTypedString) {
      if (!(actual instanceof expected.constructor)) {
        return false;
      }
      /**
       * @suppress {strictMissingProperties} Added to tighten compiler checks
       */
      expected = expected.getTypedStringValue();
    }
    return actual.getTypedStringValue() == expected;
  }
};
