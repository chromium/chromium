/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.editor.AbstractDialogTest');
goog.setTestOnly();

const AbstractDialog = goog.require('goog.ui.editor.AbstractDialog');
const ArgumentMatcher = goog.require('goog.testing.mockmatchers.ArgumentMatcher');
const DomHelper = goog.require('goog.dom.DomHelper');
const EventHandler = goog.require('goog.events.EventHandler');
const GoogEvent = goog.require('goog.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const MockControl = goog.require('goog.testing.MockControl');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

function shouldRunTests() {
  // Test disabled in IE7 due to flakiness. See b/4269021.
  return !userAgent.IE;
}

let dialog;
let builder;

let mockCtrl;
let mockAfterHideHandler;
let mockOkHandler;
let mockCancelHandler;
let mockCustomButtonHandler;

const CUSTOM_EVENT = 'customEvent';
const CUSTOM_BUTTON_ID = 'customButton';

/**
 * Sets up the mock event handler to expect an AFTER_HIDE event.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function expectAfterHide() {
  mockAfterHideHandler.handleEvent(new ArgumentMatcher(
      (arg) => arg.type == AbstractDialog.EventType.AFTER_HIDE));
}

/**
 * Sets up the mock event handler to expect an OK event.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function expectOk() {
  mockOkHandler.handleEvent(
      new ArgumentMatcher((arg) => arg.type == AbstractDialog.EventType.OK));
}

/**
 * Sets up the mock event handler to expect an OK event and to call
 * preventDefault when handling it.
 */
function expectOkPreventDefault() {
  expectOk();
  mockOkHandler.$does((e) => {
    e.preventDefault();
  });
}

/**
 * Sets up the mock event handler to expect an OK event and to return false
 * when handling it.
 */
function expectOkReturnFalse() {
  expectOk();
  mockOkHandler.$returns(false);
}

/**
 * Sets up the mock event handler to expect a CANCEL event.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function expectCancel() {
  mockCancelHandler.handleEvent(new ArgumentMatcher(
      (arg) => arg.type == AbstractDialog.EventType.CANCEL));
}

/**
 * Sets up the mock event handler to expect a custom button event.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function expectCustomButton() {
  mockCustomButtonHandler.handleEvent(
      new ArgumentMatcher((arg) => arg.type == CUSTOM_EVENT));
}

/**
 * Helper to create the dialog being tested in each test. Since NewDialog is
 * abstract, needs to add a concrete version of any abstract methods. Also
 * creates up the global builder variable which should be set up after the call
 * to this method.
 * @return {!AbstractDialog} The dialog.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function createTestDialog() {
  const dialog = new AbstractDialog(new DomHelper());
  builder = new AbstractDialog.Builder(dialog);
  /** @suppress {visibility} suppression added to enable type checking */
  dialog.createDialogControl = () => builder.build();
  /** @suppress {visibility} suppression added to enable type checking */
  dialog.createOkEvent = (e) => new GoogEvent(AbstractDialog.EventType.OK);
  dialog.addEventListener(
      AbstractDialog.EventType.AFTER_HIDE, mockAfterHideHandler);
  dialog.addEventListener(AbstractDialog.EventType.OK, mockOkHandler);
  dialog.addEventListener(AbstractDialog.EventType.CANCEL, mockCancelHandler);
  dialog.addEventListener(CUSTOM_EVENT, mockCustomButtonHandler);
  return dialog;
}

/**
 * Asserts that the given dialog is open.
 * @param {string} msg Message to be printed in case of failure.
 * @param {AbstractDialog} dialog Dialog to be tested.
 */
function assertOpen(msg, dialog) {
  assertTrue(msg + ' [AbstractDialog.isOpen()]', dialog && dialog.isOpen());
}

/**
 * Asserts that the given dialog is closed.
 * @param {string} msg Message to be printed in case of failure.
 * @param {AbstractDialog} dialog Dialog to be tested.
 */
function assertNotOpen(msg, dialog) {
  assertFalse(msg + ' [AbstractDialog.isOpen()]', dialog && dialog.isOpen());
}

testSuite({
  setUp() {
    mockCtrl = new MockControl();
    mockAfterHideHandler = mockCtrl.createLooseMock(EventHandler);
    mockOkHandler = mockCtrl.createLooseMock(EventHandler);
    mockCancelHandler = mockCtrl.createLooseMock(EventHandler);
    mockCustomButtonHandler = mockCtrl.createLooseMock(EventHandler);
  },

  tearDown() {
    if (dialog) {
      mockAfterHideHandler.$setIgnoreUnexpectedCalls(true);
      dialog.dispose();
    }
  },

  /**
   * Tests that if you create a dialog and hide it without having shown it, no
   * errors occur.
   */
  testCreateAndHide() {
    dialog = createTestDialog();
    mockCtrl.$replayAll();

    assertNotOpen('Dialog should not be open after creation', dialog);
    dialog.hide();
    assertNotOpen('Dialog should not be open after hide()', dialog);

    mockCtrl.$verifyAll();  // Verifies AFTER_HIDE was not dispatched.
  },

  /**
   * Tests that when you show and hide a dialog the flags indicating open are
   * correct and the AFTER_HIDE event is dispatched (and no errors happen).
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testShowAndHide() {
    dialog = createTestDialog();
    expectAfterHide(dialog);
    mockCtrl.$replayAll();

    assertNotOpen('Dialog should not be open before show()', dialog);
    dialog.show();
    assertOpen('Dialog should be open after show()', dialog);
    dialog.hide();
    assertNotOpen('Dialog should not be open after hide()', dialog);

    mockCtrl.$verifyAll();  // Verifies AFTER_HIDE was dispatched.
  },

  /**
   * Tests that when you show and dispose a dialog (without hiding it first) the
   * flags indicating open are correct and the AFTER_HIDE event is dispatched
   * (and no errors happen).
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testShowAndDispose() {
    dialog = createTestDialog();
    expectAfterHide(dialog);
    mockCtrl.$replayAll();

    assertNotOpen('Dialog should not be open before show()', dialog);
    dialog.show();
    assertOpen('Dialog should be open after show()', dialog);
    dialog.dispose();
    assertNotOpen('Dialog should not be open after dispose()', dialog);

    mockCtrl.$verifyAll();  // Verifies AFTER_HIDE was dispatched.
  },

  /**
   * Tests that when you dispose a dialog (without ever showing it first) the
   * flags indicating open are correct and the AFTER_HIDE event is never
   * dispatched (and no errors happen).
   */
  testDisposeWithoutShow() {
    dialog = createTestDialog();
    mockCtrl.$replayAll();

    assertNotOpen('Dialog should not be open before dispose()', dialog);
    dialog.dispose();
    assertNotOpen('Dialog should not be open after dispose()', dialog);

    mockCtrl.$verifyAll();  // Verifies AFTER_HIDE was NOT dispatched.
  },

  /**
   * Tests that labels set in the builder can be found in the resulting dialog's
   * HTML.
   */
  testBasicLayout() {
    dialog = createTestDialog();
    mockCtrl.$replayAll();

    // create some dialog content
    const content = dom.createDom(TagName.DIV, null, 'The Content');
    builder.setTitle('The Title')
        .setContent(content)
        .addOkButton('The OK Button')
        .addCancelButton()
        .addButton('The Apply Button', goog.nullFunction)
        .addClassName('myClassName');
    dialog.show();

    /** @suppress {visibility} suppression added to enable type checking */
    const dialogElem = dialog.dialogInternal_.getElement();
    const html = dialogElem.innerHTML;
    // TODO(user): This is really insufficient. If the title and
    // content were swapped this test would still pass!
    assertContains('Dialog html should contain title', '>The Title<', html);
    assertContains('Dialog html should contain content', '>The Content<', html);
    assertContains(
        'Dialog html should contain custom OK button label', '>The OK Button<',
        html);
    assertContains(
        'Dialog html should contain default Cancel button label', '>Cancel<',
        html);
    assertContains(
        'Dialog html should contain custom button label', '>The Apply Button<',
        html);
    assertTrue(
        'Dialog should have default Closure class',
        classlist.contains(dialogElem, 'modal-dialog'));
    assertTrue(
        'Dialog should have our custom class',
        classlist.contains(dialogElem, 'myClassName'));

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that clicking the OK button dispatches the OK event and closes the
   * dialog (dispatching the AFTER_HIDE event too).
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testOk() {
    dialog = createTestDialog();
    expectOk(dialog);
    expectAfterHide(dialog);
    mockCtrl.$replayAll();

    dialog.show();
    events.fireClickSequence(dialog.getOkButtonElement());
    assertNotOpen('Dialog should not be open after clicking OK', dialog);

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that hitting the enter key dispatches the OK event and closes the
   * dialog (dispatching the AFTER_HIDE event too).
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testEnter() {
    dialog = createTestDialog();
    expectOk(dialog);
    expectAfterHide(dialog);
    mockCtrl.$replayAll();

    dialog.show();
    events.fireKeySequence(dialog.dialogInternal_.getElement(), KeyCodes.ENTER);
    assertNotOpen('Dialog should not be open after hitting enter', dialog);

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that clicking the Cancel button dispatches the CANCEL event and
   * closes the dialog (dispatching the AFTER_HIDE event too).
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testCancel() {
    dialog = createTestDialog();
    expectCancel(dialog);
    expectAfterHide(dialog);
    mockCtrl.$replayAll();

    builder.addCancelButton('My Cancel Button');

    dialog.show();
    events.fireClickSequence(dialog.getCancelButtonElement());
    assertNotOpen('Dialog should not be open after clicking Cancel', dialog);

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that hitting the escape key dispatches the CANCEL event and closes
   * the dialog (dispatching the AFTER_HIDE event too).
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testEscape() {
    dialog = createTestDialog();
    expectCancel(dialog);
    expectAfterHide(dialog);
    mockCtrl.$replayAll();

    dialog.show();
    events.fireKeySequence(dialog.dialogInternal_.getElement(), KeyCodes.ESC);
    assertNotOpen('Dialog should not be open after hitting escape', dialog);

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that clicking the custom button dispatches the custom event and
   * closes the dialog (dispatching the AFTER_HIDE event too).
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testCustomButton() {
    dialog = createTestDialog();
    expectCustomButton(dialog);
    expectAfterHide(dialog);
    mockCtrl.$replayAll();

    builder.addButton('My Custom Button', () => {
      dialog.dispatchEvent(CUSTOM_EVENT);
    }, CUSTOM_BUTTON_ID);

    dialog.show();
    events.fireClickSequence(dialog.getButtonElement(CUSTOM_BUTTON_ID));
    assertNotOpen(
        'Dialog should not be open after clicking custom button', dialog);

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that if the OK handler calls preventDefault, the dialog doesn't
   *      close.
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testOkPreventDefault() {
    dialog = createTestDialog();
    expectOkPreventDefault(dialog);
    mockCtrl.$replayAll();

    dialog.show();
    events.fireClickSequence(dialog.getOkButtonElement());
    assertOpen(
        'Dialog should not be closed because preventDefault was called',
        dialog);

    mockCtrl.$verifyAll();
  },

  /**
     Tests that if the OK handler returns false, the dialog doesn't close.
     @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testOkReturnFalse() {
    dialog = createTestDialog();
    expectOkReturnFalse(dialog);
    mockCtrl.$replayAll();

    dialog.show();
    events.fireClickSequence(dialog.getOkButtonElement());
    assertOpen(
        'Dialog should not be closed because handler returned false', dialog);

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that if creating the OK event fails, no event is dispatched and the
   * dialog doesn't close.
   * @suppress {visibility} suppression added to enable type checking
   */
  testCreateOkEventFail() {
    dialog = createTestDialog();
    /** @suppress {visibility} suppression added to enable type checking */
    dialog.createOkEvent = () => {  // Override our mock createOkEvent.
      return null;
    };
    mockCtrl.$replayAll();

    dialog.show();
    events.fireClickSequence(dialog.getOkButtonElement());
    assertOpen(
        'Dialog should not be closed because OK event creation failed', dialog);

    mockCtrl.$verifyAll();  // Verifies that no event was dispatched.
  },

  /**
   * Tests that processOkAndClose() dispatches the OK event and closes the
   * dialog (dispatching the AFTER_HIDE event too).
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testProcessOkAndClose() {
    dialog = createTestDialog();
    expectOk(dialog);
    expectAfterHide(dialog);
    mockCtrl.$replayAll();

    dialog.show();
    dialog.processOkAndClose();
    assertNotOpen(
        'Dialog should not be open after processOkAndClose()', dialog);

    mockCtrl.$verifyAll();
  },

  /**
   * Tests that if the OK handler triggered by processOkAndClose calls
   * preventDefault, the dialog doesn't close (in the old implementation this
   * failed due to not great design, so this is sort of a regression test).
   * @suppress {checkTypes,visibility} suppression added to enable type checking
   */
  testProcessOkAndClosePreventDefault() {
    dialog = createTestDialog();
    expectOkPreventDefault(dialog);
    mockCtrl.$replayAll();

    dialog.show();
    dialog.processOkAndClose();
    assertOpen(
        'Dialog should not be closed because preventDefault was called',
        dialog);

    mockCtrl.$verifyAll();
  },
});
