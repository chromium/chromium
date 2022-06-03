/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ac.AutoCompleteTest');
goog.setTestOnly();

const AutoComplete = goog.require('goog.ui.ac.AutoComplete');
const EventHandler = goog.require('goog.events.EventHandler');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const InputHandler = goog.require('goog.ui.ac.InputHandler');
const InputType = goog.require('goog.dom.InputType');
const MockControl = goog.require('goog.testing.MockControl');
const RenderOptions = goog.require('goog.ui.ac.RenderOptions');
const Renderer = goog.require('goog.ui.ac.Renderer');
const Role = goog.require('goog.a11y.aria.Role');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const googString = goog.require('goog.string');
const mockmatchers = goog.require('goog.testing.mockmatchers');
const testSuite = goog.require('goog.testing.testSuite');

/** Mock DataStore */
class MockDS {
  constructor(autoHilite = undefined) {
    this.autoHilite_ = autoHilite;
    const disabledRow = {
      match: function(str) {
        return this.text.match(str);
      },
      rowDisabled: true,
      text: 'hello@u.edu',
    };
    this.rows_ = [
      '"Slartibartfast Theadore" <fjordmaster@magrathea.com>',
      '"Zaphod Beeblebrox" <theprez@universe.gov>',
      '"Ford Prefect" <ford@theguide.com>',
      '"Arthur Dent" <has.no.tea@gmail.com>',
      '"Marvin The Paranoid Android" <marv@googlemail.com>',
      'the.mice@magrathea.com',
      'the.mice@myotherdomain.com',
      'hello@a.com',
      disabledRow,
      'row@u.edu',
      'person@a.edu',
    ];
    this.isRowDisabled = (row) => !!row.rowDisabled;
  }

  requestMatchingRows(token, maxMatches, matchHandler) {
    const escapedToken = googString.regExpEscape(token);
    const matcher = new RegExp('(^|\\W+)' + escapedToken);
    const matches = [];
    for (let i = 0; i < this.rows_.length && matches.length < maxMatches; ++i) {
      const row = this.rows_[i];
      if (row.match(matcher)) {
        matches.push(row);
      }
    }
    if (this.autoHilite_ === undefined) {
      matchHandler(token, matches);
    } else {
      const options = new RenderOptions();
      options.setAutoHilite(this.autoHilite_);
      matchHandler(token, matches, options);
    }
  }
}

/** Mock Selection Handler */

function MockSelect() {}
goog.inherits(MockSelect, GoogEventTarget);

MockSelect.prototype.selectRow = function(row) {
  this.selectedRow = row;
};

/** Renderer subclass that exposes additional private members for testing. */
class TestRend extends Renderer {
  constructor() {
    super(dom.getElement('test-area'));
  }

  /** @suppress {visibility} suppression added to enable type checking */
  getRenderedRows() {
    return this.rows_;
  }

  getHilitedRowIndex() {
    return this.hilitedRow_;
  }

  getHilitedRowDiv() {
    return this.rowDivs_[this.hilitedRow_];
  }

  getRowDiv(index) {
    return this.rowDivs_[index];
  }
}

let handler;
let inputElement;
let mockControl;

function checkHilitedIndex(renderer, index) {
  assertEquals(index, renderer.getHilitedRowIndex());
}

testSuite({
  setUp() {
    inputElement = dom.createDom(TagName.INPUT, {type: InputType.TEXT});
    handler = new EventHandler();
    mockControl = new MockControl();
  },

  tearDown() {
    handler.dispose();
    mockControl.$tearDown();
    dom.removeChildren(dom.getElement('test-area'));
  },

  /** Make sure results are truncated (or not) by setMaxMatches. */
  testMaxMatches() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);

    ac.setMaxMatches(2);
    ac.setToken('the');
    assertEquals(2, rend.getRenderedRows().length);
    ac.setToken('');

    ac.setMaxMatches(3);
    ac.setToken('the');
    assertEquals(3, rend.getRenderedRows().length);
    ac.setToken('');

    ac.setMaxMatches(1000);
    ac.setToken('the');
    assertEquals(4, rend.getRenderedRows().length);
    ac.setToken('');
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHiliteViaMouse() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    let updates = 0;
    const row = null;
    let rowNode = null;
    handler.listen(rend, AutoComplete.EventType.ROW_HILITE, (evt) => {
      updates++;
      /**
       * @suppress {missingProperties} suppression added to enable type
       * checking
       */
      rowNode = evt.rowNode;
    });
    const ac = new AutoComplete(ds, rend, select);
    ac.setMaxMatches(4);
    ac.setToken('the');
    // Need to set the startRenderingRows_ time to something long ago,
    // otherwise the mouse event will not be fired.  (The autocomplete logic
    // waits for some time to pass after rendering before firing mouseover
    // events.)
    /** @suppress {visibility} suppression added to enable type checking */
    rend.startRenderingRows_ = -1;
    const hilitedRowDiv = rend.getRowDiv(3);
    events.fireMouseOverEvent(hilitedRowDiv);
    assertEquals(2, updates);
    assertTrue(
        googString.contains(rowNode.innerHTML, 'mice@myotherdomain.com'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMouseClickBeforeHilite() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setMaxMatches(4);
    ac.setToken('the');
    // Need to set the startRenderingRows_ time to something long ago,
    // otherwise the mouse event will not be fired.  (The autocomplete logic
    // waits for some time to pass after rendering before firing mouseover
    // events.)
    /** @suppress {visibility} suppression added to enable type checking */
    rend.startRenderingRows_ = -1;

    // hilite row 3...
    const hilitedRowDiv = rend.getRowDiv(3);
    events.fireMouseOverEvent(hilitedRowDiv);

    // but click row 2, to simulate mouse getting ahead of focus.
    const targetRowDiv = rend.getRowDiv(2);
    events.fireClickEvent(targetRowDiv);

    assertEquals('the.mice@magrathea.com', select.selectedRow);
  },

  testMouseClickOnFirstRowBeforeHilite() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setAutoHilite(false);
    ac.setMaxMatches(4);
    ac.setToken('the');

    // Click the first row before highlighting it, to simulate mouse getting
    // ahead of focus.
    const targetRowDiv = rend.getRowDiv(0);
    events.fireClickEvent(targetRowDiv);

    assertEquals(
        '"Zaphod Beeblebrox" <theprez@universe.gov>', select.selectedRow);
  },

  testMouseClickOnRowAfterBlur() {
    const ds = new MockDS();
    const rend = new TestRend();
    const ih = new InputHandler();
    ih.attachInput(inputElement);

    const ac = new AutoComplete(ds, rend, ih);
    events.fireFocusEvent(inputElement);
    ac.setToken('the');
    const targetRowDiv = rend.getRowDiv(0);

    // Simulate the user clicking on an autocomplete row in the short time
    // between blur and autocomplete dismissal.
    events.fireBlurEvent(inputElement);
    assertNotThrows(() => {
      events.fireClickEvent(targetRowDiv);
    });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSelectEventEmptyRow() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setMaxMatches(4);
    ac.setToken('the');
    /** @suppress {visibility} suppression added to enable type checking */
    rend.startRenderingRows_ = -1;

    // hilight row 2 ('the.mice@...')
    const hilitedRowDiv = rend.getRowDiv(2);
    events.fireMouseOverEvent(hilitedRowDiv);
    assertUndefined(select.selectedRow);

    // Dispatch an event that does not specify a row.
    rend.dispatchEvent({type: AutoComplete.EventType.SELECT, row: ''});

    assertEquals('the.mice@magrathea.com', select.selectedRow);
  },

  testSuggestionsUpdateEvent() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    let updates = 0;
    handler.listen(ac, AutoComplete.EventType.SUGGESTIONS_UPDATE, () => {
      updates++;
    });

    ac.setToken('the');
    assertEquals(1, updates);

    ac.setToken('beeb');
    assertEquals(2, updates);

    ac.setToken('ford');
    assertEquals(3, updates);

    ac.dismiss();
    assertEquals(4, updates);

    ac.setToken('dent');
    assertEquals(5, updates);
  },

  testGetRowCount() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    assertEquals(0, ac.getRowCount());

    ac.setToken('Zaphod');
    assertEquals(1, ac.getRowCount());

    ac.setMaxMatches(2);
    ac.setToken('the');
    assertEquals(2, ac.getRowCount());
  },

  /**
   * Try using next and prev to navigate past the ends with default behavior
   * of allowFreeSelect_ and wrap_.
   */
  testHiliteNextPrev_default() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);

    let updates = 0;
    handler.listen(rend, AutoComplete.EventType.ROW_HILITE, () => {
      updates++;
    });

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('the');
      assertEquals(4, rend.getRenderedRows().length);
      // check to see if we can select the last of the 4 items
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      ac.hiliteNext();
      checkHilitedIndex(rend, 3);
      // try going over the edge
      ac.hiliteNext();
      checkHilitedIndex(rend, 3);

      // go back down
      ac.hilitePrev();
      checkHilitedIndex(rend, 2);
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
    }
    // 21 changes in the loop above (3 * 7)
    assertEquals(21, updates);
  },

  /**
   * Try using next and prev to navigate past the ends with default behavior
   * of allowFreeSelect_ and wrap_ and with a disabled first row.
   */
  testHiliteNextPrevWithDisabledFirstRow_default() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);

    let updates = 0;
    handler.listen(rend, AutoComplete.EventType.ROW_HILITE, () => {
      updates++;
    });

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(3);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled first row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('edu');
      assertEquals(3, rend.getRenderedRows().length);
      // The first row is disabled, second should be highlighted.
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      // try going over the edge
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);

      // go back down
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
      // First row is disabled, make sure we don't highlight it.
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
    }
    // 9 changes in the loop above (3 * 3)
    assertEquals(9, updates);
  },

  /**
   * Try using next and prev to navigate past the ends with default behavior
   * of allowFreeSelect_ and wrap_ and with a disabled middle row.
   */
  testHiliteNextPrevWithDisabledMiddleRow_default() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);

    let updates = 0;
    handler.listen(rend, AutoComplete.EventType.ROW_HILITE, () => {
      updates++;
    });

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(3);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled middle row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('u');
      assertEquals(3, rend.getRenderedRows().length);
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      // Second row is disabled and should be skipped.
      checkHilitedIndex(rend, 2);
      // try going over the edge
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);

      // go back down
      ac.hilitePrev();
      // Second row is disabled, make sure we don't highlight it.
      checkHilitedIndex(rend, 0);
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
    }
    // 9 changes in the loop above (3 * 3)
    assertEquals(9, updates);
  },

  /**
   * Try using next and prev to navigate past the ends with default behavior
   * of allowFreeSelect_ and wrap_ and with a disabled last row.
   */
  testHiliteNextPrevWithDisabledLastRow_default() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);

    let updates = 0;
    handler.listen(rend, AutoComplete.EventType.ROW_HILITE, () => {
      updates++;
    });

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(3);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled last row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('h');
      assertEquals(3, rend.getRenderedRows().length);
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      // try going over the edge since last row is disabled
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back down
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
    }
    // 9 changes in the loop above (3 * 3)
    assertEquals(9, updates);
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ off and
   * allowFreeSelect_ on.
   */
  testHiliteNextPrev_allowFreeSelect() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setAllowFreeSelect(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('the');
      assertEquals(4, rend.getRenderedRows().length);
      // check to see if we can select the last of the 4 items
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      ac.hiliteNext();
      checkHilitedIndex(rend, 3);
      // try going over the edge. Since allowFreeSelect is on, this will
      // deselect the last row.
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, deselects first.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ off and
   * allowFreeSelect_ on, and a disabled first row.
   */
  testHiliteNextPrevWithDisabledFirstRow_allowFreeSelect() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setAllowFreeSelect(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled first row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('edu');
      assertEquals(3, rend.getRenderedRows().length);
      // The first row is disabled, second should be highlighted.
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      // Try going over the edge. Since allowFreeSelect is on, this will
      // deselect the last row.
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list, first row is disabled
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
      // first is disabled, so deselect the second.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ off and
   * allowFreeSelect_ on, and a disabled middle row.
   */
  testHiliteNextPrevWithDisabledMiddleRow_allowFreeSelect() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setAllowFreeSelect(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled middle row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('u');
      assertEquals(3, rend.getRenderedRows().length);
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      // Second row is disabled and should be skipped.
      checkHilitedIndex(rend, 2);
      // try going over the edge. Since allowFreeSelect is on, this will
      // deselect the last row.
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, deselects first.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ off and
   * allowFreeSelect_ on, and a disabled last row.
   */
  testHiliteNextPrevWithDisabledLastRow_allowFreeSelect() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setAllowFreeSelect(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled last row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('h');
      assertEquals(3, rend.getRenderedRows().length);
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      // try going over the edge since last row is disabled. Since
      // allowFreeSelect is on, this will deselect the last row.
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, deselects first.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ off.
   */
  testHiliteNextPrev_wrap() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('the');
      assertEquals(4, rend.getRenderedRows().length);
      // check to see if we can select the last of the 4 items
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      ac.hiliteNext();
      checkHilitedIndex(rend, 3);
      // try going over the edge. Since wrap is on, this will go back to 0.
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, selects last.
      ac.hilitePrev();
      checkHilitedIndex(rend, 3);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ off and a disabled first row.
   */
  testHiliteNextPrevWithDisabledFirstRow_wrap() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled first row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('edu');
      assertEquals(3, rend.getRenderedRows().length);
      // The first row is disabled, second should be highlighted.
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      // try going over the edge. Since wrap is on and first row is
      // disabled, this will go back to 1.
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
      // first is disabled, so wrap and select the last.
      ac.hilitePrev();
      checkHilitedIndex(rend, 2);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ off and a disabled middle row.
   */
  testHiliteNextPrevWithDisabledMiddleRow_wrap() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled middle row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('u');
      assertEquals(3, rend.getRenderedRows().length);
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      // Second row is disabled and should be skipped.
      checkHilitedIndex(rend, 2);
      // try going over the edge. Since wrap is on, this will go back to 0.
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, selects last.
      ac.hilitePrev();
      checkHilitedIndex(rend, 2);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ off and a disabled last row.
   */
  testHiliteNextPrevWithDisabledLastRow_wrap() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled last row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('h');
      assertEquals(3, rend.getRenderedRows().length);
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      // try going over the edge since last row is disabled. Since wrap is
      // on, this will go back to 0.
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, since wrap is on and last row is disabled,
      // this will select the second last.
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ on.
   */
  testHiliteNextPrev_wrapAndAllowFreeSelect() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('the');
      assertEquals(4, rend.getRenderedRows().length);
      // check to see if we can select the last of the 4 items
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      ac.hiliteNext();
      checkHilitedIndex(rend, 3);
      // try going over the edge. Since free select is on, this should go
      // to -1.
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, free select.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
      // wrap to last
      ac.hilitePrev();
      checkHilitedIndex(rend, 3);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ on and a disabled first row.
   */
  testHiliteNextPrevWithDisabledFirstRow_wrapAndAllowFreeSelect() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled first row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('edu');
      assertEquals(3, rend.getRenderedRows().length);
      // The first row is disabled, second should be highlighted.
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      // try going over the edge. Since free select is on, this should go to
      // -1.
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list, fist row is disabled
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
      // go back above the first, free select.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
      // wrap to last
      ac.hilitePrev();
      checkHilitedIndex(rend, 2);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ on and a disabled middle row.
   */
  testHiliteNextPrevWithDisabledMiddleRow_wrapAndAllowFreeSelect() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled middle row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('u');
      assertEquals(3, rend.getRenderedRows().length);
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      // Second row is disabled and should be skipped.
      checkHilitedIndex(rend, 2);
      // try going over the edge. Since free select is on, this should go to
      // -1
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, free select.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
      // wrap to last
      ac.hilitePrev();
      checkHilitedIndex(rend, 2);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ on and a disabled last row.
   */
  testHiliteNextPrevWithDisabledLastRow_wrapAndAllowFreeSelect() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled last row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('h');
      assertEquals(3, rend.getRenderedRows().length);
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      // try going over the edge since last row is disabled. Since free
      // select is on, this should go to -1
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, free select.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
      // wrap to the second last, since last is disabled.
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ on AND turn autoHilite_ off.
   */
  testHiliteNextPrev_wrapAndAllowFreeSelectNoAutoHilite() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);
    ac.setAutoHilite(false);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('the');
      assertEquals(4, rend.getRenderedRows().length);
      // check to see if we can select the last of the 4 items.
      // Initially nothing should be selected since autoHilite_ is off.
      checkHilitedIndex(rend, -1);
      ac.hilitePrev();
      checkHilitedIndex(rend, 3);
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      ac.hiliteNext();
      checkHilitedIndex(rend, 3);
      // try going over the edge. Since free select is on, this should go
      // to -1.
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, free select.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
      // wrap to last
      ac.hilitePrev();
      checkHilitedIndex(rend, 3);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ on AND turn autoHilite_ off, and a disabled first row.
   */
  testHiliteNextPrevWithDisabledFirstRow_wrapAndAllowFreeSelectNoAutoHilite() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);
    ac.setAutoHilite(false);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled first row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('edu');
      assertEquals(3, rend.getRenderedRows().length);
      // Initially nothing should be selected since autoHilite_ is off.
      checkHilitedIndex(rend, -1);
      ac.hilitePrev();
      checkHilitedIndex(rend, 2);
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);
      ac.hiliteNext();
      // First row is disabled.
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);
      // try going over the edge. Since free select is on, this should go to
      // -1
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list, first row is disabled
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 2);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
      // go back above the first, free select.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
      // wrap to last
      ac.hilitePrev();
      checkHilitedIndex(rend, 2);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ on AND turn autoHilite_ off, and a disabled middle
   * row.
   */
  testHiliteNextPrevWithDisabledMiddleRow_wrapAndAllowFreeSelectNoAutoHilite() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);
    ac.setAutoHilite(false);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled middle row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('u');
      assertEquals(3, rend.getRenderedRows().length);
      // Initially nothing should be selected since autoHilite_ is off.
      checkHilitedIndex(rend, -1);
      ac.hilitePrev();
      checkHilitedIndex(rend, 2);
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      // Second row is disabled
      checkHilitedIndex(rend, 2);
      // try going over the edge. Since free select is on, this should go to
      // -1.
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      // Second row is disabled.
      checkHilitedIndex(rend, 2);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, free select.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
      // wrap to last
      ac.hilitePrev();
      checkHilitedIndex(rend, 2);
    }
  },

  /**
   * Try using next and prev to navigate past the ends with wrap_ on
   * allowFreeSelect_ on AND turn autoHilite_ off, and a disabled last row.
   */
  testHiliteNextPrevWithDisabledLastRow_wrapAndAllowFreeSelectNoAutoHilite() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);
    ac.setAutoHilite(false);

    // make sure 'next' and 'prev' don't explode before any token is set
    ac.hiliteNext();
    ac.hilitePrev();
    ac.setMaxMatches(4);
    assertEquals(0, rend.getRenderedRows().length);

    // check a few times with disabled last row
    for (let i = 0; i < 3; ++i) {
      ac.setToken('');
      ac.setToken('h');
      assertEquals(3, rend.getRenderedRows().length);
      // Initially nothing should be selected since autoHilite_ is off.
      checkHilitedIndex(rend, -1);
      ac.hilitePrev();
      // Last row is disabled
      checkHilitedIndex(rend, 1);
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);
      // try going over the edge. Since free select is on, this should go to
      // -1.
      ac.hiliteNext();
      checkHilitedIndex(rend, -1);

      // go back down the list
      ac.hiliteNext();
      checkHilitedIndex(rend, 0);
      ac.hiliteNext();
      checkHilitedIndex(rend, 1);

      // go back up the list.
      ac.hilitePrev();
      checkHilitedIndex(rend, 0);
      // go back above the first, free select.
      ac.hilitePrev();
      checkHilitedIndex(rend, -1);
      // wrap to last
      ac.hilitePrev();
      checkHilitedIndex(rend, 1);
    }
  },

  testHiliteWithChangingNumberOfRows() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setAutoHilite(true);
    ac.setMaxMatches(4);

    ac.setToken('m');
    assertEquals(4, rend.getRenderedRows().length);
    checkHilitedIndex(rend, 0);

    ac.setToken('ma');
    assertEquals(3, rend.getRenderedRows().length);
    checkHilitedIndex(rend, 0);

    // Hilite the second element
    let id = rend.getRenderedRows()[1].id;
    ac.hiliteId(id);

    ac.setToken('mar');
    assertEquals(1, rend.getRenderedRows().length);
    checkHilitedIndex(rend, 0);

    ac.setToken('ma');
    assertEquals(3, rend.getRenderedRows().length);
    checkHilitedIndex(rend, 0);

    // Hilite the second element
    id = rend.getRenderedRows()[1].id;
    ac.hiliteId(id);

    ac.setToken('m');
    assertEquals(4, rend.getRenderedRows().length);
    checkHilitedIndex(rend, 0);
  },

  /**
   * Checks that autohilite is disabled when there is no token; this allows
   * the user to tab out of an empty autocomplete.
   */
  testNoAutoHiliteWhenTokenIsEmpty() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);
    ac.setAutoHilite(true);
    ac.setMaxMatches(4);

    ac.setToken('');
    assertEquals(4, rend.getRenderedRows().length);
    // No token; nothing should be hilited.
    checkHilitedIndex(rend, -1);

    ac.setToken('the');
    assertEquals(4, rend.getRenderedRows().length);
    // Now there is a token, so the first row should be highlighted.
    checkHilitedIndex(rend, 0);
  },

  /** Checks that opt_preserveHilited works. */
  testPreserveHilitedWithoutAutoHilite() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);
    ac.setMaxMatches(4);
    ac.setAutoHilite(false);

    ac.setToken('m');
    assertEquals(4, rend.getRenderedRows().length);
    // No token; nothing should be hilited.
    checkHilitedIndex(rend, -1);

    // Hilite the second element
    const id = rend.getRenderedRows()[1].id;
    ac.hiliteId(id);

    checkHilitedIndex(rend, 1);

    // Re-render and check if the second element is still hilited
    ac.renderRows(rend.getRenderedRows(), true /* preserve hilite */);

    checkHilitedIndex(rend, 1);

    // Re-render without preservation
    ac.renderRows(rend.getRenderedRows());

    checkHilitedIndex(rend, -1);
  },

  /** Checks that the autohilite argument "true" of the matcher is used. */
  testAutoHiliteFromMatcherTrue() {
    const ds = new MockDS(true);
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);
    ac.setAutoHilite(false);  // Will be overruled.
    ac.setMaxMatches(4);

    ac.setToken('the');
    assertEquals(4, rend.getRenderedRows().length);
    // The first row should be highlighted.
    checkHilitedIndex(rend, 0);
  },

  /** Checks that the autohilite argument "false" of the matcher is used. */
  testAutoHiliteFromMatcherFalse() {
    const ds = new MockDS(false);
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setWrap(true);
    ac.setAllowFreeSelect(true);
    ac.setAutoHilite(true);  // Will be overruled.
    ac.setMaxMatches(4);

    ac.setToken('the');
    assertEquals(4, rend.getRenderedRows().length);
    // The first row should not be highlighted.
    checkHilitedIndex(rend, -1);
  },

  /**
     Hilite using ids, the way mouse-based hiliting would work.
     @suppress {visibility} suppression added to enable type checking
   */
  testHiliteId() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);

    // check a few times
    for (let i = 0; i < 3; ++i) {
      ac.setToken('m');
      assertEquals(4, rend.getRenderedRows().length);
      // try hiliting all 3
      for (let x = 0; x < 4; ++x) {
        const id = rend.getRenderedRows()[x].id;
        ac.hiliteId(id);
        assertEquals(ac.getIdOfIndex_(x), id);
      }
    }
  },

  /** Test selecting the hilited row */
  testSelection() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    let ac;

    // try with default selection
    ac = new AutoComplete(ds, rend, select);
    ac.setToken('m');
    ac.selectHilited();
    assertEquals(
        '"Slartibartfast Theadore" <fjordmaster@magrathea.com>',
        select.selectedRow);

    // try second item
    ac = new AutoComplete(ds, rend, select);
    ac.setToken('the');
    ac.hiliteNext();
    ac.selectHilited();
    assertEquals('"Ford Prefect" <ford@theguide.com>', select.selectedRow);
  },

  /** Dismiss when empty and non-empty */
  testDismiss() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();

    // dismiss empty
    let ac = new AutoComplete(ds, rend, select);
    let dismissed = 0;
    handler.listen(ac, AutoComplete.EventType.DISMISS, () => {
      dismissed++;
    });
    ac.dismiss();
    assertEquals(1, dismissed);

    ac = new AutoComplete(ds, rend, select);
    ac.setToken('sir not seen in this picture');
    ac.dismiss();

    // dismiss with contents
    ac = new AutoComplete(ds, rend, select);
    ac.setToken('t');
    ac.dismiss();
  },

  testTriggerSuggestionsOnUpdate() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);

    let dismissCalled = 0;
    rend.dismiss = () => {
      dismissCalled++;
    };

    let updateCalled = 0;
    select.update = (opt_force) => {
      updateCalled++;
    };

    // Normally, menu is dismissed after selecting row (without updating).
    ac.setToken('the');
    ac.selectHilited();
    assertEquals(1, dismissCalled);
    assertEquals(0, updateCalled);

    // But not if we re-trigger on update.
    ac.setTriggerSuggestionsOnUpdate(true);
    ac.setToken('the');
    ac.selectHilited();
    assertEquals(1, dismissCalled);
    assertEquals(1, updateCalled);
  },

  testDispose() {
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setToken('the');
    ac.dispose();
  },

  /** Ensure that activedescendant is updated properly. */
  testRolesAndStates() {
    function checkActiveDescendant(activeDescendant) {
      assertNotNull(inputElement);
      assertEquals(aria.getActiveDescendant(inputElement), activeDescendant);
    }
    function checkRole(el, role) {
      assertNotNull(el);
      assertEquals(aria.getRole(el), role);
    }
    const ds = new MockDS();
    const rend = new TestRend();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const select = new MockSelect();
    const ac = new AutoComplete(ds, rend, select);
    ac.setTarget(inputElement);

    // initially activedescendant is not set
    checkActiveDescendant(null);

    // highlight the matching row and check that activedescendant updates
    ac.setToken('');
    ac.setToken('the');
    ac.hiliteNext();
    checkActiveDescendant(rend.getHilitedRowDiv());

    // highligted row should have a role of 'option'
    checkRole(rend.getHilitedRowDiv(), Role.OPTION);

    // closing the autocomplete should clear activedescendant
    ac.dismiss();
    checkActiveDescendant(null);
  },

  testAttachInputWithAnchor() {
    const anchorElement = dom.createDom(TagName.DIV, null, inputElement);

    const mockRenderer = mockControl.createLooseMock(Renderer, true);
    mockRenderer.setAnchorElement(anchorElement);
    const ignore = mockmatchers.ignoreArgument;
    mockRenderer.renderRows(ignore, ignore, inputElement);

    const mockInputHandler = mockControl.createLooseMock(InputHandler, true);
    mockInputHandler.attachInputs(inputElement);

    mockControl.$replayAll();
    const autoComplete = new AutoComplete(null, mockRenderer, mockInputHandler);
    autoComplete.attachInputWithAnchor(inputElement, anchorElement);
    autoComplete.setTarget(inputElement);

    autoComplete.renderRows(['abc', 'def']);
    mockControl.$verifyAll();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDetachInputWithAnchor() {
    const mockRenderer = mockControl.createLooseMock(Renderer, true);
    const mockInputHandler = mockControl.createLooseMock(InputHandler, true);
    const anchorElement = dom.createDom(TagName.DIV, null, inputElement);
    const inputElement2 = dom.createDom(TagName.INPUT, {type: InputType.TEXT});
    const anchorElement2 = dom.createDom(TagName.DIV, null, inputElement2);

    mockControl.$replayAll();
    const autoComplete = new AutoComplete(null, mockRenderer, mockInputHandler);

    autoComplete.attachInputWithAnchor(inputElement, anchorElement);
    autoComplete.attachInputWithAnchor(inputElement2, anchorElement2);
    autoComplete.detachInputs(inputElement, inputElement2);

    assertFalse(goog.getUid(inputElement) in autoComplete.inputToAnchorMap_);
    assertFalse(goog.getUid(inputElement2) in autoComplete.inputToAnchorMap_);
    mockControl.$verifyAll();
  },
});
