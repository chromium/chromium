/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview testcases for CSS Sanitizer. */

goog.module('goog.html.CssSanitizerTest');
goog.setTestOnly();

const CssSanitizer = goog.require('goog.html.sanitizer.CssSanitizer');
const SafeStyle = goog.require('goog.html.SafeStyle');
const SafeStyleSheet = goog.require('goog.html.SafeStyleSheet');
const SafeUrl = goog.require('goog.html.SafeUrl');
const dom = goog.require('goog.testing.dom');
const googArray = goog.require('goog.array');
const googString = goog.require('goog.string');
const isVersion = goog.require('goog.userAgent.product.isVersion');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const userAgent = goog.require('goog.userAgent');

const isIE8 = userAgent.IE && !userAgent.isVersionOrHigher(9);

const isSafari9OrOlder = product.SAFARI && !isVersion(10);

const supportsDomParser = !userAgent.IE || userAgent.isVersionOrHigher(10);

/**
 * @param {string} cssText CSS text usually associated with an inline style.
 * @return {!CSSStyleDeclaration} A styleSheet object.
 */
function getStyleFromCssText(cssText) {
  const styleDecleration = document.createElement('div').style;
  styleDecleration.cssText = cssText || '';
  return styleDecleration;
}

/**
 * Asserts that the expected CSS text is equal to the actual CSS text.
 * @param {string} expectedCssText Expected CSS text.
 * @param {string} actualCssText Actual CSS text.
 */
function assertCSSTextEquals(expectedCssText, actualCssText) {
  if (isIE8) {
    // We get a bunch of default values set in IE8 because of the way we iterate
    // over the CSSStyleDecleration keys.
    // TODO(danesh): Fix IE8 or remove this hack. It will be problematic for
    // tests which have an extra semi-colon in the value (even if quoted).
    const actualCssArray = actualCssText.split(/\s*;\s*/);
    const ie8StyleString = 'WIDTH: 0px; BOTTOM: 0px; HEIGHT: 0px; TOP: 0px; ' +
        'RIGHT: 0px; TEXT-DECORATION: none underline overline line-through; ' +
        'LEFT: 0px; TEXT-DECORATION: underline line-through;';
    ie8StyleString.split(/\s*;\s*/).forEach(ie8Css => {
      googArray.remove(actualCssArray, ie8Css);
    });
    actualCssText = actualCssArray.join('; ');
  }
  assertEquals(
      getStyleFromCssText(expectedCssText).cssText,
      getStyleFromCssText(actualCssText).cssText);
}

/**
 * Gets sanitized inline style.
 * @param {string} sourceCss CSS to be sanitized.
 * @param {function (string, string):?SafeUrl=} urlRewrite URL rewriter that
 *     only returns a SafeUrl.
 * @return {string} Sanitized inline style.
 */
function getSanitizedInlineStyle(sourceCss, urlRewrite = undefined) {
  try {
    return SafeStyle.unwrap(CssSanitizer.sanitizeInlineStyle(
               getStyleFromCssText(sourceCss), urlRewrite)) ||
        '';
  } catch (err) {
    // IE8 doesn't like setting invalid properties. It throws an "Invalid
    // Argument" exception.
    if (!isIE8) {
      throw err;
    }
    return '';
  }
}

/**
 * Asserts that the input CSS text is equal to the expected CSS text after
 * sanitization using {@link CssSanitizer.sanitizeStyleSheetString}.
 * @param {string} expectedCssText
 * @param {string} inputCssText
 * @param {?string=} containerId
 * @param {function(string, string):?SafeUrl=} uriRewriter
 */
function assertSanitizedCssEquals(
    expectedCssText, inputCssText, containerId = undefined,
    uriRewriter = undefined) {
  assertBrowserSanitizedCssEquals(
      {chrome: expectedCssText}, inputCssText, containerId, uriRewriter);
}

/**
 * Asserts that on each browser the input CSS text is equal to the expected CSS
 * text after sanitization using {@link CssSanitizer.sanitizeStyleSheetString}.
 * Automatically verifies that on older browsers the sanitizer returns an empty
 * string.
 * @param {{
 *     chrome: string,
 *     firefox: (string|undefined),
 *     safari: (string|undefined),
 *     newIE: (string|undefined),
 *     ie10: (string|undefined)}} expectedCssTextByBrowser An object that maps
 * each browser to a different expected value. All browsers but chrome are
 * optional. If a browser is missing, the chrome expected value is used instead.
 * @param {string} inputCssText
 * @param {?string=} containerId
 * @param {function(string, string):?SafeUrl=} uriRewriter
 */
function assertBrowserSanitizedCssEquals(
    expectedCssTextByBrowser, inputCssText, containerId = undefined,
    uriRewriter = undefined) {
  let expectedCssText = undefined;
  if (product.CHROME) {
    expectedCssText = expectedCssTextByBrowser.chrome;
  } else if (product.FIREFOX) {
    expectedCssText = expectedCssTextByBrowser.firefox;
  } else if (product.SAFARI) {
    expectedCssText = expectedCssTextByBrowser.safari;
  } else if (product.IE && document.documentMode == 10) {
    expectedCssText = expectedCssTextByBrowser.ie10;
    console.log('ie10');
  } else if (product.IE && !userAgent.isVersionOrHigher(10)) {
    // Don't even try with chrome as default for IE8 and IE9.
    expectedCssText = '';
  } else if (product.IE || product.EDGE) {
    expectedCssText = expectedCssTextByBrowser.newIE;
  } else {
    throw new Error('Unrecognized browser, this function needs to be updated.');
  }
  if (expectedCssText == undefined) {
    expectedCssText = expectedCssTextByBrowser.chrome;
  }
  assertEquals(
      expectedCssText,
      SafeStyleSheet.unwrap(CssSanitizer.sanitizeStyleSheetString(
          inputCssText, containerId === undefined ? 'foo' : containerId,
          uriRewriter)));
}

/**
 * Converts rules in STYLE tags into style attributes on the tags they apply to,
 * and returns a new string.
 * @param {string} html
 * @return {string}
 */
function inlineStyleRulesString(html) {
  const tree =
      CssSanitizer.safeParseHtmlAndGetInertElement(`<span>${html}</span>`);
  if (tree == null) {
    return '';
  }
  CssSanitizer.inlineStyleRules(tree);
  return tree.innerHTML;
}

/**
 * Inlines the rules in STYLE tags in the original HTML and asserts that it is
 * the same as the expected HTML.
 * @param {string} expectedHtml
 * @param {string} originalHtml
 */
function assertInlinedStyles(expectedHtml, originalHtml) {
  const inlinedHtml = inlineStyleRulesString(originalHtml);
  if (!supportsDomParser) {
    assertEquals('', inlinedHtml);
    return;
  }
  dom.assertHtmlMatches(
      expectedHtml, inlinedHtml, true /* opt_strictAttributes */);
}

/**
 * @param {string} expectedCssText
 * @param {string} inputCssText
 * @param {function(string, string):?SafeUrl=} uriRewriter A URI rewriter that
 *     returns a SafeUrl.
 */
function assertInlineStyleStringEquals(
    expectedCssText, inputCssText, uriRewriter = undefined) {
  if (userAgent.IE && document.documentMode < 10) {
    expectedCssText = '';
  }

  const safeStyle =
      CssSanitizer.sanitizeInlineStyleString(inputCssText, uriRewriter);
  const output = SafeStyle.unwrap(safeStyle);
  assertCSSTextEquals(expectedCssText, output);
}

testSuite({
  testValidCss() {
    let actualCSS = 'font-family: inherit';
    let expectedCSS = 'font-family: inherit';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));

    // .1 -> 0.1; 1.0 -> 1
    actualCSS = 'padding: 1pt .1pt 1pt 1.0em';
    expectedCSS = 'padding: 1pt 0.1pt 1pt 1em';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));

    // Negative margins are allowed.
    actualCSS = 'margin:    -7px -.5px -23px -1.25px';
    expectedCSS = 'margin: -7px -0.5px -23px -1.25px';
    if (isIE8) {
      // IE8 doesn't like sub-pixels
      // https://blogs.msdn.microsoft.com/ie/2010/11/03/sub-pixel-fonts-in-ie9/
      expectedCSS = expectedCSS.replace('-0.5px', '0px');
      expectedCSS = expectedCSS.replace('-1.25px', '-1px');
    }
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));

    actualCSS = 'quotes: "{" "}" "<" ">"';
    expectedCSS = 'quotes: "{" "}" "<" ">";';
    if (isSafari9OrOlder) {
      // We never figured out why Safari didn't work here, but it's obsolete
      // now.
      expectedCSS = 'quotes: \'{\';';
    }
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));
  },

  testInvalidCssRemoved() {
    let actualCSS;

    // Tests all have null results.
    const expectedCSS = '';

    actualCSS = 'font: Arial Black,monospace,Helvetica,#88ff88';
    // Hash values are not allowed so are dropped.
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));

    // Negative numbers for border not allowed.
    actualCSS = 'border : -7px -0.5px -23px -1.25px';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));

    // Negative numbers converted to empty.
    actualCSS = 'padding: -0 -.0 -0. -0.0 ';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));

    // Invalid values not allowed.
    actualCSS = 'padding : #123 - 5 "5"';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));

    // Font-family does not allow quantities at all.
    actualCSS = 'font-family: 7 .5 23 1.25 -7 -.5 -23 -1.25 +7 +.5 +23 +1.25 ' +
        '7cm .5em 23.mm 1.25px -7cm -.5em -23.mm -1.25px ' +
        '+7cm +.5em +23.mm +1.25px 0 .0 -0+00.0 /';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));

    actualCSS = 'background: bogus url("foo.png") transparent';
    assertCSSTextEquals(
        expectedCSS, getSanitizedInlineStyle(actualCSS, SafeUrl.sanitize));

    // expression(...) is not allowed for font so is rejected wholesale -- the
    // internal string "pwned" is not passed through.
    actualCSS =
        'font-family: Arial Black,monospace,expression(return "pwned"),' +
        'Helvetica,#88ff88';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));
  },

  testCssBackground() {
    let actualCSS;
    let expectedCSS;

    function proxyUrl(url) {
      return testing.newSafeUrlForTest(`https://goo.gl/proxy?url=${url}`);
    }

    // Don't require the URL sanitizer to protect string boundaries.
    actualCSS = 'background-image: url("javascript:evil(1337)")';
    expectedCSS = '';
    assertCSSTextEquals(
        expectedCSS, getSanitizedInlineStyle(actualCSS, SafeUrl.sanitize));

    actualCSS = 'background-image: url("http://goo.gl/foo.png")';
    expectedCSS =
        'background-image: url(https://goo.gl/proxy?url=http://goo.gl/foo.png)';
    assertCSSTextEquals(
        expectedCSS, getSanitizedInlineStyle(actualCSS, proxyUrl));

    // Without any URL sanitizer.
    actualCSS = 'background: transparent url("Bar.png")';
    const sanitizedCss = getSanitizedInlineStyle(actualCSS);
    assertFalse(googString.contains(sanitizedCss, 'background-image'));
    assertFalse(googString.contains(sanitizedCss, 'Bar.png'));
  },

  testVendorPrefixed() {
    const actualCSS = '-webkit-text-stroke: 1px red';
    const expectedCSS = '';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));
  },

  testDisallowedFunction() {
    const actualCSS = 'border-width: calc(10px + 20px)';
    const expectedCSS = '';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));
  },

  testColor() {
    const colors = [
      'red',
      'Red',
      'RED',
      'Gray',
      'grey',
      '#abc',
      '#123',
      '#ABC123',
      'rgb( 127, 64 , 255 )',
    ];
    const notcolors = [
      // Finding words that are not X11 colors is harder than you think.
      'killitwithfire', 'invisible', 'expression(red=blue)', '#aa-1bb',
      '#expression', '#doevil'
      // 'rgb(0, 0, 100%)' // Invalid in all browsers
      // 'rgba(128,255,128,50%)', // Invalid in all browsers
    ];

    for (let i = 0; i < colors.length; ++i) {
      const validColorValue = 'color: ' + colors[i];
      assertCSSTextEquals(
          validColorValue, getSanitizedInlineStyle(validColorValue));
    }

    for (let i = 0; i < notcolors.length; ++i) {
      const invalidColorValue = 'color: ' + notcolors[i];
      assertCSSTextEquals('', getSanitizedInlineStyle(invalidColorValue));
    }
  },

  testCustomVariablesSanitized() {
    const actualCSS = '\\2d-leak: leakTest; background: var(--leak);';
    assertCSSTextEquals('', getSanitizedInlineStyle(actualCSS));
  },

  testExpressionsPreserved() {
    if (isIE8) {
      // Disable this test as IE8 doesn't support expressions.
      // https://msdn.microsoft.com/en-us/library/ms537634(v=VS.85).aspx
      return;
    }

    let actualCSS;
    let expectedCSS;

    actualCSS = 'background-image: linear-gradient(to bottom right, red, blue)';
    expectedCSS =
        'background-image: linear-gradient(to right bottom, red, blue)';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));
  },

  testMultipleInlineStyles() {
    const actualCSS = 'margin: 1px ; padding: 0';
    const expectedCSS = 'margin: 1px; padding: 0px;';
    assertCSSTextEquals(expectedCSS, getSanitizedInlineStyle(actualCSS));
  },

  testSanitizeInlineStyleString_basic() {
    assertInlineStyleStringEquals('', '');
    assertInlineStyleStringEquals('color: red;', 'color: red');
    assertInlineStyleStringEquals(
        'color: green; padding: 10px;', 'color: green; padding: 10px');
  },

  testSanitizeInlineStyleString_malicious() {
    assertInlineStyleStringEquals('', 'color: expression("pwned")');
  },

  testSanitizeInlineStyleString_url() {
    assertInlineStyleStringEquals(
        '', 'background-image: url("http://example.com")');

    assertInlineStyleStringEquals(
        '', 'background-image: url("http://example.com")', (uri) => null);
    assertInlineStyleStringEquals(
        'background-image: url("http://example.com");',
        'background-image: url("http://example.com")', SafeUrl.sanitize);
  },

  testSanitizeInlineStyleString_unbalancedParenthesesInUnquotedUrl() {
    assertEquals(
        '',
        SafeStyle.unwrap(CssSanitizer.sanitizeInlineStyleString(
            'background-image: url(http://example.com/aaa(a)',
            SafeUrl.sanitize)));
    assertEquals(
        '',
        SafeStyle.unwrap(CssSanitizer.sanitizeInlineStyleString(
            'background-image: url(http://example.com/aaa)a)',
            SafeUrl.sanitize)));
  },

  testSanitizeInlineStyleString_preservesCase() {
    assertInlineStyleStringEquals(
        'font-family: Roboto, sans-serif', 'font-family: Roboto, sans-serif');
  },

  testSanitizeInlineStyleString_simpleFunctions() {
    let expectedCss = 'color: rgb(1,2,3);';
    assertInlineStyleStringEquals(expectedCss, expectedCss);
    expectedCss = 'background-image: linear-gradient(red, blue);';
    assertInlineStyleStringEquals(expectedCss, expectedCss);
  },

  testSanitizeInlineStyleString_nestedFunction() {
    const expectedCss =
        'background-image: linear-gradient(217deg, rgba(255,0,0,.8), blue);';
    assertInlineStyleStringEquals(expectedCss, expectedCss);
  },

  testSanitizeInlineStyleString_repeatingLinearGradient() {
    const expectedCss = 'background-image: repeating-linear-gradient(' +
        '-45deg, rgb(66, 133, 244), rgb(66, 133, 244) 4px, ' +
        'rgb(255, 255, 255) 4px, rgb(255, 255, 255) 5px, ' +
        'rgb(66, 133, 244) 5px, rgb(66, 133, 244) 8px);';
    assertInlineStyleStringEquals(expectedCss, expectedCss);
  },

  testSanitizeInlineStyleString_noUrlPropertyValueFanOut() {
    if (userAgent.IE && document.documentMode < 10) {
      return;
    }
    // Property fanout is ok, but value fanout isn't, because it would lead to
    // CssPropertySanitizer dropping the value. Check that no browser does
    // value fanout.
    const safeStyle = CssSanitizer.sanitizeInlineStyleString(
        'background: url("http://foo.com/a")', SafeUrl.sanitize);
    const output = SafeStyle.unwrap(safeStyle);
    // We can't use assertInlineStyleStringEquals, the browser is inconsistent
    // about fanout of properties. We'll use plain substring matching instead.
    assertContains('url("http://foo.com/a")', output);
  },

  /** @suppress {accessControls} */
  testInertDocument() {
    if (!document.implementation.createHTMLDocument) {
      return;  // skip test
    }

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    window.xssFiredInertDocument = false;
    const doc = CssSanitizer.createInertDocument_();
    try {
      doc.write('<script> window.xssFiredInertDocument = true; </script>');
    } catch (e) {
      // ignore
    }
    assertFalse(window.xssFiredInertDocument);
  },

  /** @suppress {accessControls} */
  testInertCustomElements() {
    if (typeof HTMLTemplateElement != 'function' || !document.registerElement) {
      return;  // skip test
    }

    const inertDoc = CssSanitizer.createInertDocument_();
    const xFooConstructor = document.registerElement('x-foo');
    const xFooElem =
        document.implementation.createHTMLDocument('').createElement('x-foo');
    assertTrue(xFooElem instanceof xFooConstructor);  // sanity check

    const inertXFooElem = inertDoc.createElement('x-foo');
    assertFalse(inertXFooElem instanceof xFooConstructor);
  },

  testSanitizeStyleSheetString_basic() {
    let input = '';
    assertSanitizedCssEquals(input, input);

    input = 'a {color: red}';
    let expected = '#foo a{color: red;}';
    assertSanitizedCssEquals(expected, input);

    input = 'a {color: red} b {color:red; not-allowed: 1; ' +
        'background-image: url(\'http://not.allowed\');}';
    expected = '#foo a{color: red;}#foo b{color: red;}';
    assertSanitizedCssEquals(expected, input);
  },

  testSanitizeInlineStyleString_noSelector() {
    const input = 'a{color: red;}';
    assertSanitizedCssEquals(input, input, null /* opt_containerId */);
  },

  testSanitizeStyleSheetString_comma() {
    const input = 'a, b, c > d {color: red}';
    const expected = '#foo a,#foo b,#foo c > d{color: red;}';
    assertSanitizedCssEquals(expected, input);
  },

  testSanitizeStyleSheetString_atRule() {
    const input = '@media screen { a { color: red; } }';
    const expected = '';
    assertSanitizedCssEquals(expected, input);
  },

  testSanitizeStyleSheetString_borderSpacing() {
    const input = 'table { border-spacing: 0px; }';
    const expected = '#foo table{border-spacing: 0px;}';
    assertSanitizedCssEquals(expected, input);
  },

  testSanitizeStyleSheetString_urlRewrite() {
    const urlRewriter = (url) => {
      if (input.indexOf('bar') > -1) {
        return SafeUrl.sanitize(url);
      } else {
        return null;
      }
    };

    let input = 'a {background-image: url("http://bar.com")}';
    const quoted = '#foo a{background-image: url("http://bar.com");}';
    // Safari will add a slash.
    const slash = '#foo a{background-image: url("http://bar.com/");}';
    assertBrowserSanitizedCssEquals(
        {safari: slash, chrome: quoted}, input, undefined /* opt_containerId */,
        urlRewriter);

    input = 'a {background-image: url("http://nope.com")}';
    let expected = '#foo a{}';
    assertSanitizedCssEquals(
        expected, input, undefined /* opt_containerId */, urlRewriter);

    input = 'a{background-image: url("javascript:alert(\"bar\")")}';
    expected = '#foo a{}';
    assertSanitizedCssEquals(
        expected, input, undefined /* opt_containerId */, urlRewriter);
  },

  testSanitizeStyleSheetString_unrecognized() {
    const input = 'a {mso-font-size: 2px; color: black}';
    const expected = 'a{color: black;}';
    assertSanitizedCssEquals(expected, input, null /* opt_containerId */);
  },

  testSanitizeStyleSheetString_malformed() {
    let input = '<script>alert(1)</script>';
    let expected = '';
    assertSanitizedCssEquals(expected, input);

    input = 'a { } </style><script>alert(1)</script><style>';
    expected = '#foo a{}';
    assertSanitizedCssEquals(expected, input);

    input = 'a < b { } a { font-size: 10px }';
    expected = '#foo a{font-size: 10px;}';
    assertSanitizedCssEquals(expected, input);

    input =
        'a {;;;} a { font-size: 10px } ;;; a { background-image: url(() }};;';
    expected = '#foo a{}#foo a{font-size: 10px;}';
    assertSanitizedCssEquals(expected, input);

    input = 'a[a=\"ccc] { color: red;}';
    expected = '';
    assertSanitizedCssEquals(expected, input);
  },

  testSanitizeStyleSheetString_stringWithNoEscapedQuotesInSelector() {
    let input = 'a[data-foo="foo,bar"], b { color: red }';
    // IE converts the string to single quotes.
    let doubleQuoted = '#foo a[data-foo="foo,bar"],#foo b{color: red;}';
    let singleQuoted = '#foo a[data-foo=\'foo,bar\'],#foo b{color: red;}';
    assertBrowserSanitizedCssEquals(
        {chrome: doubleQuoted, newIE: singleQuoted, ie10: singleQuoted}, input);

    input = 'a[data-foo=\'foo,bar\'], b { color: red }';
    // Chrome converts the string to double quotes.
    doubleQuoted = '#foo a[data-foo="foo,bar"],#foo b{color: red;}';
    singleQuoted = '#foo a[data-foo=\'foo,bar\'],#foo b{color: red;}';
    assertBrowserSanitizedCssEquals(
        {chrome: doubleQuoted, newIE: singleQuoted, ie10: singleQuoted}, input);

    input = 'a[foo="foo,bar"][bar="baz"], b { color: blue }';
    doubleQuoted = '#foo a[foo="foo,bar"][bar="baz"],#foo b{color: blue;}';
    singleQuoted = '#foo a[foo=\'foo,bar\'][bar=\'baz\'],#foo b{color: blue;}';
    assertBrowserSanitizedCssEquals(
        {chrome: doubleQuoted, newIE: singleQuoted, ie10: singleQuoted}, input);

    input = 'a[foo="foo,bar"], b, c[foo="f,b"][bar="f,b"] { color: red }';
    doubleQuoted = '#foo a[foo="foo,bar"],#foo b,#foo c[foo="f,b"][bar="f,b"]' +
        '{color: red;}';
    singleQuoted =
        '#foo a[foo=\'foo,bar\'],#foo b,#foo c[bar=\'f,b\'][foo=\'f,b\']' +
        '{color: red;}';
    assertBrowserSanitizedCssEquals(
        {chrome: doubleQuoted, newIE: singleQuoted, ie10: singleQuoted}, input);
  },

  testSanitizeStyleSheetString_stringWithEscapedQuotesInSelector() {
    // Contains an escaped string, but the selector is converted to a[a='a"b']
    // before the regex is executed.
    let input = 'a[a="a\\"b"] { color: black; }';
    let doubleQuoted = '#foo a[a="a\\"b"]{color: black;}';
    let singleQuoted = '#foo a[a=\'a"b\']{color: black;}';
    assertBrowserSanitizedCssEquals(
        {chrome: doubleQuoted, ie10: singleQuoted, newIE: singleQuoted}, input);

    input = 'a[a="a\\\'b"] { color: grey; }';
    doubleQuoted = '#foo a[a="a\'b"]{color: grey;}';
    singleQuoted = '#foo a[a=\'a\\\'b\']{color: grey;}';
    assertBrowserSanitizedCssEquals(
        {
          chrome: doubleQuoted,
          firefox: doubleQuoted,
          ie10: '',
          newIE: singleQuoted,
        },
        input);

    input = 'a[a=\'\\\'b\'] {color: red; }';
    doubleQuoted = '#foo a[a="\'b"]{color: red;}';
    singleQuoted = '#foo a[a=\'\\\'b\']{color: red;}';
    assertBrowserSanitizedCssEquals(
        {
          chrome: doubleQuoted,
          firefox: doubleQuoted,
          newIE: singleQuoted,
          ie10: '',
        },
        input);

    input = 'a[foo=\'b\\\'a, ,\'] { color: blue; }';
    doubleQuoted = '#foo a[foo="b\'a, ,"]{color: blue;}';
    singleQuoted = '#foo a[foo=\'b\\\'a, ,\']{color: blue;}';
    assertBrowserSanitizedCssEquals(
        {
          chrome: doubleQuoted,
          firefox: doubleQuoted,
          newIE: singleQuoted,
          ie10: '',
        },
        input);

    input = 'a[a=\'a\\"b\'] { color: black; }';
    doubleQuoted = '#foo a[a="a\\"b"]{color: black;}';
    singleQuoted = '#foo a[a=\'a"b\']{color: black;}';
    assertBrowserSanitizedCssEquals(
        {chrome: doubleQuoted, ie10: singleQuoted, newIE: singleQuoted}, input);
  },

  testSanitizeInlineStyleString_invalidSelector() {
    const input = 'a{}';
    if (supportsDomParser) {
      assertThrows(() => {
        CssSanitizer.sanitizeStyleSheetString(input, '</style>');
      });
    }
  },

  testInlineStyleRules_empty() {
    assertInlinedStyles('', '');
  },

  testInlineStyleRules_basic() {
    const input = '<style>a{color:red}</style><a>foo</a>';
    const expected = '<a style="color:red;">foo</a>';
    assertInlinedStyles(expected, input);
  },

  testInlineStyleRules_onlyStyle() {
    const input = '<style>a{color:red}</style>';
    assertInlinedStyles('', input);
  },

  testInlineStyleRules_noStyle() {
    const input = '<a>hi</a>';
    assertInlinedStyles(input, input);
  },

  testInlineStyleRules_onlyText() {
    const input = 'hello';
    assertInlinedStyles(input, input);
  },

  testInlineStyleRules_specificity() {
    const input = '<style>a{color: red; border: 1px}' +
        '#foo{color: white; border: 2px}</style>' +
        '<a id="foo" style="color: black">foo</a>';
    const expected = '<a id="foo" style="color: black; border: 2px">foo</a>';
    assertInlinedStyles(expected, input);
  },

  testInlineStyleRules_media() {
    const input =
        '<style>a{color: red;} @media screen { border: 1px; }</style>' +
        '<a id="foo">foo</a>';
    const expected = '<a id="foo" style="color: red;">foo</a>';
    assertInlinedStyles(expected, input);
  },

  testInlineStyleRules_background() {
    const input = '<style>a{background: none;}</style><a id="foo">foo</a>';
    const expected = product.SAFARI ?
        // Safari will expand multi-value properties such as background, border,
        // etc into multiple properties. The result is more verbose but it
        // should not affect the effective style.
        ('<a id="foo" style="background-image: none; ' +
         'background-position: initial initial; ' +
         'background-repeat: initial initial;">foo</a>') :
        '<a id="foo" style="background: none;">foo</a>';
    assertInlinedStyles(expected, input);
  },
});
