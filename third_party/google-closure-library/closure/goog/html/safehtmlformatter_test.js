/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.html.safeHtmlFormatterTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SafeHtml = goog.require('goog.html.SafeHtml');
const SafeHtmlFormatter = goog.require('goog.html.SafeHtmlFormatter');
const SafeUrl = goog.require('goog.html.SafeUrl');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');

let stubs;

function assertSameHtml(expected, html) {
  assertEquals(expected, SafeHtml.unwrap(html));
}
testSuite({
  setUp() {
    stubs = new PropertyReplacer();
  },

  tearDown() {
    stubs.reset();
  },

  testFormat() {
    const formatter = new SafeHtmlFormatter();
    assertSameHtml(
        'a <b class="bold">&lt;bold&gt;</b> statement',
        formatter.format(googString.subs(
            'a %s<bold>%s %s', formatter.startTag('b', {'class': 'bold'}),
            formatter.endTag('b'), formatter.text('statement'))));
  },

  testFormatWithGetMsg() {
    const formatter = new SafeHtmlFormatter();
    assertSameHtml(
        'a <b class="bold">&lt;bold&gt;</b> statement',
        formatter.format(goog.getMsg('a {$startBold}<bold>{$endBold} {$type}', {
          'startBold': formatter.startTag('b', {'class': 'bold'}),
          'endBold': formatter.endTag('b'),
          'type': formatter.text('statement'),
        })));
  },

  testFormatWithGetMsgAndSafeValues() {
    const formatter = new SafeHtmlFormatter();
    assertSameHtml(
        'start <a href="about:invalid#zClosurez">bbb</a>' +
            ' <a href="about:blank">ccc</a> end',
        formatter.format(goog.getMsg(
            'start {$startA1}bbb{$endA1} {$startA2}ccc{$endA2} end', {
              'startA1':
                  formatter.startTag('a', {'href': 'javascript:alert(1)'}),
              'endA1': formatter.endTag('a'),
              'startA2': formatter.startTag('a', {'href': SafeUrl.ABOUT_BLANK}),
              'endA2': formatter.endTag('a'),
            })));
  },

  testFormatWithText() {
    const formatter = new SafeHtmlFormatter();
    // Escapes format.
    assertSameHtml('dinner &lt;3', formatter.format('dinner <3'));
    // Escapes .text().
    assertSameHtml(
        'dinner &lt;3', formatter.format(formatter.text('dinner <3')));
  },

  testFormatWithSafeHtml() {
    const formatter = new SafeHtmlFormatter();
    assertSameHtml(
        'User input: <b>abc</b>',
        formatter.format(
            'User input: ' +
            formatter.safeHtml(SafeHtml.create('b', {}, 'abc'))));
  },

  testFormatWithInternalMarkers() {
    const formatter = new SafeHtmlFormatter();

    // Immunity against something looking like our marker.
    assertSameHtml(
        '{SafeHtmlFormatter:abc}', formatter.format('{SafeHtmlFormatter:abc}'));
    assertSameHtml(
        '{SafeHtmlFormatter:<br>}',
        formatter.format(
            '{SafeHtmlFormatter:' + formatter.startTag('br') + '}'));

    // If an attacker steals our random marker and we format his input using
    // .text() then we will get back his input (the random marker), not the tag.
    const br = formatter.startTag('br');
    const attackerInput = br;
    assertSameHtml(
        googString.htmlEscape(attackerInput) + '<br>',
        formatter.format(formatter.text(attackerInput) + br));
  },

  testInvalidTag() {
     const formatter = new SafeHtmlFormatter();

    assertThrows(
                 () => {
                   formatter.startTag('a onclick="alert(1);"');
                 });
    assertThrows(() => {
      formatter.startTag('a', {'onclick': 'alert(1);'});
    });
    assertThrows(() => {
      formatter.startTag('script');
    });
    assertThrows(() => {
      formatter.endTag('script');
    });
  },

  testFormatBalancingTags() {
    const formatter = new SafeHtmlFormatter();

    // Void tags are OK.
    formatter.format(formatter.startTag('br'));

    // Balanced tags are OK.
    formatter.format(formatter.startTag('b') + formatter.endTag('b'));

    // Order of calling startTag and endTag doesn't matter.
    const endTag = formatter.endTag('b');
    const startTag = formatter.startTag('b');
    formatter.format(startTag + endTag);

    // Unbalanced tags throw.
    assertThrows(() => {
      formatter.format(formatter.endTag('b') + formatter.startTag('b'));
    });

    // Unclosed tags throw.
    assertThrows(() => {
      formatter.format(formatter.startTag('b'));
    });

    // Unopened tags throw.
    assertThrows(() => {
      formatter.format(formatter.endTag('b'));
    });
  },

  testDetectDoubleEscaping() {
    stubs.set(googString, 'DETECT_DOUBLE_ESCAPING', true);
    stubs.set(googString, 'ALL_RE_', /[\x00&<>"'e]/);
    const formatter = new SafeHtmlFormatter();
    assertSameHtml('t&#101;st', formatter.format(formatter.text('test')));
  },
});
