/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.bidiTest');
goog.setTestOnly();

const Dir = goog.require('goog.i18n.bidi.Dir');
const bidi = goog.require('goog.i18n.bidi');
const testSuite = goog.require('goog.testing.testSuite');

const LRE = '\u202A';
const RLE = '\u202B';
const PDF = '\u202C';
const LRM = '\u200E';
const RLM = '\u200F';

/**
 * Creates simple object with text and also direction and HTML flags
 * return {Object<string, boolean, boolean>}
 * @private
 */
function SampleItem() {
  /** @suppress {globalThis} suppression added to enable type checking */
  this.text = '';
  /** @suppress {globalThis} suppression added to enable type checking */
  this.isRtl = false;
  /** @suppress {globalThis} suppression added to enable type checking */
  this.isHtml = false;
}

/**
 * Creates an array of BiDi text objects for testing,
 * setting the direction and HTML flags appropriately.
 * @return {!Array<?>}
 * @private
 */
function getBidiTextSamples() {
  const bidiText = [];
  /** @suppress {checkTypes} suppression added to enable type checking */
  let item = new SampleItem;
  item.text = 'Pure Ascii content';
  item.isRtl = false;
  bidiText.push(item);

  /** @suppress {checkTypes} suppression added to enable type checking */
  item = new SampleItem;
  item.text = '\u05d0\u05d9\u05df \u05de\u05de\u05e9 \u05de\u05d4 ' +
      '\u05dc\u05e8\u05d0\u05d5\u05ea: \u05dc\u05d0 ' +
      '\u05e6\u05d9\u05dc\u05de\u05ea\u05d9 \u05d4\u05e8\u05d1\u05d4 ' +
      '\u05d5\u05d2\u05dd \u05d0\u05dd \u05d4\u05d9\u05d9\u05ea\u05d9 ' +
      '\u05de\u05e6\u05dc\u05dd, \u05d4\u05d9\u05d4 \u05e9\u05dd';
  item.isRtl = true;
  bidiText.push(item);

  /** @suppress {checkTypes} suppression added to enable type checking */
  item = new SampleItem;
  item.text = '\u05db\u05d0\u05df - http://geek.co.il/gallery/v/2007-06 - ' +
      '\u05d0\u05d9\u05df \u05de\u05de\u05e9 \u05de\u05d4 ' +
      '\u05dc\u05e8\u05d0\u05d5\u05ea: ' +
      '\u05dc\u05d0 \u05e6\u05d9\u05dc\u05de\u05ea\u05d9 ' +
      '\u05d4\u05e8\u05d1\u05d4 \u05d5\u05d2\u05dd \u05d0\u05dd ' +
      '\u05d4\u05d9\u05d9\u05ea\u05d9 \u05de\u05e6\u05dc\u05dd, ' +
      '\u05d4\u05d9\u05d4 \u05e9\u05dd \u05d1\u05e2\u05d9\u05e7\u05e8 ' +
      '\u05d4\u05e8\u05d1\u05d4 \u05d0\u05e0\u05e9\u05d9\u05dd. ' +
      '\u05de\u05d4 \u05e9\u05db\u05df - \u05d0\u05e4\u05e9\u05e8 ' +
      '\u05dc\u05e0\u05e6\u05dc \u05d0\u05ea ' +
      '\u05d4\u05d4\u05d3\u05d6\u05de\u05e0\u05d5\u05ea ' +
      '\u05dc\u05d4\u05e1\u05ea\u05db\u05dc \u05e2\u05dc \u05db\u05de\u05d4 ' +
      '\u05ea\u05de\u05d5\u05e0\u05d5\u05ea ' +
      '\u05de\u05e9\u05e2\u05e9\u05e2\u05d5\u05ea ' +
      '\u05d9\u05e9\u05e0\u05d5\u05ea \u05d9\u05d5\u05ea\u05e8 ' +
      '\u05e9\u05d9\u05e9 \u05dc\u05d9 \u05d1\u05d0\u05ea\u05e8';
  item.isRtl = true;
  bidiText.push(item);

  /** @suppress {checkTypes} suppression added to enable type checking */
  item = new SampleItem;
  item.text =
      'CAPTCHA \u05de\u05e9\u05d5\u05db\u05dc\u05dc \u05de\u05d3\u05d9?';
  item.isRtl = true;
  bidiText.push(item);

  /** @suppress {checkTypes} suppression added to enable type checking */
  item = new SampleItem;
  item.text = 'Yes Prime Minister \u05e2\u05d3\u05db\u05d5\u05df. ' +
      '\u05e9\u05d0\u05dc\u05d5 \u05d0\u05d5\u05ea\u05d9 \u05de\u05d4 ' +
      '\u05d0\u05e0\u05d9 \u05e8\u05d5\u05e6\u05d4 \u05de\u05ea\u05e0\u05d4 ' +
      '\u05dc\u05d7\u05d2';
  item.isRtl = true;
  bidiText.push(item);

  /** @suppress {checkTypes} suppression added to enable type checking */
  item = new SampleItem;
  item.text = '17.4.02 \u05e9\u05e2\u05d4:13-20 .15-00 .\u05dc\u05d0 ' +
      '\u05d4\u05d9\u05d9\u05ea\u05d9 \u05db\u05d0\u05df.';
  item.isRtl = true;
  bidiText.push(item);

  /** @suppress {checkTypes} suppression added to enable type checking */
  item = new SampleItem;
  item.text = '5710 5720 5730. \u05d4\u05d3\u05dc\u05ea. ' +
      '\u05d4\u05e0\u05e9\u05d9\u05e7\u05d4';
  item.isRtl = true;
  bidiText.push(item);

  /** @suppress {checkTypes} suppression added to enable type checking */
  item = new SampleItem;
  item.text =
      '\u05d4\u05d3\u05dc\u05ea http://www.google.com http://www.gmail.com';
  item.isRtl = true;
  bidiText.push(item);

  /** @suppress {checkTypes} suppression added to enable type checking */
  item = new SampleItem;
  item.text = '&gt;\u05d4&lt;';
  item.isHtml = true;
  item.isRtl = true;
  bidiText.push(item);

  /** @suppress {checkTypes} suppression added to enable type checking */
  item = new SampleItem;
  item.text = '&gt;\u05d4&lt;';
  item.isHtml = false;
  item.isRtl = false;
  bidiText.push(item);

  return bidiText;
}
testSuite({
  testToDir() {
    assertEquals(null, bidi.toDir(null));
    assertEquals(null, bidi.toDir(null, true));

    assertEquals(Dir.NEUTRAL, bidi.toDir(Dir.NEUTRAL));
    assertEquals(null, bidi.toDir(0, true));

    assertEquals(Dir.LTR, bidi.toDir(Dir.LTR));
    assertEquals(Dir.LTR, bidi.toDir(Dir.LTR, true));
    assertEquals(Dir.LTR, bidi.toDir(100));
    assertEquals(Dir.LTR, bidi.toDir(100, true));
    assertEquals(Dir.LTR, bidi.toDir(false));
    assertEquals(Dir.LTR, bidi.toDir(false, true));

    assertEquals(Dir.RTL, bidi.toDir(Dir.RTL));
    assertEquals(Dir.RTL, bidi.toDir(Dir.RTL, true));
    assertEquals(Dir.RTL, bidi.toDir(-100));
    assertEquals(Dir.RTL, bidi.toDir(-100, true));
    assertEquals(Dir.RTL, bidi.toDir(true));
    assertEquals(Dir.RTL, bidi.toDir(true, true));
  },

  testIsRtlLang() {
    assert(!bidi.isRtlLanguage('en'));
    assert(!bidi.isRtlLanguage('fr'));
    assert(!bidi.isRtlLanguage('zh-CN'));
    assert(!bidi.isRtlLanguage('fil'));
    assert(!bidi.isRtlLanguage('az'));
    assert(!bidi.isRtlLanguage('iw-Latn'));
    assert(!bidi.isRtlLanguage('iw-LATN'));
    assert(!bidi.isRtlLanguage('iw_latn'));
    assert(bidi.isRtlLanguage('ar'));
    assert(bidi.isRtlLanguage('AR'));
    assert(bidi.isRtlLanguage('iw'));
    assert(bidi.isRtlLanguage('he'));
    assert(bidi.isRtlLanguage('fa'));
    assert(bidi.isRtlLanguage('ckb'));
    assert(bidi.isRtlLanguage('ckb-IQ'));
    assert(bidi.isRtlLanguage('ar-EG'));
    assert(bidi.isRtlLanguage('az-Arab'));
    assert(bidi.isRtlLanguage('az-ARAB-IR'));
    assert(bidi.isRtlLanguage('az_arab_IR'));
    // New for additions 2018-08-20
    assert(!bidi.isRtlLanguage('ff'));
    assert(!bidi.isRtlLanguage('ff_Latn'));
    assert(!bidi.isRtlLanguage('ff-GN'));
    assert(bidi.isRtlLanguage('ff-arab'));
    assert(bidi.isRtlLanguage('ff_arab'));
    assert(bidi.isRtlLanguage('ff-Arab'));
    assert(bidi.isRtlLanguage('ff_Arab'));
    assert(bidi.isRtlLanguage('ff-adlm'));
    assert(bidi.isRtlLanguage('ff_adlm'));
    assert(bidi.isRtlLanguage('ff-Adlm'));
    assert(bidi.isRtlLanguage('ff_Adlm'));
    // Anything written in Adlam script is RTL
    assert(bidi.isRtlLanguage('ha_Adlm'));

    // Rohnigya script.
    assert(bidi.isRtlLanguage('rhg-Rohg'));
    assert(bidi.isRtlLanguage('rhg_Rohg'));
    // Anything written in Rohingya script.
    assert(bidi.isRtlLanguage('bn_Rohg'));

    // Any writing in Thaana
    assert(bidi.isRtlLanguage('dv-thaa'));
    assert(bidi.isRtlLanguage('dv-Thaa'));
    assert(bidi.isRtlLanguage('chr-thaa'));

    // Test for incomplete script references
    assert(!bidi.isRtlLanguage('ff-adl'));
    assert(!bidi.isRtlLanguage('ff-Lat'));
    assert(!bidi.isRtlLanguage('chr-tha'));
  },

  testIsLtrChar() {
    assert(bidi.isLtrChar('a'));
    assert(!bidi.isLtrChar('\u05e0'));
    const str = 'a\u05e0z';
    assert(bidi.isLtrChar(str.charAt(0)));
    assert(!bidi.isLtrChar(str.charAt(1)));
    assert(bidi.isLtrChar(str.charAt(2)));
    assert(!bidi.isLtrChar('7'));  // Closure treats ASCII digits as neutral.

    // LTR beyond the BMP
    assert(bidi.isLtrChar('\uD804\uDD10'));  // Chakma block
    assert(bidi.isLtrChar('\uD805\uDF00'));  // Ahom block
    assert(bidi.isLtrChar('\uD801\uDCBA'));  // Osage block

    // Unicode 11 additions of LTR scripts
    assert(bidi.isLtrChar('\uD806\uDC00'));  // Dogra block
    assert(bidi.isLtrChar('\uD807\uDC64'));  // Gunjala Gondi
    assert(bidi.isLtrChar('\uD807\uDEE2'));  // Makasar
    assert(bidi.isLtrChar('\uD81B\uDE45'));  // Medefaidrin
  },

  testIsRtlChar() {
    assert(!bidi.isRtlChar('a'));
    assert(bidi.isRtlChar('\u05e0'));
    const str = 'a\u05e0z';
    assert(!bidi.isRtlChar(str.charAt(0)));
    assert(bidi.isRtlChar(str.charAt(1)));
    assert(!bidi.isRtlChar(str.charAt(2)));

    // New for additions 2018-08-20
    assert(bidi.isRtlChar('\u0840'));  // Mandaic
    assert(bidi.isRtlChar('\u085f'));  // Mandaic
    assert(bidi.isRtlChar('\u0800'));  // Samaritan
    assert(bidi.isRtlChar('\u083e'));  // Samaritan

    assert(bidi.isRtlChar('\u0860'));  // Syriac Ext
    assert(bidi.isRtlChar('\u086f'));  // Syriac Ext

    // RTL beyond the BMP
    assert(bidi.isRtlChar('\uD83A\uDD03'));  // Adlam as 2 supplementary points.
    assert(bidi.isRtlChar('\uD83A\uDD03'));  // Adlam
    assert(bidi.isRtlChar('\uD83A\uDD5F'));  // Adlam
    assert(bidi.isRtlChar('\uD83A\uDC00'));  // Mende Kikakui

    // Now the additional scripts
    assert(bidi.isRtlChar('\uD802\uDC00'));  // Cypriot Syllabary
    assert(bidi.isRtlChar('\uD802\uDC3F'));  // Cypriot Syllabary
    assert(bidi.isRtlChar('\uD802\uDC17'));  // Cypriot Syllabary
    assert(bidi.isRtlChar('\uD802\uDD00'));  // Phoenician
    assert(bidi.isRtlChar('\uD802\uDC40'));  // Imperial Aramaic
    assert(bidi.isRtlChar('\uD802\uDC5F'));  // Imperial Aramaic
    assert(bidi.isRtlChar('\uD802\uDE60'));  // Old South Arabian
    assert(bidi.isRtlChar('\uD802\uDE7F'));  // Old South Arabian
    assert(bidi.isRtlChar('\uD802\uDE9F'));  // Old North Arabian
    assert(bidi.isRtlChar('\uD802\uDE91'));  // Old North Arabian
    assert(bidi.isRtlChar('\uD802\uDF60'));  // Inscriptional Pahlavi
    assert(bidi.isRtlChar('\uD802\uDF7F'));  // Inscriptional Pahlavi
    assert(bidi.isRtlChar('\uD802\uDF80'));  // Psalter Pahlavi
    assert(bidi.isRtlChar('\uD802\uDFAF'));  // Psalter Pahlavi
    assert(bidi.isRtlChar('\uD802\uDF00'));  // Avestan
    assert(bidi.isRtlChar('\uD802\uDC80'));  // Nabataean
    assert(bidi.isRtlChar('\uD802\uDCAF'));  // Nabataean
    assert(bidi.isRtlChar('\uD802\uDE00'));  // Kharoshthi
    assert(bidi.isRtlChar('\uD802\uDE5F'));  // Kharoshthi
    assert(bidi.isRtlChar('\uD803\uDC00'));  // Old Turkic
    assert(bidi.isRtlChar('\uD803\uDC4F'));  // Old Turkic
    assert(bidi.isRtlChar('\uD802\uDD20'));  // Lydian
    assert(bidi.isRtlChar('\uD802\uDD3F'));  // Lydian

    // Tests for scripts added in Unicode 11 and beyond
    assert(bidi.isRtlChar('\uD803\uDD00'));  // Rohingya
    assert(bidi.isRtlChar('\uD803\uDD2F'));
    assert(bidi.isRtlChar('\uD803\uDD30'));
    assert(bidi.isRtlChar('\uD803\uDD39'));
    assert(bidi.isRtlChar('\uD803\uDF30'));  // Sogdian
    assert(bidi.isRtlChar('\uD803\uDF51'));  // Sogdian numeral
    assert(bidi.isRtlChar('\uD803\uDF00'));  // Old Sogdian
    assert(bidi.isRtlChar('\uD803\uDF17'));  // Old Sogdian
  },

  testIsNeutralChar() {
    assert(bidi.isNeutralChar('\u0000'));
    assert(bidi.isNeutralChar('\u0020'));
    assert(bidi.isNeutralChar('7'));  // Closure treats ASCII digits as neutral.
    assert(!bidi.isNeutralChar('a'));
    assert(bidi.isNeutralChar('!'));
    assert(bidi.isNeutralChar('@'));
    assert(bidi.isNeutralChar('['));
    assert(bidi.isNeutralChar('`'));
    assert(bidi.isNeutralChar('0'));
    assert(!bidi.isNeutralChar('\u05e0'));
  },

  testIsNeutralText() {
    assert(bidi.isNeutralText('123'));
    assert(!bidi.isNeutralText('abc'));
    assert(bidi.isNeutralText('http://abc'));
    assert(bidi.isNeutralText(' 123-()'));
    assert(!bidi.isNeutralText('123a456'));
    assert(!bidi.isNeutralText('123\u05e0456'));
    assert(!bidi.isNeutralText('<input value=\u05e0>123&lt;', false));
    assert(bidi.isNeutralText('<input value=\u05e0>123&lt;', true));
    assert(bidi.isNeutralText('(123)-4567!'));
    assert(!bidi.isNeutralText('(123)-X4567!'));
    // A few neutral characters from SMP. This is an approximation!
    assert(!bidi.isNeutralText('\uD800\uDD01\uD800\uDD9A\uD834\uDF55'));
  },

  testHasAnyLtr() {
    assert(!bidi.hasAnyLtr(''));
    assert(!bidi.hasAnyLtr('\u05e0\u05e1\u05e2'));
    assert(bidi.hasAnyLtr('\u05e0\u05e1z\u05e2'));
    assert(!bidi.hasAnyLtr('123\t...  \n'));
    assert(bidi.hasAnyLtr('<br>123&lt;', false));
    assert(!bidi.hasAnyLtr('<br>123&lt;', true));
    assert(!bidi.hasAnyLtr('\uD83A\uDD22\uD83A\uDD5E', true));
    assert(bidi.hasAnyLtr('\u05e0\u05e1Q\u05e2\u05e3'));
  },

  testHasAnyRtl() {
    assert(!bidi.hasAnyRtl(''));
    assert(!bidi.hasAnyRtl('abc'));
    assert(bidi.hasAnyRtl('ab\u05e0c'));
    assert(!bidi.hasAnyRtl('123\t...  \n'));
    assert(bidi.hasAnyRtl('<input value=\u05e0>123', false));
    assert(!bidi.hasAnyRtl('<input value=\u05e0>123', true));
    assert(bidi.hasAnyRtl('A\uD83A\uDD22\uD83A\uDD15B', false));
    assert(bidi.hasAnyRtl('\u05e0\u05e1a\u05e2Q\u05e3'));
  },

  testEndsWithLtr() {
    assert(bidi.endsWithLtr('a'));
    assert(bidi.endsWithLtr('abc'));
    assert(bidi.endsWithLtr('a (!)'));
    assert(bidi.endsWithLtr('a.1'));
    assert(bidi.endsWithLtr('http://www.google.com '));
    assert(bidi.endsWithLtr('\u05e0a'));
    assert(bidi.endsWithLtr(' \u05e0\u05e1a\u05e2\u05e3 a (!)'));
    assert(bidi.endsWithLtr('\u202b\u05d0!\u202c\u200e'));
    assert(!bidi.endsWithLtr(''));
    assert(!bidi.endsWithLtr(' '));
    assert(!bidi.endsWithLtr('1'));
    assert(!bidi.endsWithLtr('\u05e0'));
    assert(!bidi.endsWithLtr('\u05e0 1(!)'));
    assert(!bidi.endsWithLtr('a\u05e0'));
    assert(!bidi.endsWithLtr('a abc\u05e0\u05e1def\u05e2. 1'));
    assert(!bidi.endsWithLtr('\u200f\u202eArtielish\u202c\u200f'));
    assert(!bidi.endsWithLtr(' \u05e0\u05e1a\u05e2 &lt;', true));
    assert(bidi.endsWithLtr(' \u05e0\u05e1a\u05e2 &lt;', false));
    assert(!bidi.endsWithLtr('a\uD83A\uDD22\uD83A\uDD5E', false));
    assert(bidi.endsWithLtr('\u05e0\ud804\udf7f', false));
  },

  testEndsWithRtl() {
    assert(bidi.endsWithRtl('\u05e0'));
    assert(bidi.endsWithRtl('\u05e0\u05e1\u05e2'));
    assert(bidi.endsWithRtl('\u05e0 (!)'));
    assert(bidi.endsWithRtl('\u05e0.1'));
    assert(bidi.endsWithRtl('http://www.google.com/\u05e0 '));
    assert(bidi.endsWithRtl('a\u05e0'));
    assert(bidi.endsWithRtl(' a abc\u05e0def\u05e3. 1'));
    assert(bidi.endsWithRtl('\u200f\u202eArtielish\u202c\u200f'));
    assert(!bidi.endsWithRtl(''));
    assert(!bidi.endsWithRtl(' '));
    assert(!bidi.endsWithRtl('1'));
    assert(!bidi.endsWithRtl('a'));
    assert(!bidi.endsWithRtl('a 1(!)'));
    assert(!bidi.endsWithRtl('\u05e0a'));
    assert(!bidi.endsWithRtl('\u202b\u05d0!\u202c\u200e'));
    assert(!bidi.endsWithRtl('\u05e0 \u05e0\u05e1ab\u05e2 a (!)'));
    assert(bidi.endsWithRtl(' \u05e0\u05e1a\u05e2 &lt;', true));
    assert(!bidi.endsWithRtl(' \u05e0\u05e1a\u05e2 &lt;', false));
    assert(!bidi.endsWithRtl('\uD83A\uDD2C\ud801\udc00', false));
    assert(bidi.endsWithRtl('a\uD83A\uDD3A', false));
  },

  testStartsWithLtr() {
    // Note that "startsWithLtr" actually means the first non-neutral character
    // is LTR
    assert(bidi.startsWithLtr('X(123)-4567!'));
    assert(!bidi.startsWithLtr('\u05e0(123)-4567!'));
    assert(!bidi.startsWithLtr('(123)-4567!'));
  },

  testStartsWithRtl() {
    // Note that "startsWithRtl" actually means the first non-neutral character
    // is RTL
    assert(!bidi.startsWithRtl('X(123)-4567!'));
    assert(bidi.startsWithRtl('\u05e0(123)-4567!'));
    assert(!bidi.startsWithRtl('(123)-4567!'));
  },

  testStartsWithNeutral() {
    // Checking that these first characters are not detected as LTR or RTL.
    assert(!bidi.startsWithRtl('(123)-4567!'));
    assert(!bidi.startsWithLtr('(123)-4567!'));

    assert(!bidi.startsWithRtl('@*&~'));
    assert(!bidi.startsWithLtr('@*&~'));

    assert(!bidi.startsWithRtl('\uD800\uDD01\uD800\uDD9A'));
    // These are actually labeled as LTR.
    assert(bidi.startsWithLtr('\uD800\uDD01\uD800\uDD9A'));
  },

  testGuardBracketInText() {
    const strWithRtl = 'asc \u05d0 (\u05d0\u05d0\u05d0)';
    assertEquals(
        'asc \u05d0 \u200f(\u05d0\u05d0\u05d0)\u200f',
        bidi.guardBracketInText(strWithRtl));
    assertEquals(
        'asc \u05d0 \u200f(\u05d0\u05d0\u05d0)\u200f',
        bidi.guardBracketInText(strWithRtl, true));
    assertEquals(
        'asc \u05d0 \u200e(\u05d0\u05d0\u05d0)\u200e',
        bidi.guardBracketInText(strWithRtl, false));

    const strWithRtl2 = '\u05d0 a (asc:))';
    assertEquals(
        '\u05d0 a \u200f(asc:))\u200f', bidi.guardBracketInText(strWithRtl2));
    assertEquals(
        '\u05d0 a \u200f(asc:))\u200f',
        bidi.guardBracketInText(strWithRtl2, true));
    assertEquals(
        '\u05d0 a \u200e(asc:))\u200e',
        bidi.guardBracketInText(strWithRtl2, false));

    const strWithoutRtl = 'a (asc) {{123}}';
    assertEquals(
        'a \u200e(asc)\u200e \u200e{{123}}\u200e',
        bidi.guardBracketInText(strWithoutRtl));
    assertEquals(
        'a \u200f(asc)\u200f \u200f{{123}}\u200f',
        bidi.guardBracketInText(strWithoutRtl, true));
    assertEquals(
        'a \u200e(asc)\u200e \u200e{{123}}\u200e',
        bidi.guardBracketInText(strWithoutRtl, false));
  },

  testEnforceRtlInHtml() {
    let str = '<div> first <br> second </div>';
    assertEquals(
        '<div dir=rtl> first <br> second </div>', bidi.enforceRtlInHtml(str));
    str = 'first second';
    assertEquals(
        '\n<span dir=rtl>first second</span>', bidi.enforceRtlInHtml(str));
  },

  testEnforceRtlInText() {
    const str = 'first second';
    assertEquals(`${RLE}first second${PDF}`, bidi.enforceRtlInText(str));
  },

  testEnforceLtrInHtml() {
    let str = '<div> first <br> second </div>';
    assertEquals(
        '<div dir=ltr> first <br> second </div>', bidi.enforceLtrInHtml(str));
    str = 'first second';
    assertEquals(
        '\n<span dir=ltr>first second</span>', bidi.enforceLtrInHtml(str));
  },

  testEnforceLtrInText() {
    const str = 'first second';
    assertEquals(`${LRE}first second${PDF}`, bidi.enforceLtrInText(str));
  },

  testNormalizeHebrewQuote() {
    assertEquals('\u05d0\u05f4', bidi.normalizeHebrewQuote('\u05d0"'));
    assertEquals('\u05d0\u05f3', bidi.normalizeHebrewQuote('\u05d0\''));
    assertEquals(
        '\u05d0\u05f4\u05d0\u05f3',
        bidi.normalizeHebrewQuote('\u05d0"\u05d0\''));
  },

  testMirrorCSS() {
    let str = 'left:10px;right:20px';
    assertEquals('right:10px;left:20px', bidi.mirrorCSS(str));
    str = 'border:10px 20px 30px 40px';
    assertEquals('border:10px 40px 30px 20px', bidi.mirrorCSS(str));
  },

  testEstimateDirection() {
    assertEquals(Dir.NEUTRAL, bidi.estimateDirection('', false));
    assertEquals(Dir.NEUTRAL, bidi.estimateDirection(' ', false));
    assertEquals(Dir.NEUTRAL, bidi.estimateDirection('! (...)', false));
    assertEquals(Dir.LTR, bidi.estimateDirection('All-Ascii content', false));
    assertEquals(Dir.LTR, bidi.estimateDirection('-17.0%', false));
    assertEquals(
        'Farsi digits should count as weakly LTR', Dir.LTR,
        bidi.estimateDirection('\u06f0', false));
    assertEquals(
        'Farsi digits should count as weakly LTR', Dir.LTR,
        bidi.estimateDirection('\u06f9', false));
    assertEquals(Dir.LTR, bidi.estimateDirection('http://foo/bar/', false));
    assertEquals(
        Dir.LTR,
        bidi.estimateDirection(
            'http://foo/bar/?s=\u05d0\u05d0\u05d0\u05d0\u05d0\u05d0' +
                '\u05d0\u05d0\u05d0\u05d0\u05d0\u05d0\u05d0\u05d0\u05d0' +
                '\u05d0\u05d0\u05d0\u05d0\u05d0\u05d0\u05d0\u05d0\u05d0',
            false));
    assertEquals(Dir.RTL, bidi.estimateDirection('\u05d0', false));
    assertEquals(
        Dir.RTL, bidi.estimateDirection('9 \u05d0 -> 17.5, 23, 45, 19', false));
    assertEquals(
        'Native arabic numbers should count as RTL', Dir.RTL,
        bidi.estimateDirection('\u0660', false));
    assertEquals(
        'Native adlam numbers should count as RTL', Dir.RTL,
        bidi.estimateDirection('\uD83A\uDD55', false));
    assertEquals(
        'Both Farsi letters and digits should count as RTL', Dir.RTL,
        bidi.estimateDirection('\u06CC \u06F1 \u06F2\u06F3', false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            'http://foo/bar/ \u05d0 http://foo2/bar2/ ' +
                'http://foo3/bar3/',
            false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            '\u05d0\u05d9\u05df \u05de\u05de\u05e9 ' +
                '\u05de\u05d4 \u05dc\u05e8\u05d0\u05d5\u05ea: ' +
                '\u05dc\u05d0 \u05e6\u05d9\u05dc\u05de\u05ea\u05d9 ' +
                '\u05d4\u05e8\u05d1\u05d4 \u05d5\u05d2\u05dd \u05d0' +
                '\u05dd \u05d4\u05d9\u05d9\u05ea\u05d9 \u05de\u05e6' +
                '\u05dc\u05dd, \u05d4\u05d9\u05d4 \u05e9\u05dd',
            false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            '\u05db\u05d0 - http://geek.co.il/gallery/v/2007-06' +
                ' - \u05d0\u05d9\u05df \u05de\u05de\u05e9 \u05de\u05d4 ' +
                '\u05dc\u05e8\u05d0\u05d5\u05ea: \u05dc\u05d0 \u05e6' +
                '\u05d9\u05dc\u05de\u05ea\u05d9 \u05d4\u05e8\u05d1 ' +
                '\u05d5\u05d2\u05dd \u05d0\u05dd \u05d4\u05d9\u05d9' +
                '\u05d9 \u05de\u05e6\u05dc\u05dd, \u05d4\u05d9\u05d4 ' +
                '\u05e9\u05dd \u05d1\u05e2\u05d9\u05e7 \u05d4\u05e8' +
                '\u05d1\u05d4 \u05d0\u05e0\u05e9\u05d9\u05dd. \u05de' +
                '\u05d4 \u05e9\u05db\u05df - \u05d0\u05e4\u05e9\u05e8 ' +
                '\u05dc\u05e0\u05e6\u05dc \u05d0\u05ea \u05d4\u05d4 ' +
                '\u05d3\u05d6\u05de\u05e0\u05d5 \u05dc\u05d4\u05e1' +
                '\u05ea\u05db\u05dc \u05e2\u05dc \u05db\u05de\u05d4 ' +
                '\u05ea\u05de\u05d5\u05e0\u05d5\u05ea \u05de\u05e9' +
                '\u05e9\u05e2\u05d5\u05ea \u05d9\u05e9\u05e0\u05d5 ' +
                '\u05d9\u05d5\u05ea\u05e8 \u05e9\u05d9\u05e9 \u05dc' +
                '\u05d9 \u05d1\u05d0\u05ea\u05e8',
            false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            'CAPTCHA \u05de\u05e9\u05d5\u05db\u05dc\u05dc ' +
                '\u05de\u05d3\u05d9?',
            false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            'Yes Prime Minister \u05e2\u05d3\u05db\u05d5\u05df. ' +
                '\u05e9\u05d0\u05dc\u05d5 \u05d0\u05d5\u05ea\u05d9 ' +
                '\u05de\u05d4 \u05d0\u05e0\u05d9 \u05e8\u05d5\u05e6' +
                '\u05d4 \u05de\u05ea\u05e0\u05d4 \u05dc\u05d7\u05d2',
            false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            '17.4.02 \u05e9\u05e2\u05d4:13-20 .15-00 .\u05dc\u05d0 ' +
                '\u05d4\u05d9\u05d9\u05ea\u05d9 \u05db\u05d0\u05df.',
            false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            '5710 5720 5730. \u05d4\u05d3\u05dc\u05ea. ' +
                '\u05d4\u05e0\u05e9\u05d9\u05e7\u05d4',
            false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            '\u05d4\u05d3\u05dc\u05ea http://www.google.com ' +
                'http://www.gmail.com',
            false));
    assertEquals(
        Dir.RTL, bidi.estimateDirection('\u200f\u202eArtielish\u202c\u200f'));
    assertEquals(
        Dir.LTR,
        bidi.estimateDirection(
            '\u05d4\u05d3\u05dc <some quite nasty html mark up>', false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            '\u05d4\u05d3\u05dc <some quite nasty html mark up>', true));
    assertEquals(
        Dir.LTR,
        bidi.estimateDirection(
            '\u05d4\u05d3\u05dc\u05ea &amp; &lt; &gt;', false));
    assertEquals(
        Dir.RTL,
        bidi.estimateDirection(
            '\u05d4\u05d3\u05dc\u05ea &amp; &lt; &gt;', true));
    assertEquals(Dir.LTR, bidi.estimateDirection('foo/<b>\u05d0</b>', true));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testDetectRtlDirectionality() {
    const bidiText = getBidiTextSamples();
    for (let i = 0; i < bidiText.length; i++) {
      // alert(bidiText[i].text);
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      const is_rtl =
          bidi.detectRtlDirectionality(bidiText[i].text, bidiText[i].isHtml);
      if (is_rtl != bidiText[i].isRtl) {
        /**
         * @suppress {strictMissingProperties} suppression added to enable type
         * checking
         */
        const str = '"' + bidiText[i].text + '" should be ' +
            (bidiText[i].isRtl ? 'rtl' : 'ltr') + ' but detected as ' +
            (is_rtl ? 'rtl' : 'ltr');
        alert(str);
      }
      assertEquals(bidiText[i].isRtl, is_rtl);
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSetElementDirByTextDirectionality() {
    const el = document.createElement('DIV');

    let text = '';
    bidi.setElementDirByTextDirectionality(el, text);
    assertEquals('Expected no/empty dir value for empty text.', '', el.dir);

    text = ' ';
    bidi.setElementDirByTextDirectionality(el, text);
    assertEquals(
        `Expected no/empty dir value for neutral text:"${text}"`, '', el.dir);

    text = 'a';
    bidi.setElementDirByTextDirectionality(el, text);
    assertEquals(
        `Expected dir="ltr" value for LTR text:"${text}"`, 'ltr', el.dir);

    text = '\u05d0';
    bidi.setElementDirByTextDirectionality(el, text);
    assertEquals(
        `Expected dir="rtl" value for RTL text:"${text}"`, 'rtl', el.dir);
  },
});
