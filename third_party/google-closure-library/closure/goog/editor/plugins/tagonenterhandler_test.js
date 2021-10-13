/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.TagOnEnterHandlerTest');
goog.setTestOnly();

const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const Field = goog.require('goog.editor.Field');
const KeyCodes = goog.require('goog.events.KeyCodes');
const NodeType = goog.require('goog.dom.NodeType');
const Plugin = goog.require('goog.editor.Plugin');
const Range = goog.require('goog.dom.Range');
const SafeHtml = goog.require('goog.html.SafeHtml');
const TagName = goog.require('goog.dom.TagName');
const TagOnEnterHandler = goog.require('goog.editor.plugins.TagOnEnterHandler');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const Unicode = goog.require('goog.string.Unicode');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const userAgent = goog.require('goog.userAgent');

let savedHtml;

let editor;
let field1;

/**
 * Assert that the prepared contents matches the expected.
 * @suppress {visibility} suppression added to enable type checking
 */
function assertPreparedContents(expected, original, tag = undefined) {
  const field = makeField('field1', tag);
  field.makeEditable();
  assertEquals(
      expected, field.reduceOp_(Plugin.Op.PREPARE_CONTENTS_HTML, original));
}

/**
 * Selects the node at the given id, and simulates an ENTER keypress.
 * @param {?} field The field with the node.
 * @param {string} id A DOM id.
 * @return {boolean} Whether preventDefault was called on the event.
 */
function selectNodeAndHitEnter(field, id) {
  const cursor = field.getEditableDomHelper().getElement(id);
  Range.createFromNodeContents(cursor).select();
  return events.fireKeySequence(cursor, KeyCodes.ENTER);
}

/**
 * Creates a field with only the enter handler plugged in, for testing.
 * @param {string} id A DOM id.
 * @param {!TagName=} tag The block tag to use. Defaults to P.
 * @return {!Field} A field.
 */
function makeField(id, tag = undefined) {
  const field = new Field(id);
  field.registerPlugin(new TagOnEnterHandler(tag || TagName.P));
  return field;
}

/**
 * Runs a test for splitting the dom.
 * @param {number} offset Index into the text node to split.
 * @param {string} firstHalfString What the html of the first half of the DOM
 *     should be.
 * @param {string} secondHalfString What the html of the 2nd half of the DOM
 *     should be.
 * @param {boolean} isAppend True if the second half should be appended to the
 *     DOM.
 * @param {boolean=} opt_goToRoot True if the root argument for splitDom should
 *     be excluded.
 * @suppress {checkTypes,strictMissingProperties} suppression added to enable
 * type checking
 */
function helpTestSplit(
    offset, firstHalfString, secondHalfString, isAppend, goToBody = undefined) {
  const node = dom.createElement(TagName.DIV);
  node.innerHTML = '<b>begin bold<i>italic</i>end bold</b>';
  document.body.appendChild(node);

  const italic = dom.getElementsByTagName(TagName.I, node)[0].firstChild;

  /** @suppress {visibility} suppression added to enable type checking */
  const splitFn = isAppend ? TagOnEnterHandler.splitDomAndAppend_ :
                             TagOnEnterHandler.splitDom_;
  /** @suppress {checkTypes} suppression added to enable type checking */
  const secondHalf = splitFn(italic, offset, goToBody ? undefined : node);

  if (goToBody) {
    secondHalfString = `<div>${secondHalfString}</div>`;
  }

  assertEquals(
      'original node should have first half of the html', firstHalfString,
      node.innerHTML.toLowerCase().replace(Unicode.NBSP, '&nbsp;'));
  assertEquals(
      'new node should have second half of the html', secondHalfString,
      secondHalf.innerHTML.toLowerCase().replace(Unicode.NBSP, '&nbsp;'));

  if (isAppend) {
    assertTrue(
        'second half of dom should be the original node\'s next' +
            'sibling',
        node.nextSibling == secondHalf);
    dom.removeNode(secondHalf);
  }

  dom.removeNode(node);
}

/**
 * Runs different cases of splitting the DOM.
 * @param {function(number, string, string)} testFn Function that takes an
 *     offset, firstHalfString and secondHalfString as parameters.
 */
function splitDomCases(testFn) {
  testFn(3, '<b>begin bold<i>ita</i></b>', '<b><i>lic</i>end bold</b>');
  testFn(0, '<b>begin bold<i>&nbsp;</i></b>', '<b><i>italic</i>end bold</b>');
  testFn(6, '<b>begin bold<i>italic</i></b>', '<b><i>&nbsp;</i>end bold</b>');
}

testSuite({
  setUp() {
    field1 = makeField('field1');
    field1.makeEditable();
  },

  /**
   * Tests that deleting a BR that comes right before a block element works.
   * @bug 1471096
   */
  testDeleteBrBeforeBlock() {
    // This test only works on Gecko, because it's testing for manual deletion
    // of BR tags, which is done only for Gecko. For other browsers we fall
    // through and let the browser do the delete, which can only be tested with
    // a robot test (see javascript/apps/editor/tests/delete_br_robot.html).
    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false,
          SafeHtml.concat(
              'one', SafeHtml.BR, SafeHtml.BR,
              SafeHtml.create('div', {}, 'two')));
      const helper = new TestHelper(field1.getElement());
      helper.select(field1.getElement(), 2);  // Between the two BR's.
      events.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      assertEquals(
          'Should have deleted exactly one <br>', 'one<br><div>two</div>',
          field1.getElement().innerHTML);

    }  // End if GECKO
  },

  /**
   * Tests that deleting a BR is working normally (that the workaround for the
   * bug is not causing double deletes).
   * @bug 1471096
   */
  testDeleteBrNormal() {
    // This test only works on Gecko, because it's testing for manual deletion
    // of BR tags, which is done only for Gecko. For other browsers we fall
    // through and let the browser do the delete, which can only be tested with
    // a robot test (see javascript/apps/editor/tests/delete_br_robot.html).
    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false,
          SafeHtml.concat('one', SafeHtml.BR, SafeHtml.BR, SafeHtml.BR, 'two'));
      const helper = new TestHelper(field1.getElement());
      helper.select(
          field1.getElement(), 2);  // Between the first and second BR's.
      field1.getElement().focus();
      events.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      assertEquals(
          'Should have deleted exactly one <br>', 'one<br><br>two',
          field1.getElement().innerHTML);

    }  // End if GECKO
  },

  /**
   * Regression test for http://b/1991234 . Tests that when you hit enter and it
   * creates a blank line with whitespace and a BR, the cursor is placed in the
   * whitespace text node instead of the BR, otherwise continuing to type will
   * create adjacent text nodes, which causes browsers to mess up some
   * execcommands. Fix is in a Gecko-only codepath, thus test runs only for
   * Gecko. A full test for the entire sequence that reproed the bug is in
   * javascript/apps/editor/tests/ponenter_robot.html .
   */
  testEnterCreatesBlankLine() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false, SafeHtml.create('p', {}, ['one ', SafeHtml.BR]));
      const helper = new TestHelper(field1.getElement());
      // Place caret after 'one' but keeping a space and a BR as FF does.
      helper.select('one ', 3);
      field1.getElement().focus();
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      const range = field1.getRange();
      assertFalse(
          'Selection should not be in BR tag',
          range.getStartNode().nodeType == NodeType.ELEMENT &&
              range.getStartNode().tagName == TagName.BR);
      assertEquals(
          'Selection should be in text node to avoid creating adjacent' +
              ' text nodes',
          NodeType.TEXT, range.getStartNode().nodeType);
      const rangeStartNode = Range.createFromNodeContents(range.getStartNode());
      assertHTMLEquals(
          'The value of selected text node should be replaced with' +
              '&nbsp;',
          '&nbsp;', rangeStartNode.getHtmlFragment());
    }
  },

  /**
   * Regression test for http://b/3051179 . Tests that when you hit enter and it
   * creates a blank line with a BR and the cursor is placed in P.
   * Splitting DOM causes to make an empty text node. Then if the cursor is
   * placed at the text node the cursor is shown at wrong location. Therefore
   * this test checks that the cursor is not placed at an empty node. Fix is in
   * a Gecko-only codepath, thus test runs only for Gecko.
   */
  testEnterNormalizeNodes() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(false, SafeHtml.create('p', {}, ['one', SafeHtml.BR]));
      const helper = new TestHelper(field1.getElement());
      // Place caret after 'one' but keeping a BR as FF does.
      helper.select('one', 3);
      field1.getElement().focus();
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      const range = field1.getRange();
      assertTrue(
          'Selection should be in P tag',
          range.getStartNode().nodeType == NodeType.ELEMENT &&
              range.getStartNode().tagName == TagName.P);
      assertTrue(
          'Selection should be at the head and collapsed',
          range.getStartOffset() == 0 && range.isCollapsed());
    }
  },

  /**
   * Verifies
   * TagOnEnterHandler.prototype.handleRegularEnterGecko_
   * when we explicitly split anchor elements. This test runs only for Gecko
   * since this is a Gecko-only codepath.
   */
  testEnterAtBeginningOfLink() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(false, SafeHtml.create('a', {'href': '/'}, [
        'b',
        SafeHtml.BR,
      ]));
      const helper = new TestHelper(field1.getElement());
      field1.focusAndPlaceCursorAtStart();
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      helper.assertHtmlMatches('<p>&nbsp;</p><p><a href="/">b<br></a></p>');
    }
  },

  /** Verifies correct handling of pressing enter in an empty list item. */
  testEnterInEmptyListItemInEmptyList() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false,
          SafeHtml.create('ul', {}, SafeHtml.create('li', {}, Unicode.NBSP)));
      const helper = new TestHelper(field1.getElement());
      const li = dom.getElementsByTagName(TagName.LI, field1.getElement())[0];
      helper.select(li.firstChild, 0);
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      helper.assertHtmlMatches('<p>&nbsp;</p>');
    }
  },

  testEnterInEmptyListItemAtBeginningOfList() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false, SafeHtml.create('ul', {'style': {'font-weight': 'bold'}}, [
            SafeHtml.create('li', {}, Unicode.NBSP),
            SafeHtml.create('li', {}, '1'),
            SafeHtml.create('li', {}, '2'),
          ]));
      const helper = new TestHelper(field1.getElement());
      const li = dom.getElementsByTagName(TagName.LI, field1.getElement())[0];
      helper.select(li.firstChild, 0);
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      helper.assertHtmlMatches(
          '<p>&nbsp;</p><ul style="font-weight: bold"><li>1</li><li>2</li></ul>');
    }
  },

  testEnterInEmptyListItemAtEndOfList() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false, SafeHtml.create('ul', {'style': {'font-weight': 'bold'}}, [
            SafeHtml.create('li', {}, '1'),
            SafeHtml.create('li', {}, '2'),
            SafeHtml.create('li', {}, Unicode.NBSP),
          ]));
      const helper = new TestHelper(field1.getElement());
      const li = dom.getElementsByTagName(TagName.LI, field1.getElement())[2];
      helper.select(li.firstChild, 0);
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      helper.assertHtmlMatches(
          '<ul style="font-weight: bold"><li>1</li><li>2</li></ul><p>&nbsp;</p>');
    }
  },

  testEnterInEmptyListItemInMiddleOfList() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false, SafeHtml.create('ul', {'style': {'font-weight': 'bold'}}, [
            SafeHtml.create('li', {}, '1'),
            SafeHtml.create('li', {}, Unicode.NBSP),
            SafeHtml.create('li', {}, '2'),
          ]));
      const helper = new TestHelper(field1.getElement());
      const li = dom.getElementsByTagName(TagName.LI, field1.getElement())[1];
      helper.select(li.firstChild, 0);
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      helper.assertHtmlMatches(
          '<ul style="font-weight: bold"><li>1</li></ul>' +
          '<p>&nbsp;</p>' +
          '<ul style="font-weight: bold"><li>2</li></ul>');
    }
  },

  testEnterInEmptyListItemInSublist() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(false, SafeHtml.create('ul', {}, [
        SafeHtml.create('li', {}, 'A'),
        SafeHtml.create(
            'ul', {'style': {'font-weight': 'bold'}},
            [
              SafeHtml.create('li', {}, '1'),
              SafeHtml.create('li', {}, Unicode.NBSP),
              SafeHtml.create('li', {}, '2'),
            ]),
        SafeHtml.create('li', {}, 'B'),
      ]));
      const helper = new TestHelper(field1.getElement());
      const li = dom.getElementsByTagName(TagName.LI, field1.getElement())[2];
      helper.select(li.firstChild, 0);
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      helper.assertHtmlMatches(
          '<ul>' +
          '<li>A</li>' +
          '<ul style="font-weight: bold"><li>1</li></ul>' +
          '<li>&nbsp;</li>' +
          '<ul style="font-weight: bold"><li>2</li></ul>' +
          '<li>B</li>' +
          '</ul>');
    }
  },

  testEnterInEmptyListItemAtBeginningOfSublist() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(false, SafeHtml.create('ul', {}, [
        SafeHtml.create('li', {}, 'A'),
        SafeHtml.create(
            'ul', {'style': {'font-weight': 'bold'}},
            [
              SafeHtml.create('li', {}, Unicode.NBSP),
              SafeHtml.create('li', {}, '1'),
              SafeHtml.create('li', {}, '2'),
            ]),
        SafeHtml.create('li', {}, 'B'),
      ]));
      const helper = new TestHelper(field1.getElement());
      const li = dom.getElementsByTagName(TagName.LI, field1.getElement())[1];
      helper.select(li.firstChild, 0);
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      helper.assertHtmlMatches(
          '<ul>' +
          '<li>A</li>' +
          '<li>&nbsp;</li>' +
          '<ul style="font-weight: bold"><li>1</li><li>2</li></ul>' +
          '<li>B</li>' +
          '</ul>');
    }
  },

  testEnterInEmptyListItemAtEndOfSublist() {
    if (userAgent.GECKO) {
      field1.setSafeHtml(false, SafeHtml.create('ul', {}, [
        SafeHtml.create('li', {}, 'A'),
        SafeHtml.create(
            'ul', {'style': {'font-weight': 'bold'}},
            [
              SafeHtml.create('li', {}, '1'),
              SafeHtml.create('li', {}, '2'),
              SafeHtml.create('li', {}, Unicode.NBSP),
            ]),
        SafeHtml.create('li', {}, 'B'),
      ]));
      const helper = new TestHelper(field1.getElement());
      const li = dom.getElementsByTagName(TagName.LI, field1.getElement())[3];
      helper.select(li.firstChild, 0);
      events.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      helper.assertHtmlMatches(
          '<ul>' +
          '<li>A</li>' +
          '<ul style="font-weight: bold"><li>1</li><li>2</li></ul>' +
          '<li>&nbsp;</li>' +
          '<li>B</li>' +
          '</ul>');
    }
  },

  testPrepareContentForPOnEnter() {
    assertPreparedContents('hi', 'hi');
    assertPreparedContents(
        BrowserFeature.COLLAPSES_EMPTY_NODES ? '<p>&nbsp;</p>' : '', '   ');
  },

  testPrepareContentForDivOnEnter() {
    assertPreparedContents('hi', 'hi', TagName.DIV);
    assertPreparedContents(
        BrowserFeature.COLLAPSES_EMPTY_NODES ? '<div><br></div>' : '', '   ',
        TagName.DIV);
  },

  testSplitDom() {
    splitDomCases((offset, firstHalfString, secondHalfString) => {
      helpTestSplit(offset, firstHalfString, secondHalfString, false, true);
      helpTestSplit(offset, firstHalfString, secondHalfString, false, false);
    });
  },

  testSplitDomAndAppend() {
    splitDomCases((offset, firstHalfString, secondHalfString) => {
      helpTestSplit(offset, firstHalfString, secondHalfString, true, false);
    });
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSplitDomAtElement() {
    const node = dom.createElement(TagName.DIV);
    node.innerHTML = '<div>abc<br>def</div>';
    document.body.appendChild(node);

    TagOnEnterHandler.splitDomAndAppend_(node.firstChild, 1, node.firstChild);

    testingDom.assertHtmlContentsMatch(
        '<div>abc</div><div><br>def</div>', node);

    dom.removeNode(node);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSplitDomAtElementStart() {
    const node = dom.createElement(TagName.DIV);
    node.innerHTML = '<div>abc<br>def</div>';
    document.body.appendChild(node);

    TagOnEnterHandler.splitDomAndAppend_(node.firstChild, 0, node.firstChild);

    testingDom.assertHtmlContentsMatch(
        '<div></div><div>abc<br>def</div>', node);

    dom.removeNode(node);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSplitDomAtChildlessElement() {
    const node = dom.createElement(TagName.DIV);
    node.innerHTML = '<div>abc<br>def</div>';
    document.body.appendChild(node);

    const br = dom.getElementsByTagName(TagName.BR, node)[0];
    TagOnEnterHandler.splitDomAndAppend_(br, 0, node.firstChild);

    testingDom.assertHtmlContentsMatch(
        '<div>abc</div><div><br>def</div>', node);

    dom.removeNode(node);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testReplaceWhiteSpaceWithNbsp() {
    const node = dom.createElement(TagName.DIV);
    const textNode = document.createTextNode('');
    node.appendChild(textNode);

    textNode.nodeValue = ' test ';
    TagOnEnterHandler.replaceWhiteSpaceWithNbsp_(node.firstChild, true, false);
    assertHTMLEquals('&nbsp;test ', node.innerHTML);

    textNode.nodeValue = '  test ';
    TagOnEnterHandler.replaceWhiteSpaceWithNbsp_(node.firstChild, true, false);
    assertHTMLEquals('&nbsp;test ', node.innerHTML);

    textNode.nodeValue = ' test ';
    TagOnEnterHandler.replaceWhiteSpaceWithNbsp_(node.firstChild, false, false);
    assertHTMLEquals(' test&nbsp;', node.innerHTML);

    textNode.nodeValue = ' test  ';
    TagOnEnterHandler.replaceWhiteSpaceWithNbsp_(node.firstChild, false, false);
    assertHTMLEquals(' test&nbsp;', node.innerHTML);

    textNode.nodeValue = '';
    TagOnEnterHandler.replaceWhiteSpaceWithNbsp_(node.firstChild, false, false);
    assertHTMLEquals('&nbsp;', node.innerHTML);

    textNode.nodeValue = '';
    TagOnEnterHandler.replaceWhiteSpaceWithNbsp_(node.firstChild, false, true);
    assertHTMLEquals('', node.innerHTML);
  },
});
