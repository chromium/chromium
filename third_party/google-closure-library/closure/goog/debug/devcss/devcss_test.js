/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.debug.DevCssTest');
goog.setTestOnly();

const DevCss = goog.require('goog.debug.DevCss');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let el;

// Since background color sometimes comes back like rgb(xxx, xxx, xxx)
// or rgb(xxx,xxx,xxx) depending on browser.
function spaceless(foo) {
  return foo.replace(/\s/g, '');
}

/*
 * TODO(user): Re-enable if we ever come up with a way to make imports
 * work.
function testDisableDuplicateStyleSheetImports() {
  var el1 = document.getElementById('devcss-test-importfixer-1');
  var el2 = document.getElementById('devcss-test-importfixer-2');

  var backgroundColor = goog.style.getBackgroundColor(el1);
  assertEquals('rgb(255,255,0)', spaceless(backgroundColor));

  var backgroundColor = goog.style.getBackgroundColor(el2);
  assertEquals('rgb(255,0,0)', spaceless(backgroundColor));

  // This should disable the second coming of devcss_test_import_1.css.
  var devcssInstance = new goog.debug.DevCss();
  devcssInstance.disableDuplicateStyleSheetImports();

  var backgroundColor = goog.style.getBackgroundColor(el1);
  assertEquals('rgb(255,255,0)', spaceless(backgroundColor));

  var backgroundColor = goog.style.getBackgroundColor(el2);
  assertEquals('rgb(173,216,230)', spaceless(backgroundColor));
}
*/
testSuite({
  setUpPage() {
    el = document.getElementById('devcss-test-2');
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetIe6CombinedSelectorText() {
    let devcssInstance = new DevCss();
    /** @suppress {visibility} suppression added to enable type checking */
    devcssInstance.ie6CombinedMatches_ = [];
    let css = '.class2 { -goog-ie6-selector:".class1_class2"; prop: val; }';
    /** @suppress {visibility} suppression added to enable type checking */
    let newCss = devcssInstance.getIe6CombinedSelectorText_(css);
    assertEquals('.class1_class2', newCss);
    assertArrayEquals(
        ['class1', 'class2'], devcssInstance.ie6CombinedMatches_[0].classNames);
    assertEquals(
        'class1_class2',
        devcssInstance.ie6CombinedMatches_[0].combinedClassName);

    devcssInstance = new DevCss();
    /** @suppress {visibility} suppression added to enable type checking */
    devcssInstance.ie6CombinedMatches_ = [];
    css = '.class3 { prop: val; -goog-ie6-selector:".class1_class2_class3";' +
        'prop: val; }';
    /** @suppress {visibility} suppression added to enable type checking */
    newCss = devcssInstance.getIe6CombinedSelectorText_(css);
    assertEquals('.class1_class2_class3', newCss);
    assertArrayEquals(
        ['class1', 'class2', 'class3'],
        devcssInstance.ie6CombinedMatches_[0].classNames);
    assertEquals(
        'class1_class2_class3',
        devcssInstance.ie6CombinedMatches_[0].combinedClassName);

    devcssInstance = new DevCss();
    /** @suppress {visibility} suppression added to enable type checking */
    devcssInstance.ie6CombinedMatches_ = [];
    css = '.class3, .class5 {' +
        '-goog-ie6-selector:".class1_class2_class3, .class4_class5";' +
        'prop: val; }';
    /** @suppress {visibility} suppression added to enable type checking */
    newCss = devcssInstance.getIe6CombinedSelectorText_(css);
    assertEquals('.class1_class2_class3, .class4_class5', newCss);
    assertArrayEquals(
        ['class1', 'class2', 'class3'],
        devcssInstance.ie6CombinedMatches_[0].classNames);
    assertEquals(
        'class1_class2_class3',
        devcssInstance.ie6CombinedMatches_[0].combinedClassName);
    assertArrayEquals(
        ['class4', 'class5'], devcssInstance.ie6CombinedMatches_[1].classNames);
    assertEquals(
        'class4_class5',
        devcssInstance.ie6CombinedMatches_[1].combinedClassName);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddIe6CombinedClassNames() {
    const el_combined1 = document.getElementById('devcss-test-combined1');
    const el_combined2 = document.getElementById('devcss-test-combined2');
    const el_notcombined1 = document.getElementById('devcss-test-notcombined1');
    const el_notcombined2 = document.getElementById('devcss-test-notcombined2');
    const el_notcombined3 = document.getElementById('devcss-test-notcombined3');

    const devcssInstance = new DevCss();
    /** @suppress {visibility} suppression added to enable type checking */
    devcssInstance.ie6CombinedMatches_ = [
      {
        classNames: ['ie6-2', 'ie6-1'],
        combinedClassName: 'ie6-1_ie6-2',
        els: []
      },
      {
        classNames: ['ie6-2', 'ie6-3', 'ie6-1'],
        combinedClassName: 'ie6-1_ie6-2_ie6-3',
        els: [],
      },
    ];

    devcssInstance.addIe6CombinedClassNames_();
    assertEquals(-1, el_notcombined1.className.indexOf('ie6-1_ie6-2'));
    assertEquals(-1, el_notcombined2.className.indexOf('ie6-1_ie6-2'));
    assertEquals(-1, el_notcombined3.className.indexOf('ie6-1_ie6-2_ie6-3'));
    assertTrue(el_combined1.className.indexOf('ie6-1_ie6-2') != -1);
    assertTrue(el_combined2.className.indexOf('ie6-1_ie6-2_ie6-3') != -1);
  },

  testActivateBrowserSpecificCssALL() {
    // equals GECKO
    /** @suppress {checkTypes} suppression added to enable type checking */
    const devcssInstance = new DevCss('GECKO');
    devcssInstance.activateBrowserSpecificCssRules(false);
    let backgroundColor = style.getBackgroundColor(el);
    assertEquals('rgb(255,192,203)', spaceless(backgroundColor));

    // GECKO test case w/ two selectors joined by a commma.
    const elGeckoOne = document.getElementById('devcss-gecko-1');
    backgroundColor = style.getBackgroundColor(elGeckoOne);
    assertEquals('rgb(255,192,203)', spaceless(backgroundColor));
    const elGeckoTwo = document.getElementById('devcss-gecko-2');
    backgroundColor = style.getBackgroundColor(elGeckoTwo);
    assertEquals('rgb(255,192,203)', spaceless(backgroundColor));
  },

  testActivateBrowserSpecificCssWithVersion() {
    // equals IE 6
    /** @suppress {checkTypes} suppression added to enable type checking */
    const devcssInstance = new DevCss('IE', '6');
    devcssInstance.activateBrowserSpecificCssRules(false);
    const elIe6 = document.getElementById('devcss-test-ie6');
    let backgroundColor = style.getBackgroundColor(elIe6);
    assertEquals('rgb(255,192,203)', spaceless(backgroundColor));

    // IE8 test case w/ two selectors joined by a commma.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const devCssInstanceTwo = new DevCss('IE', '8');
    devCssInstanceTwo.activateBrowserSpecificCssRules(false);
    const elIe8One = document.getElementById('devcss-ie8-1');
    backgroundColor = style.getBackgroundColor(elIe8One);
    assertEquals('rgb(255,192,203)', spaceless(backgroundColor));
    const elIe8Two = document.getElementById('devcss-ie8-2');
    backgroundColor = style.getBackgroundColor(elIe8Two);
    assertEquals('rgb(255,192,203)', spaceless(backgroundColor));
  },

  testActivateBrowserSpecificCssGteInvalid() {
    // WEBKIT_GTE255
    let marginBox = style.getMarginBox(el);
    assertEquals(1, marginBox.top);  // should still be 1

    /** @suppress {checkTypes} suppression added to enable type checking */
    const devcssInstance = new DevCss('WEBKIT', 254);
    devcssInstance.activateBrowserSpecificCssRules(false);
    marginBox = style.getMarginBox(el);
    assertEquals(1, marginBox.top);  // should still be 1
  },

  testActivateBrowserSpecificCssGteValid() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const devcssInstance = new DevCss('WEBKIT', 255);
    devcssInstance.activateBrowserSpecificCssRules(false);
    const marginBox = style.getMarginBox(el);
    assertEquals(20, marginBox.top);
  },

  testActivateBrowserSpecificCssLteInvalid() {
    // IE_LTE6
    let marginBox = style.getMarginBox(el);
    assertEquals(1, marginBox.left);  // should still be 1

    /** @suppress {checkTypes} suppression added to enable type checking */
    const devcssInstance = new DevCss('WEBKIT', 202);
    devcssInstance.activateBrowserSpecificCssRules(false);
    marginBox = style.getMarginBox(el);
    assertEquals(1, marginBox.left);  // should still be 1
  },

  testActivateBrowserSpecificCssLteValid() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const devcssInstance = new DevCss('WEBKIT', 199);
    devcssInstance.activateBrowserSpecificCssRules(false);
    const marginBox = style.getMarginBox(el);
    assertEquals(15, marginBox.left);
  },

  testReplaceIe6Selectors() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const devcssInstance = new DevCss('IE', 6);
    devcssInstance.activateBrowserSpecificCssRules(false);

    // It should correctly be transparent, even in IE6.
    const compound2El = document.getElementById('devcss-test-compound2');
    let backgroundColor = spaceless(style.getBackgroundColor(compound2El));

    assertTrue(
        `Unexpected background color: ${backgroundColor}`,
        'transparent' == backgroundColor || 'rgba(0,0,0,0)' == backgroundColor);

    // And this one should have the combined selector working, even in
    // IE6.
    backgroundColor = style.getBackgroundColor(
        document.getElementById('devcss-test-compound1-2'));
    assertEquals('rgb(255,192,203)', spaceless(backgroundColor));
  },
});
