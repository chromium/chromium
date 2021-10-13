/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.AbstractDialogPluginTest');
goog.setTestOnly();

const AbstractDialog = goog.require('goog.ui.editor.AbstractDialog');
const AbstractDialogPlugin = goog.require('goog.editor.plugins.AbstractDialogPlugin');
const ArgumentMatcher = goog.require('goog.testing.mockmatchers.ArgumentMatcher');
const EventHandler = goog.require('goog.events.EventHandler');
const Field = goog.require('goog.editor.Field');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const GoogEvent = goog.require('goog.events.Event');
const MockClock = goog.require('goog.testing.MockClock');
const MockControl = goog.require('goog.testing.MockControl');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SafeHtml = goog.require('goog.html.SafeHtml');
const SavedRange = goog.require('goog.dom.SavedRange');
const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const functions = goog.require('goog.functions');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let plugin;
let mockCtrl;
let mockField;
let mockSavedRange;
let mockOpenedHandler;
let mockClosedHandler;

const COMMAND = 'myCommand';
const stubs = new PropertyReplacer();

let mockClock;
let fieldObj;
let fieldElem;
let mockHandler;

function setUpMockRange() {
  mockSavedRange = mockCtrl.createLooseMock(SavedRange);
  mockSavedRange.restore();

  stubs.setPath(
      'goog.editor.range.saveUsingNormalizedCarets',
      functions.constant(mockSavedRange));
}

/**
 * Creates a concrete instance of AbstractDialog by adding
 * a plain implementation of createDialogControl().
 * @param {dom.DomHelper} domHelper The dom helper to be used to create the
 *     dialog.
 * @return {!AbstractDialog} The created dialog.
 */
function createDialog(domHelper) {
  const dialog = new AbstractDialog(domHelper);
  /** @suppress {visibility} suppression added to enable type checking */
  dialog.createDialogControl = () => new AbstractDialog.Builder(dialog).build();
  return dialog;
}

/**
 * Creates a concrete instance of the abstract class
 * AbstractDialogPlugin
 * and registers it with the mock editable field being used.
 * @return {!AbstractDialogPlugin} The created plugin.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function createDialogPlugin() {
  const plugin = new AbstractDialogPlugin(COMMAND);
  /** @suppress {visibility} suppression added to enable type checking */
  plugin.createDialog = createDialog;
  /**
   * @suppress {strictMissingProperties,visibility} suppression added to enable
   * type checking
   */
  plugin.returnControlToEditableField = plugin.restoreOriginalSelection;
  plugin.registerFieldObject(mockField);
  plugin.addEventListener(
      AbstractDialogPlugin.EventType.OPENED, mockOpenedHandler);
  plugin.addEventListener(
      AbstractDialogPlugin.EventType.CLOSED, mockClosedHandler);
  return plugin;
}

/**
 * Sets up the mock event handler to expect an OPENED event.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function expectOpened(/** number= */ times = undefined) {
  mockOpenedHandler.handleEvent(new ArgumentMatcher(
      (arg) => arg.type == AbstractDialogPlugin.EventType.OPENED));
  mockField.dispatchSelectionChangeEvent();
  if (times) {
    mockOpenedHandler.$times(times);
    mockField.$times(times);
  }
}

/**
 * Sets up the mock event handler to expect a CLOSED event.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function expectClosed(/** number= */ times = undefined) {
  mockClosedHandler.handleEvent(new ArgumentMatcher(
      (arg) => arg.type == AbstractDialogPlugin.EventType.CLOSED));
  mockField.dispatchSelectionChangeEvent();
  if (times) {
    mockClosedHandler.$times(times);
    mockField.$times(times);
  }
}

/**
 * Setup a real editable field (instead of a mock) and register the plugin to
 * it.
 */
function setUpRealEditableField() {
  fieldElem = dom.createElement(TagName.DIV);
  fieldElem.id = 'myField';
  document.body.appendChild(fieldElem);
  fieldObj = new Field('myField', document);
  fieldObj.makeEditable();
  // Register the plugin to that field.
  plugin.getTrogClassId = functions.constant('myClassId');
  fieldObj.registerPlugin(plugin);
}

/** Tear down the real editable field. */
function tearDownRealEditableField() {
  if (fieldObj) {
    fieldObj.makeUneditable();
    fieldObj.dispose();
    fieldObj = null;
  }
  if (fieldElem && fieldElem.parentNode == document.body) {
    document.body.removeChild(fieldElem);
  }
}

testSuite({
  /** @suppress {missingProperties} suppression added to enable type checking */
  setUp() {
    mockCtrl = new MockControl();
    mockOpenedHandler = mockCtrl.createLooseMock(EventHandler);
    mockClosedHandler = mockCtrl.createLooseMock(EventHandler);

    /** @suppress {checkTypes} suppression added to enable type checking */
    mockField = new FieldMock(undefined, undefined, {});
    mockCtrl.addMock(mockField);
    mockField.focus();

    plugin = createDialogPlugin();
  },

  tearDown() {
    stubs.reset();
    tearDownRealEditableField();
    if (mockClock) {
      // Crucial to letting time operations work normally in the rest of tests.
      mockClock.dispose();
    }
    if (plugin) {
      mockField.$setIgnoreUnexpectedCalls(true);
      plugin.dispose();
    }
  },

  /**
   * Tests the simple flow of calling execCommand (which opens the
   * dialog) and immediately disposing of the plugin (which closes the dialog).
   * @param {boolean=} reuse Whether to set the plugin to reuse its dialog.
   * @suppress {missingProperties,visibility} suppression added to enable type
   * checking
   */
  testExecAndDispose(reuse = undefined) {
    setUpMockRange();
    expectOpened();
    expectClosed();
    mockField.debounceEvent(Field.EventType.SELECTIONCHANGE);
    mockCtrl.$replayAll();
    if (reuse) {
      plugin.setReuseDialog(true);
    }
    assertFalse(
        'Dialog should not be open yet',
        !!plugin.getDialog() && plugin.getDialog().isOpen());

    plugin.execCommand(COMMAND);
    assertTrue(
        'Dialog should be open now',
        !!plugin.getDialog() && plugin.getDialog().isOpen());

    /** @suppress {visibility} suppression added to enable type checking */
    const tempDialog = plugin.getDialog();
    plugin.dispose();
    assertFalse(
        'Dialog should not still be open after disposal', tempDialog.isOpen());
    mockCtrl.$verifyAll();
  },

  /** Tests execCommand and dispose while reusing the dialog. */
  testExecAndDisposeReuse() {
    this.testExecAndDispose(true);
  },

  /**
   * Tests the flow of calling execCommand (which opens the dialog) and
   * then hiding it (simulating that a user did somthing to cause the dialog to
   * close).
   * @param {boolean=} reuse Whether to set the plugin to reuse its dialog.
   * @suppress {missingProperties,visibility} suppression added to enable type
   * checking
   */
  testExecAndHide(reuse = undefined) {
    setUpMockRange();
    expectOpened();
    expectClosed();
    mockField.debounceEvent(Field.EventType.SELECTIONCHANGE);
    mockCtrl.$replayAll();
    if (reuse) {
      plugin.setReuseDialog(true);
    }
    assertFalse(
        'Dialog should not be open yet',
        !!plugin.getDialog() && plugin.getDialog().isOpen());

    plugin.execCommand(COMMAND);
    assertTrue(
        'Dialog should be open now',
        !!plugin.getDialog() && plugin.getDialog().isOpen());

    /** @suppress {visibility} suppression added to enable type checking */
    const tempDialog = plugin.getDialog();
    plugin.getDialog().hide();
    assertFalse(
        'Dialog should not still be open after hiding', tempDialog.isOpen());
    if (reuse) {
      assertFalse(
          'Dialog should not be disposed after hiding (will be reused)',
          tempDialog.isDisposed());
    } else {
      assertTrue(
          'Dialog should be disposed after hiding', tempDialog.isDisposed());
    }
    plugin.dispose();
    mockCtrl.$verifyAll();
  },

  /** Tests execCommand and hide while reusing the dialog. */
  testExecAndHideReuse() {
    this.testExecAndHide(true);
  },

  /**
   * Tests the flow of calling execCommand (which opens a dialog) and
   * then calling it again before the first dialog is closed. This is not
   * something anyone should be doing since dialogs are (usually?) modal so the
   * user can't do another execCommand before closing the first dialog. But
   * since the API makes it possible, I thought it would be good to guard
   * against and unit test.
   * @param {boolean=} reuse Whether to set the plugin to reuse its dialog.
   * @suppress {visibility,missingProperties} suppression added to enable type
   * checking
   */
  testExecTwice(reuse = undefined) {
    setUpMockRange();
    if (reuse) {
      expectOpened(2);  // The second exec should cause a second OPENED event.
      // But the dialog was not closed between exec calls, so only one CLOSED is
      // expected.
      expectClosed();
      plugin.setReuseDialog(true);
      mockField.debounceEvent(Field.EventType.SELECTIONCHANGE);
    } else {
      expectOpened(2);  // The second exec should cause a second OPENED event.
      // The first dialog will be disposed so there should be two CLOSED events.
      expectClosed(2);
      mockSavedRange.restore();  // Expected 2x, once already recorded in setup.
      mockField.focus();         // Expected 2x, once already recorded in setup.
      mockField.debounceEvent(Field.EventType.SELECTIONCHANGE);
      mockField.$times(2);
    }
    mockCtrl.$replayAll();

    assertFalse(
        'Dialog should not be open yet',
        !!plugin.getDialog() && plugin.getDialog().isOpen());

    plugin.execCommand(COMMAND);
    assertTrue(
        'Dialog should be open now',
        !!plugin.getDialog() && plugin.getDialog().isOpen());

    /** @suppress {visibility} suppression added to enable type checking */
    const tempDialog = plugin.getDialog();
    plugin.execCommand(COMMAND);
    if (reuse) {
      assertTrue(
          'Reused dialog should still be open after second exec',
          tempDialog.isOpen());
      assertFalse(
          'Reused dialog should not be disposed after second exec',
          tempDialog.isDisposed());
    } else {
      assertFalse(
          'First dialog should not still be open after opening second',
          tempDialog.isOpen());
      assertTrue(
          'First dialog should be disposed after opening second',
          tempDialog.isDisposed());
    }
    plugin.dispose();
    mockCtrl.$verifyAll();
  },

  /** Tests execCommand twice while reusing the dialog. */
  testExecTwiceReuse() {
    this.testExecTwice(true);
  },

  /**
   * Tests that the selection is cleared when the dialog opens and is
   * correctly restored after it closes.
   * @suppress {visibility} suppression added to enable type checking
   */
  testRestoreSelection() {
    setUpRealEditableField();

    fieldObj.setSafeHtml(false, SafeHtml.htmlEscape('12345'));
    const elem = fieldObj.getElement();
    const helper = new TestHelper(elem);
    helper.select('12345', 1, '12345', 4);  // Selects '234'.

    assertEquals(
        'Incorrect text selected before dialog is opened', '234',
        fieldObj.getRange().getText());
    plugin.execCommand(COMMAND);
    if (!userAgent.IE) {
      // IE returns some bogus range when field doesn't have selection.
      // Opera can't remove the selection from a whitebox field.
      assertNull(
          'There should be no selection while dialog is open',
          fieldObj.getRange());
    }
    plugin.getDialog().hide();
    assertEquals(
        'Incorrect text selected after dialog is closed', '234',
        fieldObj.getRange().getText());
  },

  /**
   * Tests that after the dialog is hidden via a keystroke, the editable field
   * doesn't fire an extra SELECTIONCHANGE event due to the keyup from that
   * keystroke.
   * There is also a robot test in dialog_robot.html to test debouncing the
   * SELECTIONCHANGE event when the dialog closes.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testDebounceSelectionChange() {
    mockClock = new MockClock(true);
    // Initial time is 0 which evaluates to false in debouncing implementation.
    mockClock.tick(1);

    setUpRealEditableField();

    // Set up a mock event handler to make sure selection change isn't fired
    // more than once on close and a second time on close.
    let count = 0;
    fieldObj.addEventListener(Field.EventType.SELECTIONCHANGE, (e) => {
      count++;
    });

    assertEquals(0, count);
    plugin.execCommand(COMMAND);
    assertEquals(1, count);
    plugin.getDialog().hide();
    assertEquals(2, count);

    // Fake the keyup event firing on the field after the dialog closes.
    /** @suppress {visibility} suppression added to enable type checking */
    const e = new GoogEvent('keyup', plugin.fieldObject.getElement());
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = 13;
    events.fireBrowserEvent(e);

    // Tick the mock clock so that selection change tries to fire.
    mockClock.tick(Field.SELECTION_CHANGE_FREQUENCY_ + 1);

    // Ensure the handler did not fire again.
    assertEquals(2, count);
  },
});
