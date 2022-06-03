/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.BidiFormatterTest');
goog.setTestOnly();

const BidiFormatter = goog.require('goog.i18n.BidiFormatter');
const Dir = goog.require('goog.i18n.bidi.Dir');
const Format = goog.require('goog.i18n.bidi.Format');
const SafeHtml = goog.require('goog.html.SafeHtml');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');

const LRM = Format.LRM;
const RLM = Format.RLM;
const LRE = Format.LRE;
const RLE = Format.RLE;
const PDF = Format.PDF;
const LTR = Dir.LTR;
const RTL = Dir.RTL;
const NEUTRAL = Dir.NEUTRAL;
const he = '\u05e0\u05e1';
const en = 'abba';
const html = '&lt;';
const longEn = 'abba sabba gabba ';
const longHe = '\u05e0 \u05e1 \u05e0 ';
const ltrFmt = new BidiFormatter(LTR, false);   // LTR context
const rtlFmt = new BidiFormatter(RTL, false);   // RTL context
const unkFmt = new BidiFormatter(null, false);  // unknown context

/**
 * @param {!BidiFormatter} formatter
 * @param {string} html
 * @param {boolean=} dirReset
 * @return {string}
 */
function spanWrap(formatter, html, dirReset = undefined) {
  return SafeHtml.unwrap(
      formatter.spanWrapSafeHtml(testing.newSafeHtmlForTest(html), dirReset));
}

/**
 * @param {!BidiFormatter} formatter
 * @param {?Dir} dir
 * @param {string} html
 * @return {string}
 */
function spanWrapWithKnownDir(formatter, dir, html) {
  return SafeHtml.unwrap(formatter.spanWrapSafeHtmlWithKnownDir(
      dir, testing.newSafeHtmlForTest(html)));
}

function assertHtmlEquals(expected, html) {
  assertEquals(expected, SafeHtml.unwrap(html));
}
testSuite({
  testGetContextDir() {
    assertEquals(null, unkFmt.getContextDir());
    assertEquals(null, new BidiFormatter(NEUTRAL).getContextDir());
    assertEquals(LTR, ltrFmt.getContextDir());
    assertEquals(RTL, rtlFmt.getContextDir());
  },

  testEstimateDirection() {
    assertEquals(NEUTRAL, ltrFmt.estimateDirection(''));
    assertEquals(NEUTRAL, rtlFmt.estimateDirection(''));
    assertEquals(NEUTRAL, unkFmt.estimateDirection(''));
    assertEquals(LTR, ltrFmt.estimateDirection(en));
    assertEquals(LTR, rtlFmt.estimateDirection(en));
    assertEquals(LTR, unkFmt.estimateDirection(en));
    assertEquals(RTL, ltrFmt.estimateDirection(he));
    assertEquals(RTL, rtlFmt.estimateDirection(he));
    assertEquals(RTL, unkFmt.estimateDirection(he));

    // Text contains HTML or HTML-escaping.
    assertEquals(
        LTR, ltrFmt.estimateDirection(`<some sort of tag/>${he} &amp;`, false));
    assertEquals(
        RTL, ltrFmt.estimateDirection(`<some sort of tag/>${he} &amp;`, true));
  },

  testDirAttrValue() {
    assertEquals(
        'overall dir is RTL, context dir is LTR', 'rtl',
        ltrFmt.dirAttrValue(he, true));
    assertEquals(
        'overall dir and context dir are RTL', 'rtl',
        rtlFmt.dirAttrValue(he, true));
    assertEquals(
        'overall dir is LTR, context dir is RTL', 'ltr',
        rtlFmt.dirAttrValue(en, true));
    assertEquals(
        'overall dir and context dir are LTR', 'ltr',
        ltrFmt.dirAttrValue(en, true));

    // Input's directionality is neutral.
    assertEquals('ltr', ltrFmt.dirAttrValue('', true));
    assertEquals('rtl', rtlFmt.dirAttrValue('', true));
    assertEquals('ltr', unkFmt.dirAttrValue('', true));

    // Text contains HTML or HTML-escaping:
    assertEquals(
        'rtl', ltrFmt.dirAttrValue(`${he}<some sort of an HTML tag>`, true));
    assertEquals(
        'ltr', ltrFmt.dirAttrValue(`${he}<some sort of an HTML tag>`, false));
  },

  testKnownDirAttrValue() {
    assertEquals('rtl', ltrFmt.knownDirAttrValue(RTL));
    assertEquals('rtl', rtlFmt.knownDirAttrValue(RTL));
    assertEquals('rtl', unkFmt.knownDirAttrValue(RTL));
    assertEquals('ltr', rtlFmt.knownDirAttrValue(LTR));
    assertEquals('ltr', ltrFmt.knownDirAttrValue(LTR));
    assertEquals('ltr', unkFmt.knownDirAttrValue(LTR));

    // Input directionality is neutral.
    assertEquals('ltr', ltrFmt.knownDirAttrValue(NEUTRAL));
    assertEquals('rtl', rtlFmt.knownDirAttrValue(NEUTRAL));
    assertEquals('ltr', unkFmt.knownDirAttrValue(NEUTRAL));
  },

  testDirAttr() {
    assertEquals(
        'overall dir (RTL) doesnt match context dir (LTR)', 'dir="rtl"',
        ltrFmt.dirAttr(he, true));
    assertEquals(
        'overall dir (RTL) doesnt match context dir (unknown)', 'dir="rtl"',
        unkFmt.dirAttr(he, true));
    assertEquals(
        'overall dir matches context dir (RTL)', '', rtlFmt.dirAttr(he, true));

    assertEquals(
        'overall dir (LTR) doesnt match context dir (RTL)', 'dir="ltr"',
        rtlFmt.dirAttr(en, true));
    assertEquals(
        'overall dir (LTR) doesnt match context dir (unknown)', 'dir="ltr"',
        unkFmt.dirAttr(en, true));
    assertEquals(
        'overall dir matches context dir (LTR)', '', ltrFmt.dirAttr(en, true));

    assertEquals('neutral in RTL context', '', rtlFmt.dirAttr('.', true));
    assertEquals('neutral in LTR context', '', ltrFmt.dirAttr('.', true));
    assertEquals('neutral in unknown context', '', unkFmt.dirAttr('.', true));

    // Text contains HTML or HTML-escaping:
    assertEquals(
        'dir="rtl"', ltrFmt.dirAttr(`${he}<some sort of an HTML tag>`, true));
    assertEquals('', ltrFmt.dirAttr(`${he}<some sort of an HTML tag>`, false));
  },

  testKnownDirAttr() {
    assertEquals(
        'overall dir (RTL) doesnt match context dir (LTR)', 'dir="rtl"',
        ltrFmt.knownDirAttr(RTL));
    assertEquals(
        'overall dir matches context dir (RTL)', '', rtlFmt.knownDirAttr(RTL));
    assertEquals(
        'overall dir (LTR) doesnt match context dir (RTL)', 'dir="ltr"',
        rtlFmt.knownDirAttr(LTR));
    assertEquals(
        'overall dir matches context dir (LTR)', '', ltrFmt.knownDirAttr(LTR));
  },

  testSpanWrap() {
    // alwaysSpan is false and opt_isHtml is true, unless specified otherwise.
    assertEquals(
        'overall dir matches context dir (LTR), no dirReset', en,
        spanWrap(ltrFmt, en, false));
    assertEquals(
        'overall dir matches context dir (LTR), dirReset', en,
        spanWrap(ltrFmt, en, true));
    assertEquals(
        'overall dir matches context dir (RTL), no dirReset', he,
        spanWrap(rtlFmt, he, false));
    assertEquals(
        'overall dir matches context dir (RTL), dirReset', he,
        spanWrap(rtlFmt, he, true));

    assertEquals(
        'overall dir (RTL) doesnt match context dir (LTR), ' +
            'no dirReset',
        `<span dir="rtl">${he}</span>`, spanWrap(ltrFmt, he, false));
    assertEquals(
        'overall dir (RTL) doesnt match context dir (LTR), dirReset',
        `<span dir="rtl">${he}</span>${LRM}`, spanWrap(ltrFmt, he, true));
    assertEquals(
        'overall dir (LTR) doesnt match context dir (RTL), ' +
            'no dirReset',
        `<span dir="ltr">${en}</span>`, spanWrap(rtlFmt, en, false));
    assertEquals(
        'overall dir (LTR) doesnt match context dir (RTL), dirReset',
        `<span dir="ltr">${en}</span>${RLM}`, spanWrap(rtlFmt, en, true));
    assertEquals(
        'overall dir (LTR) doesnt match context dir (unknown), ' +
            'no dirReset',
        `<span dir="ltr">${en}</span>`, spanWrap(unkFmt, en, false));
    assertEquals(
        'overall dir (RTL) doesnt match context dir (unknown), ' +
            'dirReset',
        `<span dir="rtl">${he}</span>`, spanWrap(unkFmt, he, true));
    assertEquals(
        'overall dir (neutral) doesnt match context dir (LTR), ' +
            'dirReset',
        '', spanWrap(ltrFmt, '', true));

    assertEquals(
        'exit dir (but not overall dir) is opposite to context dir, ' +
            'dirReset',
        longEn + he + html + LRM, spanWrap(ltrFmt, longEn + he + html, true));
    assertEquals(
        'overall dir (but not exit dir) is opposite to context dir, ' +
            'dirReset',
        `<span dir="ltr">${longEn}${he}</span>${RLM}`,
        spanWrap(rtlFmt, longEn + he, true));

    const ltrAlwaysSpanFmt = new BidiFormatter(LTR, true);
    const rtlAlwaysSpanFmt = new BidiFormatter(RTL, true);
    const unkAlwaysSpanFmt = new BidiFormatter(null, true);

    assertEquals(
        'alwaysSpan, overall dir matches context dir (LTR), ' +
            'no dirReset',
        `<span>${en}</span>`, spanWrap(ltrAlwaysSpanFmt, en, false));
    assertEquals(
        'alwaysSpan, overall dir matches context dir (LTR), dirReset',
        `<span>${en}</span>`, spanWrap(ltrAlwaysSpanFmt, en, true));
    assertEquals(
        'alwaysSpan, overall dir matches context dir (RTL), ' +
            'no dirReset',
        `<span>${he}</span>`, spanWrap(rtlAlwaysSpanFmt, he, false));
    assertEquals(
        'alwaysSpan, overall dir matches context dir (RTL), dirReset',
        `<span>${he}</span>`, spanWrap(rtlAlwaysSpanFmt, he, true));

    assertEquals(
        'alwaysSpan, overall dir (RTL) doesnt match ' +
            'context dir (LTR), no dirReset',
        `<span dir="rtl">${he}</span>`, spanWrap(ltrAlwaysSpanFmt, he, false));
    assertEquals(
        'alwaysSpan, overall dir (RTL) doesnt match ' +
            'context dir (LTR), dirReset',
        `<span dir="rtl">${he}</span>${LRM}`,
        spanWrap(ltrAlwaysSpanFmt, he, true));
    assertEquals(
        'alwaysSpan, overall dir (neutral) doesnt match ' +
            'context dir (LTR), dirReset',
        '<span></span>', spanWrap(ltrAlwaysSpanFmt, '', true));
  },

  testSpanWrapSafeHtml() {
    const html = SafeHtml.htmlEscape('a');
    const wrapped = rtlFmt.spanWrapSafeHtml(html, false);
    assertHtmlEquals('<span dir="ltr">a</span>', wrapped);
    assertEquals(NEUTRAL, wrapped.getDirection());
  },

  testSpanWrapWithKnownDir() {
    assertEquals(
        'known LTR in LTR context', en, spanWrapWithKnownDir(ltrFmt, LTR, en));
    assertEquals(
        'unknown LTR in LTR context', en,
        spanWrapWithKnownDir(ltrFmt, null, en));
    assertEquals(
        'overall LTR but exit RTL in LTR context', he + LRM,
        spanWrapWithKnownDir(ltrFmt, LTR, he));
    assertEquals(
        'known RTL in LTR context', `<span dir="rtl">${he}</span>${LRM}`,
        spanWrapWithKnownDir(ltrFmt, RTL, he));
    assertEquals(
        'unknown RTL in LTR context', `<span dir="rtl">${he}</span>${LRM}`,
        spanWrapWithKnownDir(ltrFmt, null, he));
    assertEquals(
        'overall RTL but exit LTR in LTR context',
        `<span dir="rtl">${en}</span>${LRM}`,
        spanWrapWithKnownDir(ltrFmt, RTL, en));
    assertEquals(
        'known neutral in LTR context', '.',
        spanWrapWithKnownDir(ltrFmt, NEUTRAL, '.'));
    assertEquals(
        'unknown neutral in LTR context', '.',
        spanWrapWithKnownDir(ltrFmt, null, '.'));
    assertEquals(
        'overall neutral but exit LTR in LTR context', en,
        spanWrapWithKnownDir(ltrFmt, NEUTRAL, en));
    assertEquals(
        'overall neutral but exit RTL in LTR context', he + LRM,
        spanWrapWithKnownDir(ltrFmt, NEUTRAL, he));

    assertEquals(
        'known RTL in RTL context', he, spanWrapWithKnownDir(rtlFmt, RTL, he));
    assertEquals(
        'unknown RTL in RTL context', he,
        spanWrapWithKnownDir(rtlFmt, null, he));
    assertEquals(
        'overall RTL but exit LTR in RTL context', en + RLM,
        spanWrapWithKnownDir(rtlFmt, RTL, en));
    assertEquals(
        'known LTR in RTL context', `<span dir="ltr">${en}</span>${RLM}`,
        spanWrapWithKnownDir(rtlFmt, LTR, en));
    assertEquals(
        'unknown LTR in RTL context', `<span dir="ltr">${en}</span>${RLM}`,
        spanWrapWithKnownDir(rtlFmt, null, en));
    assertEquals(
        'LTR but exit RTL in RTL context', `<span dir="ltr">${he}</span>${RLM}`,
        spanWrapWithKnownDir(rtlFmt, LTR, he));
    assertEquals(
        'known neutral in RTL context', '.',
        spanWrapWithKnownDir(rtlFmt, NEUTRAL, '.'));
    assertEquals(
        'unknown neutral in RTL context', '.',
        spanWrapWithKnownDir(rtlFmt, null, '.'));
    assertEquals(
        'overall neutral but exit LTR in LTR context', he,
        spanWrapWithKnownDir(rtlFmt, NEUTRAL, he));
    assertEquals(
        'overall neutral but exit RTL in LTR context', en + RLM,
        spanWrapWithKnownDir(rtlFmt, NEUTRAL, en));

    assertEquals(
        'known RTL in unknown context', `<span dir="rtl">${he}</span>`,
        spanWrapWithKnownDir(unkFmt, RTL, he));
    assertEquals(
        'unknown RTL in unknown context', `<span dir="rtl">${he}</span>`,
        spanWrapWithKnownDir(unkFmt, null, he));
    assertEquals(
        'overall RTL but exit LTR in unknown context',
        `<span dir="rtl">${en}</span>`, spanWrapWithKnownDir(unkFmt, RTL, en));
    assertEquals(
        'known LTR in unknown context', `<span dir="ltr">${en}</span>`,
        spanWrapWithKnownDir(unkFmt, LTR, en));
    assertEquals(
        'unknown LTR in unknown context', `<span dir="ltr">${en}</span>`,
        spanWrapWithKnownDir(unkFmt, null, en));
    assertEquals(
        'LTR but exit RTL in unknown context', `<span dir="ltr">${he}</span>`,
        spanWrapWithKnownDir(unkFmt, LTR, he));
    assertEquals(
        'known neutral in unknown context', '.',
        spanWrapWithKnownDir(unkFmt, NEUTRAL, '.'));
    assertEquals(
        'unknown neutral in unknown context', '.',
        spanWrapWithKnownDir(unkFmt, null, '.'));
    assertEquals(
        'overall neutral but exit LTR in unknown context', he,
        spanWrapWithKnownDir(unkFmt, NEUTRAL, he));
    assertEquals(
        'overall neutral but exit RTL in unknown context', en,
        spanWrapWithKnownDir(unkFmt, NEUTRAL, en));
  },

  testSpanWrapSafeHtmlWithKnownDir() {
    const html = SafeHtml.htmlEscape('a');
    assertHtmlEquals(
        '<span dir="ltr">a</span>',
        rtlFmt.spanWrapSafeHtmlWithKnownDir(LTR, html, false));
  },

  testUnicodeWrap() {
    // opt_isHtml is true, unless specified otherwise.
    assertEquals(
        'overall dir matches context dir (LTR), no dirReset', en,
        ltrFmt.unicodeWrap(en, true, false));
    assertEquals(
        'overall dir matches context dir (LTR), dirReset', en,
        ltrFmt.unicodeWrap(en, true, true));
    assertEquals(
        'overall dir matches context dir (RTL), no dirReset', he,
        rtlFmt.unicodeWrap(he, true, false));
    assertEquals(
        'overall dir matches context dir (RTL), dirReset', he,
        rtlFmt.unicodeWrap(he, true, true));

    assertEquals(
        'overall dir (RTL) doesnt match context dir (LTR), ' +
            'no dirReset',
        RLE + he + PDF, ltrFmt.unicodeWrap(he, true, false));
    assertEquals(
        'overall dir (RTL) doesnt match context dir (LTR), dirReset',
        RLE + he + PDF + LRM, ltrFmt.unicodeWrap(he, true, true));
    assertEquals(
        'overall dir (LTR) doesnt match context dir (RTL), ' +
            'no dirReset',
        LRE + en + PDF, rtlFmt.unicodeWrap(en, true, false));
    assertEquals(
        'overall dir (LTR) doesnt match context dir (RTL), dirReset',
        LRE + en + PDF + RLM, rtlFmt.unicodeWrap(en, true, true));
    assertEquals(
        'overall dir (LTR) doesnt match context dir (unknown), ' +
            'no dirReset',
        LRE + en + PDF, unkFmt.unicodeWrap(en, true, false));
    assertEquals(
        'overall dir (RTL) doesnt match context dir (unknown), ' +
            'dirReset',
        RLE + he + PDF, unkFmt.unicodeWrap(he, true, true));
    assertEquals(
        'overall dir (neutral) doesnt match context dir (LTR), ' +
            'dirReset',
        '', ltrFmt.unicodeWrap('', true, true));

    assertEquals(
        'exit dir (but not overall dir) is opposite to context dir, ' +
            'dirReset',
        longEn + he + html + LRM,
        ltrFmt.unicodeWrap(longEn + he + html, true, true));
    assertEquals(
        'overall dir (but not exit dir) is opposite to context dir, ' +
            'dirReset',
        LRE + longEn + he + PDF + RLM,
        rtlFmt.unicodeWrap(longEn + he, true, true));
  },

  testUnicodeWrapWithKnownDir() {
    assertEquals(
        'known LTR in LTR context', en,
        ltrFmt.unicodeWrapWithKnownDir(LTR, en));
    assertEquals(
        'unknown LTR in LTR context', en,
        ltrFmt.unicodeWrapWithKnownDir(null, en));
    assertEquals(
        'overall LTR but exit RTL in LTR context', he + LRM,
        ltrFmt.unicodeWrapWithKnownDir(LTR, he));
    assertEquals(
        'known RTL in LTR context', RLE + he + PDF + LRM,
        ltrFmt.unicodeWrapWithKnownDir(RTL, he));
    assertEquals(
        'unknown RTL in LTR context', RLE + he + PDF + LRM,
        ltrFmt.unicodeWrapWithKnownDir(null, he));
    assertEquals(
        'overall RTL but exit LTR in LTR context', RLE + en + PDF + LRM,
        ltrFmt.unicodeWrapWithKnownDir(RTL, en));
    assertEquals(
        'known neutral in LTR context', '.',
        ltrFmt.unicodeWrapWithKnownDir(NEUTRAL, '.'));
    assertEquals(
        'unknown neutral in LTR context', '.',
        ltrFmt.unicodeWrapWithKnownDir(null, '.'));
    assertEquals(
        'overall neutral but exit LTR in LTR context', en,
        ltrFmt.unicodeWrapWithKnownDir(NEUTRAL, en));
    assertEquals(
        'overall neutral but exit RTL in LTR context', he + LRM,
        ltrFmt.unicodeWrapWithKnownDir(NEUTRAL, he));

    assertEquals(
        'known RTL in RTL context', he,
        rtlFmt.unicodeWrapWithKnownDir(RTL, he));
    assertEquals(
        'unknown RTL in RTL context', he,
        rtlFmt.unicodeWrapWithKnownDir(null, he));
    assertEquals(
        'overall RTL but exit LTR in RTL context', en + RLM,
        rtlFmt.unicodeWrapWithKnownDir(RTL, en));
    assertEquals(
        'known LTR in RTL context', LRE + en + PDF + RLM,
        rtlFmt.unicodeWrapWithKnownDir(LTR, en));
    assertEquals(
        'unknown LTR in RTL context', LRE + en + PDF + RLM,
        rtlFmt.unicodeWrapWithKnownDir(null, en));
    assertEquals(
        'LTR but exit RTL in RTL context', LRE + he + PDF + RLM,
        rtlFmt.unicodeWrapWithKnownDir(LTR, he));
    assertEquals(
        'known neutral in RTL context', '.',
        rtlFmt.unicodeWrapWithKnownDir(NEUTRAL, '.'));
    assertEquals(
        'unknown neutral in RTL context', '.',
        rtlFmt.unicodeWrapWithKnownDir(null, '.'));
    assertEquals(
        'overall neutral but exit LTR in LTR context', he,
        rtlFmt.unicodeWrapWithKnownDir(NEUTRAL, he));
    assertEquals(
        'overall neutral but exit RTL in LTR context', en + RLM,
        rtlFmt.unicodeWrapWithKnownDir(NEUTRAL, en));

    assertEquals(
        'known RTL in unknown context', RLE + he + PDF,
        unkFmt.unicodeWrapWithKnownDir(RTL, he));
    assertEquals(
        'unknown RTL in unknown context', RLE + he + PDF,
        unkFmt.unicodeWrapWithKnownDir(null, he));
    assertEquals(
        'overall RTL but exit LTR in unknown context', RLE + en + PDF,
        unkFmt.unicodeWrapWithKnownDir(RTL, en));
    assertEquals(
        'known LTR in unknown context', LRE + en + PDF,
        unkFmt.unicodeWrapWithKnownDir(LTR, en));
    assertEquals(
        'unknown LTR in unknown context', LRE + en + PDF,
        unkFmt.unicodeWrapWithKnownDir(null, en));
    assertEquals(
        'LTR but exit RTL in unknown context', LRE + he + PDF,
        unkFmt.unicodeWrapWithKnownDir(LTR, he));
    assertEquals(
        'known neutral in unknown context', '.',
        unkFmt.unicodeWrapWithKnownDir(NEUTRAL, '.'));
    assertEquals(
        'unknown neutral in unknown context', '.',
        unkFmt.unicodeWrapWithKnownDir(null, '.'));
    assertEquals(
        'overall neutral but exit LTR in unknown context', he,
        unkFmt.unicodeWrapWithKnownDir(NEUTRAL, he));
    assertEquals(
        'overall neutral but exit RTL in unknown context', en,
        unkFmt.unicodeWrapWithKnownDir(NEUTRAL, en));
  },

  testMarkAfter() {
    assertEquals(
        'exit dir (RTL) is opposite to context dir (LTR)', LRM,
        ltrFmt.markAfter(longEn + he + html, true));
    assertEquals(
        'exit dir (LTR) is opposite to context dir (RTL)', RLM,
        rtlFmt.markAfter(longHe + en, true));
    assertEquals(
        'exit dir (LTR) doesnt match context dir (unknown)', '',
        unkFmt.markAfter(longEn + en, true));
    assertEquals(
        'overall dir (RTL) is opposite to context dir (LTR)', LRM,
        ltrFmt.markAfter(longHe + en, true));
    assertEquals(
        'overall dir (LTR) is opposite to context dir (RTL)', RLM,
        rtlFmt.markAfter(longEn + he, true));
    assertEquals(
        'exit dir and overall dir match context dir (LTR)', '',
        ltrFmt.markAfter(longEn + he + html, false));
    assertEquals(
        'exit dir and overall dir matches context dir (RTL)', '',
        rtlFmt.markAfter(longHe + he, true));
  },

  testMarkAfterKnownDir() {
    assertEquals(
        'known LTR in LTR context', '', ltrFmt.markAfterKnownDir(LTR, en));
    assertEquals(
        'unknown LTR in LTR context', '', ltrFmt.markAfterKnownDir(null, en));
    assertEquals(
        'overall LTR but exit RTL in LTR context', LRM,
        ltrFmt.markAfterKnownDir(LTR, he));
    assertEquals(
        'known RTL in LTR context', LRM, ltrFmt.markAfterKnownDir(RTL, he));
    assertEquals(
        'unknown RTL in LTR context', LRM, ltrFmt.markAfterKnownDir(null, he));
    assertEquals(
        'overall RTL but exit LTR in LTR context', LRM,
        ltrFmt.markAfterKnownDir(RTL, en));
    assertEquals(
        'known neutral in LTR context', '',
        ltrFmt.markAfterKnownDir(NEUTRAL, '.'));
    assertEquals(
        'unknown neutral in LTR context', '',
        ltrFmt.markAfterKnownDir(null, '.'));
    assertEquals(
        'overall neutral but exit LTR in LTR context', '',
        ltrFmt.markAfterKnownDir(NEUTRAL, en));
    assertEquals(
        'overall neutral but exit RTL in LTR context', LRM,
        ltrFmt.markAfterKnownDir(NEUTRAL, he));

    assertEquals(
        'known RTL in RTL context', '', rtlFmt.markAfterKnownDir(RTL, he));
    assertEquals(
        'unknown RTL in RTL context', '', rtlFmt.markAfterKnownDir(null, he));
    assertEquals(
        'overall RTL but exit LTR in RTL context', RLM,
        rtlFmt.markAfterKnownDir(RTL, en));
    assertEquals(
        'known LTR in RTL context', RLM, rtlFmt.markAfterKnownDir(LTR, en));
    assertEquals(
        'unknown LTR in RTL context', RLM, rtlFmt.markAfterKnownDir(null, en));
    assertEquals(
        'LTR but exit RTL in RTL context', RLM,
        rtlFmt.markAfterKnownDir(LTR, he));
    assertEquals(
        'known neutral in RTL context', '',
        rtlFmt.markAfterKnownDir(NEUTRAL, '.'));
    assertEquals(
        'unknown neutral in RTL context', '',
        rtlFmt.markAfterKnownDir(null, '.'));
    assertEquals(
        'overall neutral but exit LTR in LTR context', '',
        rtlFmt.markAfterKnownDir(NEUTRAL, he));
    assertEquals(
        'overall neutral but exit RTL in LTR context', RLM,
        rtlFmt.markAfterKnownDir(NEUTRAL, en));

    assertEquals(
        'known RTL in unknown context', '', unkFmt.markAfterKnownDir(RTL, he));
    assertEquals(
        'unknown RTL in unknown context', '',
        unkFmt.markAfterKnownDir(null, he));
    assertEquals(
        'overall RTL but exit LTR in unknown context', '',
        unkFmt.markAfterKnownDir(RTL, en));
    assertEquals(
        'known LTR in unknown context', '', unkFmt.markAfterKnownDir(LTR, en));
    assertEquals(
        'unknown LTR in unknown context', '',
        unkFmt.markAfterKnownDir(null, en));
    assertEquals(
        'LTR but exit RTL in unknown context', '',
        unkFmt.markAfterKnownDir(LTR, he));
    assertEquals(
        'known neutral in unknown context', '',
        unkFmt.markAfterKnownDir(NEUTRAL, '.'));
    assertEquals(
        'unknown neutral in unknown context', '',
        unkFmt.markAfterKnownDir(null, '.'));
    assertEquals(
        'overall neutral but exit LTR in unknown context', '',
        unkFmt.markAfterKnownDir(NEUTRAL, he));
    assertEquals(
        'overall neutral but exit RTL in unknown context', '',
        unkFmt.markAfterKnownDir(NEUTRAL, en));
  },

  testMark() {
    // Implicitly, also tests the constructor.
    assertEquals(LRM, (new BidiFormatter(LTR)).mark());
    assertEquals('', (new BidiFormatter(null)).mark());
    assertEquals('', (new BidiFormatter(NEUTRAL)).mark());
    assertEquals(RLM, (new BidiFormatter(RTL)).mark());
    assertEquals(RLM, (new BidiFormatter(true)).mark());
    assertEquals(LRM, (new BidiFormatter(false)).mark());
  },

  testStartEdge() {
    assertEquals('left', ltrFmt.startEdge());
    assertEquals('left', unkFmt.startEdge());
    assertEquals('right', rtlFmt.startEdge());
  },

  testEndEdge() {
    assertEquals('right', ltrFmt.endEdge());
    assertEquals('right', unkFmt.endEdge());
    assertEquals('left', rtlFmt.endEdge());
  },
});
