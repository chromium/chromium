/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for goog.html.util. */

goog.module('goog.html.UtilsTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const googArray = goog.require('goog.array');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');
const utils = goog.require('goog.html.utils');

const FAILURE_MESSAGE = 'Failed to strip all HTML.';
const STRIP = 'Hello world!';
let result;

/**
 * Constructs the HTML of an element from the given tag and content.
 * @param {!TagName} tag The HTML tagName for the element.
 * @param {string} content The content.
 * @param {number=} copies Optional number of copies to make.
 * @param {number=} tabIndex Optional tabIndex to give the element.
 * @param {string=} id Optional id to give the element.
 * @return {string} The HTML of an element from the given tag and content.
 */
function makeHtml(
    tag, content, copies = undefined, tabIndex = undefined, id = undefined) {
  let html = [`<${tag}`, `>${content}</${tag}>`];
  if (typeof tabIndex === 'number') {
    googArray.insertAt(html, ` tabIndex="${tabIndex}"`, 1);
  }
  if (typeof id === 'string') {
    googArray.insertAt(html, ` id="${id}"`, 1);
  }
  html = html.join('');
  const array = [];
  for (let i = 0, length = copies || 1; i < length; i++) {
    array[i] = html;
  }
  return array.join('');
}
testSuite({
  tearDown() {
    result = null;
  },

  testStripAllHtmlTagsSingle() {
    googObject.forEach(TagName, (tag) => {
      if (typeof tag !== 'string') {
        return;
      }

      /** @suppress {checkTypes} suppression added to enable type checking */
      result = utils.stripHtmlTags(makeHtml(tag, STRIP));
      assertEquals(FAILURE_MESSAGE, STRIP, result);
    });
  },

  testStripAllHtmlTagsAttribute() {
    googObject.forEach(TagName, (tag) => {
      if (typeof tag !== 'string') {
        return;
      }

      /** @suppress {checkTypes} suppression added to enable type checking */
      result = utils.stripHtmlTags(makeHtml(tag, STRIP, 1, 0, 'a'));
      assertEquals(FAILURE_MESSAGE, STRIP, result);
    });
  },

  testStripAllHtmlTagsDouble() {
    const tag1 = TagName.B;
    const tag2 = TagName.DIV;
    result = utils.stripHtmlTags(makeHtml(tag1, STRIP, 2));
    assertEquals(FAILURE_MESSAGE, STRIP + STRIP, result);
    result = utils.stripHtmlTags(makeHtml(tag2, STRIP, 2));
    assertEquals(FAILURE_MESSAGE, `${STRIP} ${STRIP}`, result);
  },

  testComplex() {
    const html = '<h1 id=\"life\">Life at Google</h1>' +
        '<p>Read and interact with the information below to learn about ' +
        'life at <u>Google</u>.</p>' +
        '<h2 id=\"food\">Food at Google</h2>' +
        '<p>Google has <em>the best food in the world</em>.</p>' +
        '<h2 id=\"transportation\">Transportation at Google</h2>' +
        '<p>Google provides <i>free transportation</i>.</p>' +
        // Some text with symbols to make sure that it does not get stripped
        '<3i><x>\n-10<x<10 3cat < 3dog &amp;&lt;&gt;&quot;';
    result = utils.stripHtmlTags(html);
    const expected = 'Life at Google ' +
        'Read and interact with the information below to learn about ' +
        'life at Google. ' +
        'Food at Google ' +
        'Google has the best food in the world. ' +
        'Transportation at Google ' +
        'Google provides free transportation. ' +
        '-10<x<10 3cat < 3dog &<>\"';
    assertEquals(FAILURE_MESSAGE, expected, result);
  },

  testInteresting() {
    result = utils.stripHtmlTags(
        '<img/src="bogus"onerror=alert(13) style="display:none">');
    assertEquals(FAILURE_MESSAGE, '', result);
    result = utils.stripHtmlTags(
        '<img o\'reilly blob src=bogus onerror=alert(1337)>');
    assertEquals(FAILURE_MESSAGE, '', result);
  },
});
