/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for SafeStyle and its builders. */

goog.module('goog.html.safeStyleTest');
goog.setTestOnly();

const Const = goog.require('goog.string.Const');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SafeStyle = goog.require('goog.html.SafeStyle');
const SafeUrl = goog.require('goog.html.SafeUrl');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Asserts that created SafeStyle matches expected value.
 * @param {string} expected
 * @param {!SafeStyle.PropertyMap} style
 */
function assertCreateEquals(expected, style) {
  const styleWrapped = SafeStyle.create(style);
  assertEquals(expected, SafeStyle.unwrap(styleWrapped));
}

const stubs = new PropertyReplacer();

testSuite({
  tearDown() {
    stubs.reset();
  },

  testSafeStyle() {
    const style = 'width: 1em;height: 1em;';
    const safeStyle = SafeStyle.fromConstant(Const.from(style));
    const extracted = SafeStyle.unwrap(safeStyle);
    assertEquals(style, extracted);
    assertEquals(style, safeStyle.getTypedStringValue());
    assertEquals(`${style}`, String(safeStyle));

    // Interface marker is present.
    assertTrue(safeStyle.implementsGoogStringTypedString);
  },

  /** @suppress {checkTypes} */
  testUnwrap() {
    const privateFieldName = 'privateDoNotAccessOrElseSafeStyleWrappedValue_';
    const propNames =
        googObject.getKeys(SafeStyle.fromConstant(Const.from('')));
    assertContains(privateFieldName, propNames);
    const evil = {};
    evil[privateFieldName] = 'width: expression(evil);';

    const exception = assertThrows(() => {
      SafeStyle.unwrap(evil);
    });
    assertContains('expected object of type SafeStyle', exception.message);
  },

  testFromConstant_allowsEmptyString() {
    assertEquals(SafeStyle.EMPTY, SafeStyle.fromConstant(Const.from('')));
  },

  testFromConstant_throwsIfNoFinalSemicolon() {
    assertThrows(() => {
      SafeStyle.fromConstant(Const.from('width: 1em'));
    });
  },

  testFromConstant_throwsIfNoColon() {
    assertThrows(() => {
      SafeStyle.fromConstant(Const.from('width= 1em;'));
    });
  },

  testEmpty() {
    assertEquals('', SafeStyle.unwrap(SafeStyle.EMPTY));
  },

  testCreate() {
    assertCreateEquals(
        'background:url(i.png);margin:0;',
        {'background': Const.from('url(i.png)'), 'margin': '0'});
  },

  testCreate_allowsEmpty() {
    assertEquals(SafeStyle.EMPTY, SafeStyle.create({}));
  },

  testCreate_skipsNull() {
    const style = SafeStyle.create({'background': null});
    assertEquals(SafeStyle.EMPTY, style);
  },

  testCreate_allowsLengths() {
    assertCreateEquals(
        'padding:0 1px .2% 3.4em;',  // expected
        {'padding': '0 1px .2% 3.4em'});
  },

  testCreate_allowsRgb() {
    assertCreateEquals(
        'color:rgb(10,20,30);',  // expected
        {'color': 'rgb(10,20,30)'});
    assertCreateEquals(
        'color:rgb(10%, 20%, 30%);',  // expected
        {'color': 'rgb(10%, 20%, 30%)'});
    assertCreateEquals(
        'background:0 5px rgb(10,20,30);',  // expected
        {'background': '0 5px rgb(10,20,30)'});
    assertCreateEquals(
        'background:rgb(10,0,0), rgb(0,0,30);',
        {'background': 'rgb(10,0,0), rgb(0,0,30)'});
  },

  testCreate_allowsRgba() {
    assertCreateEquals(
        'color:rgba(10,20,30,0.1);',  // expected
        {'color': 'rgba(10,20,30,0.1)'});
    assertCreateEquals(
        'color:rgba(10%, 20%, 30%, .5);',  // expected
        {'color': 'rgba(10%, 20%, 30%, .5)'});
  },

  testCreate_allowsCalc() {
    assertCreateEquals(
        'height:calc(100% * 0.8 - 20px + 3vh);',  // expected
        {'height': 'calc(100% * 0.8 - 20px + 3vh)'});
  },

  testCreate_allowsRepeat() {
    assertCreateEquals(
        'grid-template-columns:repeat(3, [start] 100px [end]);',
        {'grid-template-columns': 'repeat(3, [start] 100px [end])'});
  },

  testCreate_allowsCubicBezier() {
    assertCreateEquals(
        'transition-timing-function:cubic-bezier(0.26, 0.86, 0.44, 0.95);',
        {'transition-timing-function': 'cubic-bezier(0.26, 0.86, 0.44, 0.95)'});
  },

  testCreate_allowsMinmax() {
    assertCreateEquals(
        'grid-template-columns:minmax(max-content, 50px) 20px;',
        {'grid-template-columns': 'minmax(max-content, 50px) 20px'});
  },

  testCreate_allowsFitContent() {
    assertCreateEquals(
        'grid-template-columns:fit-content(50px) 20px;',
        {'grid-template-columns': 'fit-content(50px) 20px'});
  },

  testCreate_allowsScale() {
    assertCreateEquals(
        'transform:scale(.5, 2);',  // expected
        {'transform': 'scale(.5, 2)'});
  },

  testCreate_allowsRotate() {
    assertCreateEquals(
        'transform:rotate(45deg);',  // expected
        {'transform': 'rotate(45deg)'});
  },

  testCreate_allowsTranslate() {
    assertCreateEquals(
        'transform:translate(10px);',  // expected
        {'transform': 'translate(10px)'});
    assertCreateEquals(
        'transform:translateX(5px);',  // expected
        {'transform': 'translateX(5px)'});
  },

  testCreate_allowsVar() {
    assertCreateEquals(
        'color:var(--xyz);',  // expected
        {'color': 'var(--xyz)'});
    assertCreateEquals(
        'color:var(--xyz, black);',  // expected
        {'color': 'var(--xyz, black)'});
  },

  testCreate_allowsSafeUrl() {
    assertCreateEquals('background:url("http://example.com");', {
      'background': SafeUrl.fromConstant(Const.from('http://example.com')),
    });
  },

  testCreate_allowsSafeUrlWithSpecialCharacters() {
    assertCreateEquals('background:url("http://example.com/\\"");', {
      'background': SafeUrl.fromConstant(Const.from('http://example.com/"')),
    });
    assertCreateEquals('background:url("http://example.com/%3c");', {
      'background': SafeUrl.fromConstant(Const.from('http://example.com/<')),
    });
    assertCreateEquals('background:url("http://example.com/;");', {
      'background': SafeUrl.fromConstant(Const.from('http://example.com/;')),
    });
  },

  testCreate_allowsArray() {
    const url = SafeUrl.fromConstant(Const.from('http://example.com'));
    assertCreateEquals(
        'background:red url("http://example.com") repeat-y;',
        {'background': ['red', url, 'repeat-y']});
  },

  testCreate_allowsUrl() {
    assertCreateEquals(
        'background:url(http://example.com);',
        {'background': 'url(http://example.com)'});
    assertCreateEquals(
        'background:url("http://example.com");',
        {'background': 'url("http://example.com")'});
    assertCreateEquals(
        'background:url( \'http://example.com\' );',
        {'background': 'url( \'http://example.com\' )'});
    assertCreateEquals(
        'background:url(http://example.com) red;',
        {'background': 'url(http://example.com) red'});
    assertCreateEquals(
        'background:url(' + SafeUrl.INNOCUOUS_STRING + ');',
        {'background': 'url(javascript:alert)'});
    assertCreateEquals(
        'background:url(")");',  // Expected.
        {'background': 'url(")")'});
    assertCreateEquals(
        'background:url(" ");',  // Expected.
        {'background': 'url(" ")'});
    assertThrows(() => {
      SafeStyle.create({'background': 'url(\'http://example.com\'"")'});
    });
    assertThrows(() => {
      SafeStyle.create({'background': 'url("\\\\")'});
    });
    assertThrows(() => {
      SafeStyle.create({'background': 'url(a""b)'});
    });
  },

  testCreate_throwsOnForbiddenCharacters() {
    assertThrows(() => {
      SafeStyle.create({'<': '0'});
    });
  },

  testCreate_allowsNestedFunctions() {
    assertCreateEquals(
        'grid-template-columns:repeat(3, minmax(100px, 200px));',
        {'grid-template-columns': 'repeat(3, minmax(100px, 200px))'});
    assertThrows(() => {
      SafeStyle.create({
        'grid-template-columns':
            'repeat(3, minmax(100px, minmax(200px, 300px)))',
      });
    });
  },

  testCreate_disallowsComments() {
    assertThrows(() => {
      SafeStyle.create({'color': 'rgb(/*)'});
    });
  },

  testCreate_allowBalancedSquareBrackets() {
    assertCreateEquals(
        'grid-template-columns:[trackName] 20px [other_track-name];',
        {'grid-template-columns': '[trackName] 20px [other_track-name]'});
    assertThrows(() => {
      SafeStyle.create({'grid-template-columns': '20px ["trackName"]'});
    });
    assertThrows(() => {
      SafeStyle.create({'grid-template-columns': '20px [tra[ckName]'});
    });
    assertThrows(() => {
      SafeStyle.create({'grid-template-columns': '20px [tra'});
    });
    assertThrows(() => {
      SafeStyle.create({'grid-template-columns': '20px [tra ckName]'});
    });
    assertThrows(() => {
      SafeStyle.create({'grid-template-columns': '20px [trackName] 20px]'});
    });
  },

  testCreate_values() {
    const valids = [
      '0',
      '0 0',
      '1px',
      '100%',
      '2.3px',
      '.1em',
      'red',
      '#f00',
      'red !important',
      '"Times New Roman"',
      '\'Times New Roman\'',
      '"Bold \'nuff"',
      '"O\'Connor\'s Revenge"',
    ];
    for (let i = 0; i < valids.length; i++) {
      const value = valids[i];
      assertCreateEquals(
          `background:${value};`,  // expected
          {'background': value});
    }

    const invalids = [
      '',
      'expression(alert(1))',
      '"',
      '"\'"\'',
      Const.from('red;'),
    ];
    for (let i = 0; i < invalids.length; i++) {
      const value = invalids[i];
      assertThrows(() => {
        SafeStyle.create({'background': value});
      });
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCreate_withMonkeypatchedObjectPrototype() {
    stubs.set(Object.prototype, 'foo', 'bar');
    assertCreateEquals(
        'background:url(i.png);margin:0;',
        {'background': Const.from('url(i.png)'), 'margin': '0'});
  },

  testConcat() {
    const width = SafeStyle.fromConstant(Const.from('width: 1em;'));
    const margin = SafeStyle.create({'margin': '0'});
    const padding = SafeStyle.create({'padding': '0'});

    let style = SafeStyle.concat(width, margin);
    assertEquals('width: 1em;margin:0;', SafeStyle.unwrap(style));

    style = SafeStyle.concat([width, margin]);
    assertEquals('width: 1em;margin:0;', SafeStyle.unwrap(style));

    style = SafeStyle.concat([width], [padding, margin]);
    assertEquals('width: 1em;padding:0;margin:0;', SafeStyle.unwrap(style));
  },

  testConcat_allowsEmpty() {
    const empty = SafeStyle.EMPTY;
    assertEquals(empty, SafeStyle.concat());
    assertEquals(empty, SafeStyle.concat([]));
    assertEquals(empty, SafeStyle.concat(empty));
  },
});
