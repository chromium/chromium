/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.annotateTest');
goog.setTestOnly();

const SafeHtml = goog.require('goog.html.SafeHtml');
const TagName = goog.require('goog.dom.TagName');
const annotate = goog.require('goog.dom.annotate');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

const $ = dom.getElement;

const TEXT = 'This little piggy cried "Wee! Wee! Wee!" all the way home.';

function doAnnotation(termIndex, termHtml) {
  return SafeHtml.create('span', {'class': `c${termIndex}`}, termHtml);
}

testSuite({
  // goog.dom.annotate.annotateText tests
  testAnnotateText() {
    let terms = [['pig', true]];
    let html = annotate.annotateText(TEXT, terms, doAnnotation);
    assertEquals(null, html);

    terms = [['pig', false]];
    html = annotate.annotateText(TEXT, terms, doAnnotation);
    /** @suppress {checkTypes} suppression added to enable type checking */
    html = SafeHtml.unwrap(html);
    assertEquals(
        'This little <span class="c0">pig</span>gy cried ' +
            '&quot;Wee! Wee! Wee!&quot; all the way home.',
        html);

    terms = [[' piggy ', true]];
    html = annotate.annotateText(TEXT, terms, doAnnotation);
    assertEquals(null, html);

    terms = [[' piggy ', false]];
    html = annotate.annotateText(TEXT, terms, doAnnotation);
    /** @suppress {checkTypes} suppression added to enable type checking */
    html = SafeHtml.unwrap(html);
    assertEquals(
        'This little<span class="c0"> piggy </span>cried ' +
            '&quot;Wee! Wee! Wee!&quot; all the way home.',
        html);

    terms = [['goose', true], ['piggy', true]];
    html = annotate.annotateText(TEXT, terms, doAnnotation);
    /** @suppress {checkTypes} suppression added to enable type checking */
    html = SafeHtml.unwrap(html);
    assertEquals(
        'This little <span class="c1">piggy</span> cried ' +
            '&quot;Wee! Wee! Wee!&quot; all the way home.',
        html);
  },

  testAnnotateTextHtmlEscaping() {
    let terms = [['a', false]];
    let html = annotate.annotateText('&a', terms, doAnnotation);
    /** @suppress {checkTypes} suppression added to enable type checking */
    html = SafeHtml.unwrap(html);
    assertEquals('&amp;<span class="c0">a</span>', html);

    terms = [['a', false]];
    html = annotate.annotateText('a&', terms, doAnnotation);
    /** @suppress {checkTypes} suppression added to enable type checking */
    html = SafeHtml.unwrap(html);
    assertEquals('<span class="c0">a</span>&amp;', html);

    terms = [['&', false]];
    html = annotate.annotateText('&', terms, doAnnotation);
    /** @suppress {checkTypes} suppression added to enable type checking */
    html = SafeHtml.unwrap(html);
    assertEquals('<span class="c0">&amp;</span>', html);
  },

  testAnnotateTextIgnoreCase() {
    let terms = [['wEe', true]];
    let html = annotate.annotateText(TEXT, terms, doAnnotation, true);
    /** @suppress {checkTypes} suppression added to enable type checking */
    html = SafeHtml.unwrap(html);
    assertEquals(
        'This little piggy cried &quot;<span class="c0">Wee</span>! ' +
            '<span class="c0">Wee</span>! <span class="c0">Wee</span>!' +
            '&quot; all the way home.',
        html);

    terms = [['WEE!', true], ['HE', false]];
    html = annotate.annotateText(TEXT, terms, doAnnotation, true);
    /** @suppress {checkTypes} suppression added to enable type checking */
    html = SafeHtml.unwrap(html);
    assertEquals(
        'This little piggy cried &quot;<span class="c0">Wee!</span> ' +
            '<span class="c0">Wee!</span> <span class="c0">Wee!</span>' +
            '&quot; all t<span class="c1">he</span> way home.',
        html);
  },

  testAnnotateTextOverlappingTerms() {
    const terms = [['tt', false], ['little', false]];
    let html = annotate.annotateText(TEXT, terms, doAnnotation);
    /** @suppress {checkTypes} suppression added to enable type checking */
    html = SafeHtml.unwrap(html);
    assertEquals(
        'This <span class="c1">little</span> piggy cried &quot;Wee! ' +
            'Wee! Wee!&quot; all the way home.',
        html);
  },

  // goog.dom.annotate.annotateTerms tests
  testAnnotateTerms() {
    let terms = [['pig', true]];
    assertFalse(annotate.annotateTerms($('p'), terms, doAnnotation));
    assertEquals('Tom &amp; Jerry', $('p').innerHTML);

    terms = [['Tom', true]];
    assertTrue(annotate.annotateTerms($('p'), terms, doAnnotation));
    const spans = dom.getElementsByTagNameAndClass(TagName.SPAN, 'c0', $('p'));
    assertEquals(1, spans.length);
    assertEquals('Tom', spans[0].innerHTML);
    assertEquals(' & Jerry', spans[0].nextSibling.nodeValue);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testAnnotateTermsInTable() {
    const terms = [['pig', false]];
    assertTrue(annotate.annotateTerms($('q'), terms, doAnnotation));
    const spans = dom.getElementsByTagNameAndClass(TagName.SPAN, 'c0', $('q'));
    assertEquals(2, spans.length);
    assertEquals('pig', spans[0].innerHTML);
    assertEquals('gy', spans[0].nextSibling.nodeValue);
    assertEquals('pig', spans[1].innerHTML);
    assertEquals(String(TagName.I), spans[1].parentNode.tagName);
  },

  testAnnotateTermsWithClassExclusions() {
    const terms = [['pig', false]];
    const classesToIgnore = ['s'];
    assertTrue(annotate.annotateTerms(
        $('r'), terms, doAnnotation, false, classesToIgnore));
    const spans = dom.getElementsByTagNameAndClass(TagName.SPAN, 'c0', $('r'));
    assertEquals(1, spans.length);
    assertEquals('pig', spans[0].innerHTML);
    assertEquals('gy', spans[0].nextSibling.nodeValue);
  },

  testAnnotateTermsIgnoreCase() {
    const terms1 = [['pig', false]];
    assertTrue(annotate.annotateTerms($('t'), terms1, doAnnotation, true));
    let spans = dom.getElementsByTagNameAndClass(TagName.SPAN, 'c0', $('t'));
    assertEquals(2, spans.length);
    assertEquals('pig', spans[0].innerHTML);
    assertEquals('gy', spans[0].nextSibling.nodeValue);
    assertEquals('Pig', spans[1].innerHTML);

    const terms2 = [['Pig', false]];
    assertTrue(annotate.annotateTerms($('u'), terms2, doAnnotation, true));
    spans = dom.getElementsByTagNameAndClass(TagName.SPAN, 'c0', $('u'));
    assertEquals(2, spans.length);
    assertEquals('pig', spans[0].innerHTML);
    assertEquals('gy', spans[0].nextSibling.nodeValue);
    assertEquals('Pig', spans[1].innerHTML);
  },

  testAnnotateTermsInObject() {
    const terms = [['object', true]];
    assertTrue(annotate.annotateTerms($('o'), terms, doAnnotation));
    const spans = dom.getElementsByTagNameAndClass(TagName.SPAN, 'c0', $('o'));
    assertEquals(1, spans.length);
    assertEquals('object', spans[0].innerHTML);
  },

  testAnnotateTermsInScript() {
    const terms = [['variable', true]];
    assertFalse(annotate.annotateTerms($('script'), terms, doAnnotation));
  },

  testAnnotateTermsInStyle() {
    const terms = [['color', true]];
    assertFalse(annotate.annotateTerms($('style'), terms, doAnnotation));
  },

  testAnnotateTermsInHtmlComment() {
    const terms = [['note', true]];
    assertFalse(annotate.annotateTerms($('comment'), terms, doAnnotation));
  },
});
