/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for SafeStyleSheet and its builders. */

goog.module('goog.html.safeStyleSheetTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const SafeStyle = goog.require('goog.html.SafeStyle');
const SafeStyleSheet = goog.require('goog.html.SafeStyleSheet');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * @param {string} expected
 * @param {string} selector
 * @param {!SafeStyle.PropertyMap|!SafeStyle} style
 */
function assertCreateRuleEquals(expected, selector, style) {
  const actual = SafeStyleSheet.createRule(selector, style);
  assertEquals(expected, SafeStyleSheet.unwrap(actual));
}

testSuite({
  testSafeStyleSheet() {
    const styleSheet = 'P.special { color:red ; }';
    const safeStyleSheet = SafeStyleSheet.fromConstant(Const.from(styleSheet));
    const extracted = SafeStyleSheet.unwrap(safeStyleSheet);
    assertEquals(styleSheet, extracted);
    assertEquals(styleSheet, safeStyleSheet.getTypedStringValue());
    assertEquals(`${styleSheet}`, String(safeStyleSheet));

    // Interface marker is present.
    assertTrue(safeStyleSheet.implementsGoogStringTypedString);
  },

  /** @suppress {checkTypes} */
  testUnwrap() {
    const privateFieldName =
        'privateDoNotAccessOrElseSafeStyleSheetWrappedValue_';
    const propNames =
        googObject.getKeys(SafeStyleSheet.fromConstant(Const.from('')));
    assertContains(privateFieldName, propNames);
    const evil = {};
    evil[privateFieldName] = 'P.special { color:expression(evil) ; }';

    const exception = assertThrows(() => {
      SafeStyleSheet.unwrap(evil);
    });
    assertContains('expected object of type SafeStyleSheet', exception.message);
  },

  testCreateRule() {
    assertCreateRuleEquals(
        '#id{top:0;left:0;}', '#id', {'top': '0', 'left': '0'});
    assertCreateRuleEquals(
        '.class{margin-left:5px;}', '.class',
        SafeStyle.create({'margin-left': '5px'}));
    assertCreateRuleEquals(
        'tag #id, .class{color:black !important;}', 'tag #id, .class',
        {'color': 'black !important'});
    assertCreateRuleEquals(
        '[title=\'son\\\'s\']{}', '[title=\'son\\\'s\']', {});
    assertCreateRuleEquals('[title="{"]{}', '[title="{"]', {});
    assertCreateRuleEquals(':nth-child(1){}', ':nth-child(1)', {});
    assertCreateRuleEquals(
        'a::before{content:"\\3C ";}', 'a::before',
        {'content': Const.from('"<"')});

    assertThrows(() => {
      SafeStyleSheet.createRule('tag{color:black;}', {});
    });
    assertThrows(() => {
      SafeStyleSheet.createRule('[title', {});
    });
    assertThrows(() => {
      SafeStyleSheet.createRule('[foo)bar]', {});
    });
    assertThrows(() => {
      SafeStyleSheet.createRule('[foo[bar]', {});
    });
    assertThrows(() => {
      SafeStyleSheet.createRule('foo(bar(baz)', {});
    });
    assertThrows(() => {
      SafeStyleSheet.createRule(':nth-child(1', {});
    });
    assertThrows(() => {
      SafeStyleSheet.createRule('[type="a]', {});
    });
    assertThrows(() => {
      SafeStyleSheet.createRule('[type=\'a]', {});
    });
    assertThrows(() => {
      SafeStyleSheet.createRule('<', {});
    });
    assertThrows(() => {
      SafeStyleSheet.createRule('@import "foo";#id', {});
    });
  },

  testFromConstant_allowsEmptyString() {
    assertEquals(
        SafeStyleSheet.EMPTY, SafeStyleSheet.fromConstant(Const.from('')));
  },

  testFromConstant_throwsOnLessThanCharacter() {
    assertThrows(() => {
      SafeStyleSheet.fromConstant(Const.from('x<x'));
    });
  },

  testConcat() {
    const styleSheet1 =
        SafeStyleSheet.fromConstant(Const.from('P.special { color:red ; }'));
    const styleSheet2 =
        SafeStyleSheet.fromConstant(Const.from('P.regular { color:blue ; }'));
    const expected = 'P.special { color:red ; }P.special { color:red ; }' +
        'P.regular { color:blue ; }P.regular { color:blue ; }';

    let concatStyleSheet = SafeStyleSheet.concat(
        styleSheet1, [styleSheet1, styleSheet2], styleSheet2);
    assertEquals(expected, SafeStyleSheet.unwrap(concatStyleSheet));

    // Empty.
    concatStyleSheet = SafeStyleSheet.concat();
    assertEquals('', SafeStyleSheet.unwrap(concatStyleSheet));
    concatStyleSheet = SafeStyleSheet.concat([]);
    assertEquals('', SafeStyleSheet.unwrap(concatStyleSheet));
  },

  testEmpty() {
    assertEquals('', SafeStyleSheet.unwrap(SafeStyleSheet.EMPTY));
  },
});
