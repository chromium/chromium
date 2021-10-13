/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.cssomTest');
goog.setTestOnly();

const CssRuleType = goog.require('goog.cssom.CssRuleType');
const cssom = goog.require('goog.cssom');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

// Since sheet cssom_test1.css's first line is to import
// cssom_test2.css, we should get 2 before one in the string.
let cssText = '.css-link-1 { display: block; } ' +
    '.css-import-2 { display: block; } ' +
    '.css-import-1 { display: block; } ' +
    '.css-style-1 { display: block; } ' +
    '.css-style-2 { display: block; } ' +
    '.css-style-3 { display: block; }';

const replacementCssText = '.css-repl-1 { display: block; }';

// We're going to toLowerCase cssText before testing, because IE returns
// CSS property names in UPPERCASE, and the function shouldn't
// "fix" the text as it would be expensive and rarely of use.
// Same goes for the trailing whitespace in IE.
// Same goes for fixing the optimized removal of trailing ; in rules.
// Also needed for Opera.
function fixCssTextForIe(cssText) {
  cssText = cssText.toLowerCase().replace(/\s*$/, '');
  if (cssText.match(/[^;] \}/)) {
    cssText = cssText.replace(/([^;]) \}/g, '$1; }');
  }
  return cssText;
}

// Tests the scenario where we have a known stylesheet and index.

testSuite({
  testGetFileNameFromStyleSheet() {
    // cast to create mock object.
    let styleSheet =
        /** @type {?} */ ({'href': 'http://foo.com/something/filename.css'});
    assertEquals('filename.css', cssom.getFileNameFromStyleSheet(styleSheet));

    styleSheet = /** @type {?} */ (
        {'href': 'https://foo.com:123/something/filename.css'});
    assertEquals('filename.css', cssom.getFileNameFromStyleSheet(styleSheet));

    styleSheet = /** @type {?} */ (
        {'href': 'http://foo.com/something/filename.css?bar=bas'});
    assertEquals('filename.css', cssom.getFileNameFromStyleSheet(styleSheet));

    styleSheet = /** @type {?} */ ({'href': 'filename.css?bar=bas'});
    assertEquals('filename.css', cssom.getFileNameFromStyleSheet(styleSheet));

    styleSheet = /** @type {?} */ ({'href': 'filename.css'});
    assertEquals('filename.css', cssom.getFileNameFromStyleSheet(styleSheet));
  },

  testGetAllCssStyleSheets() {
    // NOTE: getAllCssStyleSheets return type is wrong, it should be
    // !Array<!Stylesheet> rather than nullable array entries
    const styleSheets = /** @type {?} */ (cssom.getAllCssStyleSheets());
    assertEquals(4, styleSheets.length);
    // Makes sure they're in the right cascade order.
    assertEquals(
        'cssom_test_link_1.css',
        cssom.getFileNameFromStyleSheet(styleSheets[0]));
    assertEquals(
        'cssom_test_import_2.css',
        cssom.getFileNameFromStyleSheet(styleSheets[1]));
    assertEquals(
        'cssom_test_import_1.css',
        cssom.getFileNameFromStyleSheet(styleSheets[2]));
    // Not an external styleSheet
    assertNull(cssom.getFileNameFromStyleSheet(styleSheets[3]));
  },

  testGetAllCssText() {
    const allCssText = cssom.getAllCssText();
    assertEquals(cssText, fixCssTextForIe(allCssText));
  },

  testGetAllCssStyleRules() {
    const allCssRules = cssom.getAllCssStyleRules();
    assertEquals(6, allCssRules.length);
  },

  testAddCssText() {
    const newCssText = '.css-add-1 { display: block; }';
    const newCssNode = cssom.addCssText(newCssText);

    assertEquals(document.styleSheets.length, 3);

    const allCssText = cssom.getAllCssText();

    assertEquals(`${cssText} ${newCssText}`, fixCssTextForIe(allCssText));

    let cssRules = cssom.getAllCssStyleRules();
    assertEquals(7, cssRules.length);

    // Remove the new stylesheet now so it doesn't interfere with other
    // tests.
    newCssNode.parentNode.removeChild(newCssNode);
    // Sanity check.
    cssRules = cssom.getAllCssStyleRules();
    assertEquals(6, cssRules.length);
  },

  /** @suppress {missingProperties} cssRules not defined on StyleSheet */
  testAddCssRule() {
    // test that addCssRule correctly adds the rule to the style
    // sheet.
    const styleSheets = cssom.getAllCssStyleSheets();
    const styleSheet = styleSheets[3];
    const newCssRule = '.css-addCssRule { display: block; }';
    let rules = styleSheet.rules || styleSheet.cssRules;
    const origNumberOfRules = rules.length;

    cssom.addCssRule(styleSheet, newCssRule, 1);

    rules = styleSheet.rules || styleSheet.cssRules;
    const newNumberOfRules = rules.length;
    assertEquals(newNumberOfRules, origNumberOfRules + 1);

    // Remove the added rule so we don't mess up other tests.
    cssom.removeCssRule(styleSheet, 1);
  },

  /** @suppress {missingProperties} cssRules not defined on StyleSheet */
  testAddCssRuleAtPos() {
    // test that addCssRule correctly adds the rule to the style
    // sheet at the specified position.
    const styleSheets = cssom.getAllCssStyleSheets();
    const styleSheet = styleSheets[3];
    const newCssRule = '.css-addCssRulePos { display: block; }';
    let rules = cssom.getCssRulesFromStyleSheet(styleSheet);
    const origNumberOfRules = rules.length;

    // Firefox croaks if we try to insert a CSSRule at an index that
    // contains a CSSImport Rule. Since we deal only with CSSStyleRule
    // objects, we find the first CSSStyleRule and return its index.
    //
    // NOTE(user): We could have unified the code block below for all
    // browsers but IE6 horribly mangled up the stylesheet by creating
    // duplicate instances of a rule when removeCssRule was invoked
    // just after addCssRule with the looping construct in. This is
    // perfectly fine since IE's styleSheet.rules does not contain
    // references to anything but CSSStyleRules.
    let pos = 0;
    if (styleSheet.cssRules) {
      pos = Array.prototype.findIndex.call(
          rules, rule => rule.type == CssRuleType.STYLE);
    }
    cssom.addCssRule(styleSheet, newCssRule, pos);

    rules = cssom.getCssRulesFromStyleSheet(styleSheet);
    const newNumberOfRules = rules.length;
    assertEquals(newNumberOfRules, origNumberOfRules + 1);

    // Remove the added rule so we don't mess up other tests.
    cssom.removeCssRule(styleSheet, pos);

    rules = cssom.getCssRulesFromStyleSheet(styleSheet);
    assertEquals(origNumberOfRules, rules.length);
  },

  testAddCssRuleNoIndex() {
    // How well do we handle cases where the optional index is
    //  not passed in?
    const styleSheets = cssom.getAllCssStyleSheets();
    const styleSheet = styleSheets[3];
    let rules = cssom.getCssRulesFromStyleSheet(styleSheet);
    const origNumberOfRules = rules.length;
    const newCssRule = '.css-addCssRuleNoIndex { display: block; }';

    // Try inserting the rule without specifying an index.
    // Make sure we don't throw an exception, and that we added
    // the entry.
    cssom.addCssRule(styleSheet, newCssRule);

    rules = cssom.getCssRulesFromStyleSheet(styleSheet);
    const newNumberOfRules = rules.length;
    assertEquals(newNumberOfRules, origNumberOfRules + 1);

    // Remove the added rule so we don't mess up the other tests.
    cssom.removeCssRule(styleSheet, newNumberOfRules - 1);

    rules = cssom.getCssRulesFromStyleSheet(styleSheet);
    assertEquals(origNumberOfRules, rules.length);
  },

  testGetParentStyleSheetAfterGetAllCssStyleRules() {
    const cssRules = cssom.getAllCssStyleRules();
    const cssRule = cssRules[4];
    const parentStyleSheet = cssom.getParentStyleSheet(cssRule);
    const styleSheets = cssom.getAllCssStyleSheets();
    const styleSheet = styleSheets[3];
    assertEquals(styleSheet, parentStyleSheet);
  },

  /** @suppress {missingProperties} cssRules not defined on StyleSheet */
  testGetCssRuleIndexInParentStyleSheetAfterGetAllCssStyleRules() {
    const cssRules = cssom.getAllCssStyleRules();
    const cssRule = cssRules[4];
    // Note here that this is correct - IE's styleSheet.rules does not
    // contain references to anything but CSSStyleRules while FF and others
    // include anything that inherits from the CSSRule interface.
    // See http://dev.w3.org/csswg/cssom/#cssrule.
    const parentStyleSheet = cssom.getParentStyleSheet(cssRule);
    const ruleIndex = (parentStyleSheet.cssRules != null) ? 2 : 1;
    assertEquals(ruleIndex, cssom.getCssRuleIndexInParentStyleSheet(cssRule));
  },

  /** @suppress {missingProperties} cssRules not defined on StyleSheet */
  testGetCssRuleIndexInParentStyleSheetNonStyleRule() {
    // IE's styleSheet.rules only contain CSSStyleRules.
    if (!userAgent.IE) {
      const styleSheets = cssom.getAllCssStyleSheets();
      const styleSheet = styleSheets[3];
      const newCssRule = '@media print { .css-nonStyle { display: block; } }';
      cssom.addCssRule(styleSheet, newCssRule);
      const rules = styleSheet.rules || styleSheet.cssRules;
      const cssRule = rules[rules.length - 1];
      assertEquals(CssRuleType.MEDIA, cssRule.type);
      // Make sure we don't throw an exception.
      cssom.getCssRuleIndexInParentStyleSheet(cssRule, styleSheet);
      // Remove the added rule.
      cssom.removeCssRule(styleSheet, rules.length - 1);
    }
  },

  testReplaceCssRuleWithStyleSheetAndIndex() {
    const styleSheets = cssom.getAllCssStyleSheets();
    const styleSheet = styleSheets[3];
    const rules = cssom.getCssRulesFromStyleSheet(styleSheet);
    const index = 2;
    const origCssRule = rules[index];
    const origCssText =
        fixCssTextForIe(cssom.getCssTextFromCssRule(origCssRule));

    cssom.replaceCssRule(origCssRule, replacementCssText, styleSheet, index);

    const newRules = cssom.getCssRulesFromStyleSheet(styleSheet);
    const newCssRule = newRules[index];
    const newCssText = cssom.getCssTextFromCssRule(newCssRule);
    assertEquals(replacementCssText, fixCssTextForIe(newCssText));

    // Now we need to re-replace our rule, to preserve parity for the other
    // tests.
    cssom.replaceCssRule(newCssRule, origCssText, styleSheet, index);
    const nowRules = cssom.getCssRulesFromStyleSheet(styleSheet);
    const nowCssRule = nowRules[index];
    const nowCssText = cssom.getCssTextFromCssRule(nowCssRule);
    assertEquals(origCssText, fixCssTextForIe(nowCssText));
  },

  /** @suppress {missingProperties} cssRules not defined on StyleSheet */
  testReplaceCssRuleUsingGetAllCssStyleRules() {
    const cssRules = cssom.getAllCssStyleRules();
    const origCssRule = cssRules[4];
    const origCssText =
        fixCssTextForIe(cssom.getCssTextFromCssRule(origCssRule));
    // notice we don't pass in the stylesheet or index.
    cssom.replaceCssRule(origCssRule, replacementCssText);

    const styleSheets = cssom.getAllCssStyleSheets();
    const styleSheet = styleSheets[3];
    const rules = cssom.getCssRulesFromStyleSheet(styleSheet);
    const index = (styleSheet.cssRules != null) ? 2 : 1;
    const cssRule = rules[index];
    const cssText = fixCssTextForIe(cssom.getCssTextFromCssRule(cssRule));
    assertEquals(replacementCssText, cssText);

    // try getting it the other way around too.
    const newCssRules = cssom.getAllCssStyleRules();
    const newCssRule = newCssRules[4];
    const newCssText = fixCssTextForIe(cssom.getCssTextFromCssRule(newCssRule));
    assertEquals(replacementCssText, newCssText);

    // Now we need to re-replace our rule, to preserve parity for the other
    // tests.
    cssom.replaceCssRule(newCssRule, origCssText);
    const nowCssRules = cssom.getAllCssStyleRules();
    const nowCssRule = nowCssRules[4];
    const nowCssText = fixCssTextForIe(cssom.getCssTextFromCssRule(nowCssRule));
    assertEquals(origCssText, nowCssText);
  },
});
