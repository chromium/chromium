/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Tests for {@link goog.html.sanitizer.CssPropertySanitizer} */

goog.module('goog.html.sanitizer.CssPropertySanitizerTest');
goog.setTestOnly();

const CssPropertySanitizer = goog.require('goog.html.sanitizer.CssPropertySanitizer');
const SafeUrl = goog.require('goog.html.SafeUrl');
const googFunctions = goog.require('goog.functions');
const noclobber = goog.require('goog.html.sanitizer.noclobber');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');


const NAME = 'foo';

/**
 * @param {string} name
 * @param {string} value
 * @return {!CSSStyleDeclaration}
 * @suppress {checkTypes} suppression added to enable type checking
 */
function getProcessedPropertyValue(name, value) {
  const div = document.createElement('div');
  div.innerHTML =
      '<div style="' + name + ': ' + value.replace(/"/g, '&quot;') + '"></div>';
  return noclobber.getCssPropertyValue(div.children[0].style, name);
}

// This suite contains several tests that test/document assumptions on how CSS
// property values are filtered/sanitized by the browser before they are passed
// to our property value sanitizer. If these tests start failing, it might mean
// that one of our assumptions has been broken by a new browser version, which
// means that the code might now contain a vulnerability, or some checks have
// been made redundant.

testSuite({
  testBrowserBehavior_mixedCase() {
    assertEquals('red', getProcessedPropertyValue('coLOr', 'rEd'));
    assertEquals(
        'url("http://foo.com/a")',
        getProcessedPropertyValue(
            'background-iMAge', 'uRL("http://foo.com/a")'));
  },

  testBrowserBehavior_spaces() {
    assertEquals(
        '', getProcessedPropertyValue('background-color', 'rgba (1,2,3,0)'));
    assertEquals(
        'rgba(1, 2, 3, 0)',
        getProcessedPropertyValue('background-color', 'rgba(1,2,3,0)'));
    assertEquals(
        '',
        getProcessedPropertyValue(
            'background-image', 'url ( "http://foo.com/a" )'));
  },

  testBrowserBehavior_noComment() {
    // Verify that we don't have to worry about comments in post-processed
    // values that need to be sanitized.
    assertEquals(
        'red', getProcessedPropertyValue('color', ' /* a */ red /* b */'));
    assertEquals(
        'url("http://foo.com/u")',
        getProcessedPropertyValue(
            'background-image', ' /* a */ url("http://foo.com/u")'));
  },

  testBrowserBehavior_noParanthesesInUnquotedUrl() {
    // Verify how browsers deal with extra parentheses in unquoted urls.
    assertEquals(
        '',
        getProcessedPropertyValue(
            'background-image', 'url(http://foo.com/a)b)'));
    // Parentheses can be escaped.
    assertEquals(
        'url("http://foo.com/a)b")',
        getProcessedPropertyValue(
            'background-image', 'url(http://foo.com/a\\)b)'));
    assertEquals(
        '',
        getProcessedPropertyValue(
            'background-image', 'url(http://foo.com/a(b)c)'));
  },

  testBrowserBehavior_escapedQuotes() {
    // Verify how browsers deal with escaped quotes inside strings.
    let expectedValue;
    if (product.CHROME || product.FIREFOX) {
      expectedValue = 'url("http://foo.com/a\\")")';
    } else if (product.SAFARI) {
      // It interprets \" as ", urlescapes it and closes the url() call for us
      // at the end of input.
      expectedValue = 'url("http://foo.com/a%22)")';
    } else if (product.IE || product.EDGE) {
      // Same for IE, except that no urlescaping happens, so even the output
      // value is malformed.
      expectedValue = 'url("http://foo.com/a")")';
    }
    assertEquals(
        expectedValue,
        getProcessedPropertyValue(
            'background-image', 'url("http://foo.com/a\\")'));
    if (product.CHROME || product.FIREFOX) {
      expectedValue = 'url("http://foo.com/a\\"b)")';
    } else if (product.SAFARI) {
      expectedValue = 'url("http://foo.com/a%22b)")';
    } else if (product.IE || product.EDGE) {
      expectedValue = 'url("http://foo.com/a"b)")';
    }
    assertEquals(
        expectedValue,
        getProcessedPropertyValue(
            'background-image', 'url("http://foo.com/a\\"b)'));
  },

  testBrowserBehavior_escapedQuotesMultiple() {
    // Same as above, but check what happens if there are other values after the
    // string-based one.
    let expectedValue;
    if (product.CHROME || product.FIREFOX) {
      expectedValue = 'url("http://foo.com/a\\"), rgba(1,1,1,0)")';
    } else if (product.SAFARI) {
      // Safari and IE/EDGE do the same thing as above, it's more obvious from
      // this input.
      expectedValue = 'url("http://foo.com/a%22),%20rgba(1,1,1,0)")';
    } else if (product.IE || product.EDGE) {
      expectedValue = 'url("http://foo.com/a"), rgba(1,1,1,0)")';
    }
    assertEquals(
        expectedValue,
        getProcessedPropertyValue(
            'background-image', 'url("http://foo.com/a\\"), rgba(1,1,1,0)'));
    assertEquals(
        '',
        getProcessedPropertyValue(
            'background-image',
            'url("http://foo.com/a\\"), url("http://foo.com/b\\"), rgba(1,1,1,0)'));
  },

  testBrowserBehavior_urlArguments() {
    // Check that any trailing values or arguments in url are removed.
    assertEquals(
        '',
        getProcessedPropertyValue(
            'background-image', 'url("http://foo.com" abc)'));
    assertEquals(
        '',
        getProcessedPropertyValue(
            'background-image', 'url("http://foo.com", abc)'));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testBrowserBehavior_relative() {
    // Safari is the only browser that resolves relative URLs.
    assertTrue(
        getProcessedPropertyValue('background-image', 'url(/foo.com/a.jpg)')
            .startsWith(product.SAFARI ? 'url("http://' : 'url("/foo.com'));
    assertTrue(getProcessedPropertyValue('background-image', 'url(a.jpg)')
                   .startsWith(product.SAFARI ? 'url("http://' : 'url("a.jpg'));
  },

  testSanitizeProperty_basic() {
    assertEquals('1px', CssPropertySanitizer.sanitizeProperty(NAME, '1px'));
    assertEquals(
        '1px 2px', CssPropertySanitizer.sanitizeProperty(NAME, '1px 2px'));
    assertEquals('auto', CssPropertySanitizer.sanitizeProperty(NAME, 'auto'));
  },

  testSanitizeProperty_oneFunction() {
    assertEquals(
        null, CssPropertySanitizer.sanitizeProperty(NAME, 'foo(1,2,3,4)'));
    assertEquals(
        'rgba(1,2,3,4)',
        CssPropertySanitizer.sanitizeProperty(NAME, 'rgba(1,2,3,4)'));
  },

  testSanitizeProperty_nestedFunctions() {
    assertEquals(
        null,
        CssPropertySanitizer.sanitizeProperty(
            NAME, 'linear-gradient(1,foo(2),3,4)'));
    assertEquals(
        'linear-gradient(red, rgba(1,2,3,4))',
        CssPropertySanitizer.sanitizeProperty(
            NAME, 'linear-gradient(red, rgba(1,2,3,4))'));
  },

  testSanitizeProperty_noStrings() {
    assertEquals(
        null, CssPropertySanitizer.sanitizeProperty(NAME, 'rgba("1",2,3,4)'));
    assertEquals(
        null,
        CssPropertySanitizer.sanitizeProperty(
            NAME, 'linear-gradient("a", rgba(1,2,3,4)'));
  },

  testSanitizeProperty_complexNested() {
    const expectedValue = 'background-image: repeating-linear-gradient(' +
        '-45deg, rgb(66, 133, 244), rgb(66, 133, 244) 4px, ' +
        'rgb(255, 255, 255) 4px, rgb(255, 255, 255) 5px, ' +
        'rgb(66, 133, 244) 5px, rgb(66, 133, 244) 8px);';
    assertEquals(
        expectedValue,
        CssPropertySanitizer.sanitizeProperty(NAME, expectedValue));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSanitizeProperty_url() {
    const url = 'url("http://foo.com")';
    assertEquals(null, CssPropertySanitizer.sanitizeProperty(NAME, url));
    assertEquals(
        null,
        CssPropertySanitizer.sanitizeProperty(NAME, url, googFunctions.FALSE));
    assertEquals(
        url,
        CssPropertySanitizer.sanitizeProperty(NAME, url, SafeUrl.sanitize));
    assertEquals(
        'url("https://foo.com")',
        CssPropertySanitizer.sanitizeProperty(NAME, url, function(url) {
          return SafeUrl.sanitize(url.replace('http', 'https'));
        }));
  },

  testSanitizeProperty_urlAndOther() {
    assertEquals(
        null,
        CssPropertySanitizer.sanitizeProperty(
            NAME, 'url("http://foo.com"), rgba(1,1,1,0)', SafeUrl.sanitize));
    assertEquals(
        null,
        CssPropertySanitizer.sanitizeProperty(
            NAME, 'rgba(1,1,1,0), url("http://foo.com")', SafeUrl.sanitize));
    assertEquals(
        null,
        CssPropertySanitizer.sanitizeProperty(
            NAME, 'url(http://foo.com), rgba(1,1,1,0)', SafeUrl.sanitize));
    assertEquals(
        null,
        CssPropertySanitizer.sanitizeProperty(
            NAME, 'rgba(1,1,1,0), url(http://foo.com)', SafeUrl.sanitize));
  },

  testSanitizeProperty_mixedCaseFunction() {
    const lowerCaseValue = 'translatex(10px)';
    assertEquals(
        lowerCaseValue,
        CssPropertySanitizer.sanitizeProperty(NAME, lowerCaseValue));
    const mixedCaseValue = 'translateX(10px)';
    assertEquals(
        mixedCaseValue,
        CssPropertySanitizer.sanitizeProperty(NAME, mixedCaseValue));
  }
});
