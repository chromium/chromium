/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.EnterHandlerTest');
goog.setTestOnly();

const Blockquote = goog.require('goog.editor.plugins.Blockquote');
const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const EnterHandler = goog.require('goog.editor.plugins.EnterHandler');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const Field = goog.require('goog.editor.Field');
const KeyCodes = goog.require('goog.events.KeyCodes');
const MockClock = goog.require('goog.testing.MockClock');
const NodeType = goog.require('goog.dom.NodeType');
const Plugin = goog.require('goog.editor.Plugin');
const Range = goog.require('goog.dom.Range');
const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const editorRange = goog.require('goog.editor.range');
const events = goog.require('goog.events');
const googDom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const testingDom = goog.require('goog.testing.dom');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

let savedHtml;

let field1;
let field2;
let firedDelayedChange;
let firedBeforeChange;
let clock;
let container;
let EXPECTEDFAILURES;

function setUpFields(classnameRequiredToSplitBlockquote) {
  field1 = makeField('field1', classnameRequiredToSplitBlockquote);
  field2 = makeField('field2', classnameRequiredToSplitBlockquote);

  field1.makeEditable();
  field2.makeEditable();
}

/**
 * Selects the node at the given id, and simulates an ENTER keypress.
 * @param {Field} field The field with the node.
 * @param {string} id A DOM id.
 * @return {boolean} Whether preventDefault was called on the event.
 */
function selectNodeAndHitEnter(field, id) {
  const dom = field.getEditableDomHelper();
  const cursor = dom.getElement(id);
  Range.createFromNodeContents(cursor).select();
  return testingEvents.fireKeySequence(cursor, KeyCodes.ENTER);
}

/**
 * Creates a field with only the enter handler plugged in, for testing.
 * @param {string} id A DOM id.
 * @return {!Field} A field.
 */
function makeField(id, classnameRequiredToSplitBlockquote) {
  const field = new Field(id);
  field.registerPlugin(new EnterHandler());
  field.registerPlugin(new Blockquote(classnameRequiredToSplitBlockquote));

  events.listen(field, Field.EventType.BEFORECHANGE, () => {
    // set the global flag that beforechange was fired.
    firedBeforeChange = true;
  });
  events.listen(field, Field.EventType.DELAYEDCHANGE, () => {
    // set the global flag that delayed change was fired.
    firedDelayedChange = true;
  });

  return field;
}

/** Reset all the global flags related to change events. */
function resetChangeFlags() {
  waitForChangeEvents();
  firedBeforeChange = firedDelayedChange = false;
}

/** Asserts that both change flags were fired since the last reset. */
function assertChangeFlags() {
  assert('Beforechange should have fired', firedBeforeChange);
  assert('Delayedchange should have fired', firedDelayedChange);
}

/** Wait for delayedchange to propagate. */
function waitForChangeEvents() {
  clock.tick(Field.DELAYED_CHANGE_FREQUENCY + Field.CHANGE_FREQUENCY);
}

function getNbsp() {
  return '&nbsp;';
}

/**
 * Assert that the prepared contents matches the expected.
 * @suppress {visibility} suppression added to enable type checking
 */
function assertPreparedContents(expected, original) {
  assertEquals(
      expected, field1.reduceOp_(Plugin.Op.PREPARE_CONTENTS_HTML, original));
}

// UTILITY FUNCTION TESTS.

testSuite({
  setUpPage() {
    container = googDom.getElement('container');
  },

  setUp() {
    EXPECTEDFAILURES = new ExpectedFailures();
    savedHtml = googDom.getElement('root').innerHTML;
    clock = new MockClock(true);
  },

  tearDown() {
    clock.dispose();

    EXPECTEDFAILURES.handleTearDown();

    googDom.getElement('root').innerHTML = savedHtml;
  },

  testEnterInNonSetupBlockquote() {
    setUpFields(true);
    resetChangeFlags();
    const prevented = !selectNodeAndHitEnter(field1, 'field1cursor');
    waitForChangeEvents();
    assertChangeFlags();

    // make sure there's just one blockquote, and that the text has been
    // deleted.
    const elem = field1.getElement();
    const dom = field1.getEditableDomHelper();
    try {
      assertEquals(
          'Blockquote should not be split', 1,
          dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)
              .length);
    } catch (e) {
      EXPECTEDFAILURES.handleException(e);
    }
    assert(
        'Selection should be deleted',
        -1 == elem.innerHTML.indexOf('selection'));

    assertEquals(
        'The event should have been prevented only on webkit', prevented,
        userAgent.WEBKIT);
  },

  testEnterInSetupBlockquote() {
    setUpFields(true);
    resetChangeFlags();
    const prevented = !selectNodeAndHitEnter(field2, 'field2cursor');
    waitForChangeEvents();
    assertChangeFlags();

    // make sure there are two blockquotes, and a DIV with nbsp in the middle.
    const elem = field2.getElement();
    const dom = field2.getEditableDomHelper();
    assertEquals(
        'Blockquote should be split', 2,
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)
            .length);
    assert(
        'Selection should be deleted',
        -1 == elem.innerHTML.indexOf('selection'));

    assert(
        'should have div with &nbsp;',
        -1 != elem.innerHTML.indexOf('>' + getNbsp() + '<'));
    assert('event should have been prevented', prevented);
  },

  testEnterInNonSetupBlockquoteWhenClassnameIsNotRequired() {
    setUpFields(false);

    resetChangeFlags();
    const prevented = !selectNodeAndHitEnter(field1, 'field1cursor');
    waitForChangeEvents();
    assertChangeFlags();

    // make sure there are two blockquotes, and a DIV with nbsp in the middle.
    const elem = field1.getElement();
    const dom = field1.getEditableDomHelper();
    assertEquals(
        'Blockquote should be split', 2,
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)
            .length);
    assert(
        'Selection should be deleted',
        -1 == elem.innerHTML.indexOf('selection'));

    assert(
        'should have div with &nbsp;',
        -1 != elem.innerHTML.indexOf('>' + getNbsp() + '<'));
    assert('event should have been prevented', prevented);
  },

  testEnterInBlockquoteCreatesDivInBrMode() {
    setUpFields(true);
    selectNodeAndHitEnter(field2, 'field2cursor');
    const elem = field2.getElement();
    const dom = field2.getEditableDomHelper();

    const firstBlockquote =
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)[0];
    const div = dom.getNextElementSibling(firstBlockquote);
    assertEquals(
        'Element after blockquote should be a div', 'DIV', div.tagName);
    assertEquals(
        'Element after div should be second blockquote', 'BLOCKQUOTE',
        dom.getNextElementSibling(div).tagName);
  },

  /**
   * Tests that breaking after a BR doesn't result in unnecessary newlines.
   * @bug 1471047
   */
  testEnterInBlockquoteRemovesUnnecessaryBrWithCursorAfterBr() {
    setUpFields(true);

    // Assume the following HTML snippet:-
    // <blockquote>one<br>|two<br></blockquote>
    //
    // After enter on the cursor position without the fix, the resulting HTML
    // after the blockquote split was:-
    // <blockquote>one</blockquote>
    // <div>&nbsp;</div>
    // <blockquote><br>two<br></blockquote>
    //
    // This creates the impression on an unnecessary newline. The resulting HTML
    // after the fix is:-
    //
    // <blockquote>one<br></blockquote>
    // <div>&nbsp;</div>
    // <blockquote>two<br></blockquote>
    field1.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<blockquote id="quote" class="tr_bq">one<br>' +
            'two<br></blockquote>'));
    const dom = field1.getEditableDomHelper();
    Range.createCaret(dom.getElement('quote'), 2).select();
    testingEvents.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
    const elem = field1.getElement();
    const secondBlockquote =
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)[1];
    assertHTMLEquals('two<br>', secondBlockquote.innerHTML);

    // Verifies that a blockquote split doesn't happen if it doesn't need to.
    field1.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<blockquote class="tr_bq">one<br id="brcursor"></blockquote>'));
    selectNodeAndHitEnter(field1, 'brcursor');
    assertEquals(
        1,
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)
            .length);
  },

  /**
   * Tests that breaking in a text node before a BR doesn't result in
   * unnecessary newlines.
   * @bug 1471047
   */
  testEnterInBlockquoteRemovesUnnecessaryBrWithCursorBeforeBr() {
    setUpFields(true);

    // Assume the following HTML snippet:-
    // <blockquote>one|<br>two<br></blockquote>
    //
    // After enter on the cursor position, the resulting HTML should be.
    // <blockquote>one<br></blockquote>
    // <div>&nbsp;</div>
    // <blockquote>two<br></blockquote>
    field1.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<blockquote id="quote" class="tr_bq">one<br>' +
            'two<br></blockquote>'));
    const dom = field1.getEditableDomHelper();
    let cursor = dom.getElement('quote').firstChild;
    Range.createCaret(cursor, 3).select();
    testingEvents.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
    const elem = field1.getElement();
    let secondBlockquote =
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)[1];
    assertHTMLEquals('two<br>', secondBlockquote.innerHTML);

    // Ensures that standard text node split works as expected with the new
    // change.
    field1.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<blockquote id="quote" class="tr_bq">one<b>two</b><br>'));
    cursor = dom.getElement('quote').firstChild;
    Range.createCaret(cursor, 3).select();
    testingEvents.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
    secondBlockquote =
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)[1];
    assertHTMLEquals('<b>two</b><br>', secondBlockquote.innerHTML);
  },

  /**
   * Tests that pressing enter in a blockquote doesn't create unnecessary
   * DOM subtrees.
   * @bug 1991539
   * @bug 1991392
   */
  testEnterInBlockquoteRemovesExtraNodes() {
    setUpFields(true);

    // Let's assume we have the following DOM structure and the
    // cursor is placed after the first numbered list item "one".
    //
    // <blockquote class="tr_bq">
    //   <div><div>a</div><ol><li>one|</li></div>
    //   <div>two</div>
    // </blockquote>
    //
    // After pressing enter, we have the following structure.
    //
    // <blockquote class="tr_bq">
    //   <div><div>a</div><ol><li>one|</li></div>
    // </blockquote>
    // <div>&nbsp;</div>
    // <blockquote class="tr_bq">
    //   <div><ol><li><span id=""></span></li></ol></div>
    //   <div>two</div>
    // </blockquote>
    //
    // This appears to the user as an empty list. After the fix, the HTML
    // will be
    //
    // <blockquote class="tr_bq">
    //   <div><div>a</div><ol><li>one|</li></div>
    // </blockquote>
    // <div>&nbsp;</div>
    // <blockquote class="tr_bq">
    //   <div>two</div>
    // </blockquote>
    //
    field1.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<blockquote class="tr_bq">' +
            '<div><div>a</div><ol><li id="cursor">one</li></div>' +
            '<div>b</div>' +
            '</blockquote>'));
    const dom = field1.getEditableDomHelper();
    Range.createCaret(dom.getElement('cursor').firstChild, 3).select();
    testingEvents.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
    const elem = field1.getElement();
    let secondBlockquote =
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)[1];
    assertHTMLEquals('<div>b</div>', secondBlockquote.innerHTML);

    // Ensure that we remove only unnecessary subtrees.
    field1.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<blockquote class="tr_bq">' +
            '<div><span>a</span><div id="cursor">one</div><div>two</div></div>' +
            '<div><span>c</span></div>' +
            '</blockquote>'));
    Range.createCaret(dom.getElement('cursor').firstChild, 3).select();
    testingEvents.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
    secondBlockquote =
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem)[1];
    const expectedHTML = '<div><div>two</div></div>' +
        '<div><span>c</span></div>';
    assertHTMLEquals(expectedHTML, secondBlockquote.innerHTML);

    // Place the cursor in the middle of a line.
    field1.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<blockquote id="quote" class="tr_bq">' +
            '<div>one</div><div>two</div>' +
            '</blockquote>'));
    Range.createCaret(dom.getElement('quote').firstChild.firstChild, 1)
        .select();
    testingEvents.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
    const blockquotes =
        dom.getElementsByTagNameAndClass(TagName.BLOCKQUOTE, null, elem);
    assertEquals(2, blockquotes.length);
    assertHTMLEquals('<div>o</div>', blockquotes[0].innerHTML);
    assertHTMLEquals('<div>ne</div><div>two</div>', blockquotes[1].innerHTML);
  },

  testEnterInList() {
    setUpFields(true);

    // <enter> in a list should *never* be handled by custom code. Lists are
    // just way too complicated to get right.
    field1.setSafeHtml(
        false,
        testing.newSafeHtmlForTest(
            '<ol><li>hi!<span id="field1cursor"></span></li></ol>'));
    const prevented = !selectNodeAndHitEnter(field1, 'field1cursor');
    assertFalse('<enter> in a list should not be prevented', prevented);
  },

  testEnterAtEndOfBlockInWebkit() {
    setUpFields(true);

    if (userAgent.WEBKIT) {
      field1.setSafeHtml(
          false,
          testing.newSafeHtmlForTest(
              '<blockquote>hi!<span id="field1cursor"></span></blockquote>'));

      const cursor = field1.getEditableDomHelper().getElement('field1cursor');
      editorRange.placeCursorNextTo(cursor, false);
      googDom.removeNode(cursor);

      const prevented =
          !testingEvents.fireKeySequence(field1.getElement(), KeyCodes.ENTER);
      waitForChangeEvents();
      assertChangeFlags();
      assert('event should have been prevented', prevented);

      // Make sure that the block now has two brs.
      const elem = field1.getElement();
      assertEquals(
          'should have inserted two br tags: ' + elem.innerHTML, 2,
          googDom.getElementsByTagNameAndClass(TagName.BR, null, elem).length);
    }
  },

  /**
   * Tests that deleting a BR that comes right before a block element works.
   * @bug 1471096
   * @bug 2056376
   */
  testDeleteBrBeforeBlock() {
    setUpFields(true);

    // This test only works on Gecko, because it's testing for manual deletion
    // of BR tags, which is done only for Gecko. For other browsers we fall
    // through and let the browser do the delete, which can only be tested with
    // a robot test (see javascript/apps/editor/tests/delete_br_robot.html).
    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false, testing.newSafeHtmlForTest('one<br><br><div>two</div>'));
      const helper = new TestHelper(field1.getElement());
      helper.select(field1.getElement(), 2);  // Between the two BR's.
      testingEvents.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      assertEquals(
          'Should have deleted exactly one <br>', 'one<br><div>two</div>',
          field1.getElement().innerHTML);

      // We test the case where the BR has a previous sibling which is not
      // a block level element.
      field1.setSafeHtml(
          false, testing.newSafeHtmlForTest('one<br><ul><li>two</li></ul>'));
      helper.select(field1.getElement(), 1);  // Between one and BR.
      testingEvents.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      assertEquals(
          'Should have deleted the <br>', 'one<ul><li>two</li></ul>',
          field1.getElement().innerHTML);
      // Verify that the cursor is placed at the end of the text node "one".
      let range = field1.getRange();
      let focusNode = range.getFocusNode();
      assertTrue('The selected range should be collapsed', range.isCollapsed());
      assertTrue(
          'The focus node should be the text node "one"',
          focusNode.nodeType == NodeType.TEXT && focusNode.data == 'one');
      assertEquals(
          'The focus offset should be at the end of the text node "one"',
          focusNode.length, range.getFocusOffset());
      assertTrue(
          'The next sibling of the focus node should be the UL',
          focusNode.nextSibling && focusNode.nextSibling.tagName == TagName.UL);

      // We test the case where the previous sibling of the BR is a block
      // level element.
      field1.setSafeHtml(
          false,
          testing.newSafeHtmlForTest(
              '<div>foo</div><br><div><span>bar</span></div>'));
      helper.select(field1.getElement(), 1);  // Before the BR.
      testingEvents.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      assertEquals(
          'Should have deleted the <br>',
          '<div>foo</div><div><span>bar</span></div>',
          field1.getElement().innerHTML);
      range = field1.getRange();
      assertEquals(
          'The selected range should be contained within the <span>',
          String(TagName.SPAN), range.getContainerElement().tagName);
      assertTrue('The selected range should be collapsed', range.isCollapsed());
      // Verify that the cursor is placed inside the span at the beginning of
      // bar.
      focusNode = range.getFocusNode();
      assertTrue(
          'The focus node should be the text node "bar"',
          focusNode.nodeType == NodeType.TEXT && focusNode.data == 'bar');
      assertEquals(
          'The focus offset should be at the beginning ' +
              'of the text node "bar"',
          0, range.getFocusOffset());

      // We test the case where the BR does not have a previous sibling.
      field1.setSafeHtml(
          false, testing.newSafeHtmlForTest('<br><ul><li>one</li></ul>'));
      helper.select(field1.getElement(), 0);  // Before the BR.
      testingEvents.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      assertEquals(
          'Should have deleted the <br>', '<ul><li>one</li></ul>',
          field1.getElement().innerHTML);
      range = field1.getRange();
      // Verify that the cursor is placed inside the LI at the text node "one".
      assertEquals(
          'The selected range should be contained within the <li>',
          String(TagName.LI), range.getContainerElement().tagName);
      assertTrue('The selected range should be collapsed', range.isCollapsed());
      focusNode = range.getFocusNode();
      assertTrue(
          'The focus node should be the text node "one"',
          (focusNode.nodeType == NodeType.TEXT && focusNode.data == 'one'));
      assertEquals(
          'The focus offset should be at the beginning of ' +
              'the text node "one"',
          0, range.getFocusOffset());

      // Testing deleting a BR followed by a block level element and preceded
      // by a BR.
      field1.setSafeHtml(
          false, testing.newSafeHtmlForTest('<br><br><ul><li>one</li></ul>'));
      helper.select(field1.getElement(), 1);  // Between the BR's.
      testingEvents.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      assertEquals(
          'Should have deleted the <br>', '<br><ul><li>one</li></ul>',
          field1.getElement().innerHTML);
      // Verify that the cursor is placed inside the LI at the text node "one".
      range = field1.getRange();
      assertEquals(
          'The selected range should be contained within the <li>',
          String(TagName.LI), range.getContainerElement().tagName);
      assertTrue('The selected range should be collapsed', range.isCollapsed());
      focusNode = range.getFocusNode();
      assertTrue(
          'The focus node should be the text node "one"',
          (focusNode.nodeType == NodeType.TEXT && focusNode.data == 'one'));
      assertEquals(
          'The focus offset should be at the beginning of ' +
              'the text node "one"',
          0, range.getFocusOffset());
    }  // End if GECKO
  },

  /**
   * Tests that deleting a BR before a blockquote doesn't remove quoted text.
   * @bug 1471075
   */
  testDeleteBeforeBlockquote() {
    setUpFields(true);

    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false,
          testing.newSafeHtmlForTest(
              '<br><br><div><br><blockquote>foo</blockquote></div>'));
      const helper = new TestHelper(field1.getElement());
      helper.select(field1.getElement(), 0);  // Before the first BR.
      // Fire three deletes in quick succession.
      testingEvents.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      testingEvents.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      testingEvents.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      assertEquals(
          'Should have deleted all the <br>\'s and the blockquote ' +
              'isn\'t affected',
          '<div><blockquote>foo</blockquote></div>',
          field1.getElement().innerHTML);
      const range = field1.getRange();
      assertEquals(
          'The selected range should be contained within the ' +
              '<blockquote>',
          String(TagName.BLOCKQUOTE), range.getContainerElement().tagName);
      assertTrue('The selected range should be collapsed', range.isCollapsed());
      const focusNode = range.getFocusNode();
      assertTrue(
          'The focus node should be the text node "foo"',
          (focusNode.nodeType == NodeType.TEXT && focusNode.data == 'foo'));
      assertEquals(
          'The focus offset should be at the ' +
              'beginning of the text node "foo"',
          0, range.getFocusOffset());
    }
  },

  /**
   * Tests that deleting a BR is working normally (that the workaround for the
   * bug is not causing double deletes).
   * @bug 1471096
   */
  testDeleteBrNormal() {
    setUpFields(true);

    // This test only works on Gecko, because it's testing for manual deletion
    // of BR tags, which is done only for Gecko. For other browsers we fall
    // through and let the browser do the delete, which can only be tested with
    // a robot test (see javascript/apps/editor/tests/delete_br_robot.html).
    if (userAgent.GECKO) {
      field1.setSafeHtml(
          false, testing.newSafeHtmlForTest('one<br><br><br>two'));
      const helper = new TestHelper(field1.getElement());
      helper.select(
          field1.getElement(), 2);  // Between the first and second BR's.
      field1.getElement().focus();
      testingEvents.fireKeySequence(field1.getElement(), KeyCodes.DELETE);
      assertEquals(
          'Should have deleted exactly one <br>', 'one<br><br>two',
          field1.getElement().innerHTML);

    }  // End if GECKO
  },

  /**
   * Tests that deleteCursorSelectionW3C_ correctly recognizes visually
   * collapsed selections in Opera even if they contain a <br>.
   * See the deleteCursorSelectionW3C_ comment in enterhandler.js.
   */
  testCollapsedSelectionKeepsBrOpera() {
    setUpFields(true);
  },

  testPrepareContent() {
    setUpFields(true);
    assertPreparedContents('hi', 'hi');
    assertPreparedContents(
        BrowserFeature.COLLAPSES_EMPTY_NODES ? '<br>' : '', '   ');
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testDeleteW3CSimple() {
    if (BrowserFeature.HAS_W3C_RANGES) {
      container.innerHTML = '<div>abcd</div>';
      const range = Range.createFromNodes(
          container.firstChild.firstChild, 1, container.firstChild.firstChild,
          3);
      range.select();
      EnterHandler.deleteW3cRange_(range);

      testingDom.assertHtmlContentsMatch('<div>ad</div>', container);
    }
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testDeleteW3CAll() {
    if (BrowserFeature.HAS_W3C_RANGES) {
      container.innerHTML = '<div>abcd</div>';
      const range = Range.createFromNodes(
          container.firstChild.firstChild, 0, container.firstChild.firstChild,
          4);
      range.select();
      EnterHandler.deleteW3cRange_(range);

      testingDom.assertHtmlContentsMatch('<div>&nbsp;</div>', container);
    }
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testDeleteW3CPartialEnd() {
    if (BrowserFeature.HAS_W3C_RANGES) {
      container.innerHTML = '<div>ab</div><div>cd</div>';
      const range = Range.createFromNodes(
          container.firstChild.firstChild, 1, container.lastChild.firstChild,
          1);
      range.select();
      EnterHandler.deleteW3cRange_(range);

      testingDom.assertHtmlContentsMatch('<div>ad</div>', container);
    }
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testDeleteW3CNonPartialEnd() {
    if (BrowserFeature.HAS_W3C_RANGES) {
      container.innerHTML = '<div>ab</div><div>cd</div>';
      const range = Range.createFromNodes(
          container.firstChild.firstChild, 1, container.lastChild.firstChild,
          2);
      range.select();
      EnterHandler.deleteW3cRange_(range);

      testingDom.assertHtmlContentsMatch('<div>a</div>', container);
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testIsInOneContainer() {
    if (BrowserFeature.HAS_W3C_RANGES) {
      container.innerHTML = '<div><br></div>';
      const div = container.firstChild;
      const range = Range.createFromNodes(div, 0, div, 1);
      range.select();
      assertTrue(
          'Selection must be recognized as being in one container',
          EnterHandler.isInOneContainerW3c_(range));
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDeletingEndNodesWithNoNewLine() {
    if (BrowserFeature.HAS_W3C_RANGES) {
      container.innerHTML =
          'a<div>b</div><div><br></div><div>c</div><div>d</div>';
      const range = Range.createFromNodes(
          container.childNodes[2], 0, container.childNodes[4].childNodes[0], 1);
      range.select();
      /** @suppress {visibility} suppression added to enable type checking */
      const newRange = EnterHandler.deleteW3cRange_(range);
      testingDom.assertHtmlContentsMatch('a<div>b</div>', container);
      assertTrue(newRange.isCollapsed());
      assertEquals(container, newRange.getStartNode());
      assertEquals(2, newRange.getStartOffset());
    }
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testDeleteW3CRemoveEntireLineWithShiftDown() {
    if (!BrowserFeature.HAS_W3C_RANGES) {
      return;
    }
    container.innerHTML = '<div>a</div><div>b</div><div>cc</div><div>d</div>';
    // anchor: |, focus: ||
    // <div>a</div><div>|b</div><div>||cc</div><div>d</div>
    const range = Range.createFromNodes(
        container.children[1].firstChild, 0, container.children[2], 0);
    range.select();
    EnterHandler.deleteW3cRange_(range);

    // Browsers will add a newline between the a and cc lines, but this doesn't
    // happen in unit tests as we can't trigger the browser's behavior when
    // editing content-editable divs.
    testingDom.assertHtmlContentsMatch(
        '<div>a</div><div>cc</div><div>d</div>', container);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testDeleteW3CRemoveEntireFirstLineWithShiftDown() {
    if (!BrowserFeature.HAS_W3C_RANGES) {
      return;
    }
    container.innerHTML = '<div>a</div><div>b</div><div>cc</div><div>d</div>';
    // anchor: |, focus: ||
    // <div>|a</div><div>||b</div><div>cc</div><div>d</div>
    const range = Range.createFromNodes(
        container.children[0].firstChild, 0, container.children[1], 0);
    range.select();
    EnterHandler.deleteW3cRange_(range);

    // Browsers will add a newline between the a and cc lines, but this doesn't
    // happen in unit tests as we can't trigger the browser's behavior when
    // editing content-editable divs.
    testingDom.assertHtmlContentsMatch(
        '<div>b</div><div>cc</div><div>d</div>', container);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testDeleteW3CRemoveEntireLineWithPartialSecondLine() {
    if (BrowserFeature.HAS_W3C_RANGES) {
      return;
    }
    container.innerHTML = '<div>a</div><div>b</div><div>cc</div><div>d</div>';
    // anchor: |, focus: ||
    // <div>a</div><div>|b</div><div>c||c</div><div>d</div>
    const range = Range.createFromNodes(
        container.children[1].firstChild, 0, container.children[2].firstChild,
        1);
    range.select();
    EnterHandler.deleteW3cRange_(range);

    // Browsers will add a newline between the a and cc lines, but this doesn't
    // happen in unit tests as we can't trigger the browser's behavior when
    // editing content-editable divs.
    testingDom.assertHtmlContentsMatch(
        '<div>a</div><div>c</div><div>d</div>', container);
  },
});
