/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.DialogTest');
goog.setTestOnly();

const Dialog = goog.require('goog.ui.Dialog');
const EventType = goog.require('goog.events.EventType');
const KeyCodes = goog.require('goog.events.KeyCodes');
const MockClock = goog.require('goog.testing.MockClock');
const Role = goog.require('goog.a11y.aria.Role');
const SafeHtml = goog.require('goog.html.SafeHtml');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const css3 = goog.require('goog.fx.css3');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const recordFunction = goog.require('goog.testing.recordFunction');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

let bodyChildElement;
let decorateTarget;
let dialog;
let mockClock;

/**
 * Helper that clicks the first button in the dialog and checks if that
 * results in a Dialog.EventType.SELECT being dispatched.
 * @param {boolean} disableButton Whether to disable the button being tested.
 * @return {boolean} Whether a Dialog.EventType.SELECT was dispatched.
 */
function checkSelectDispatchedOnButtonClick(disableButton) {
  const aButton =
      dom.getElementsByTagName(TagName.BUTTON, dialog.getButtonElement())[0];
  assertNotEquals(aButton, null);
  aButton.disabled = disableButton;
  let wasCalled = false;
  const callRecorder = () => {
    wasCalled = true;
  };
  events.listen(dialog, Dialog.EventType.SELECT, callRecorder);
  testingEvents.fireClickSequence(aButton);
  return wasCalled;
}

function checkEnterKeyDoesNothingOnSpecialFormElement(content, tagName) {
  dialog.setSafeHtmlContent(content);
  const formElement =
      dialog.getContentElement().getElementsByTagName(tagName)[0];
  let wasCalled = false;
  const callRecorder = () => {
    wasCalled = true;
  };
  events.listen(dialog, Dialog.EventType.SELECT, callRecorder);

  // Enter does not fire on the enabled form element.
  testingEvents.fireKeySequence(formElement, KeyCodes.ENTER);
  assertFalse(wasCalled);

  // Enter fires on the disabled form element.
  formElement.disabled = true;
  testingEvents.fireKeySequence(formElement, KeyCodes.ENTER);
  assertTrue(wasCalled);
}

/**
 * Assert that the dialog has buttons with the given keys in the correct
 * order.
 * @param {Array<string>} keys An array of button keys.
 */
function assertButtons(keys) {
  const buttons = dom.getElementsByTagName(TagName.BUTTON, dialog.getElement());
  const actualKeys = [];
  for (let i = 0; i < buttons.length; i++) {
    actualKeys[i] = buttons[i].name;
  }
  assertArrayEquals(keys, actualKeys);
}

// Asserts that a test element which is a child of the document body has the
// aria property 'hidden' set on it, or not.
function assertAriaHidden(expectedHidden) {
  const expectedString = expectedHidden ? 'true' : '';
  assertEquals(expectedString, aria.getState(bodyChildElement, State.HIDDEN));
}
testSuite({
  setUp() {
    mockClock = new MockClock(true);
    bodyChildElement = dom.createElement(TagName.DIV);
    document.body.appendChild(bodyChildElement);
    dialog = new Dialog();
    const buttons = new Dialog.ButtonSet();
    buttons.set(Dialog.DefaultButtonKeys.CANCEL, 'Foo!', false, true);
    buttons.set(Dialog.DefaultButtonKeys.OK, 'OK', true);
    dialog.setButtonSet(buttons);
    dialog.setVisible(true);

    decorateTarget = dom.createDom(TagName.DIV);
    document.body.appendChild(decorateTarget);
  },

  tearDown() {
    dialog.dispose();
    dom.removeNode(bodyChildElement);
    dom.removeNode(decorateTarget);
    mockClock.dispose();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testCrossFrameFocus() {
    // Firefox (3.6, maybe future versions) fails this test when there are too
    // many other test files being run concurrently.
    if (userAgent.IE || userAgent.GECKO) {
      return;
    }
    dialog.setVisible(false);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const iframeWindow = dom.getElement('f').contentWindow;
    const iframeInput =
        dom.getElementsByTagName(TagName.INPUT, iframeWindow.document)[0];
    dialog.setButtonSet(Dialog.ButtonSet.OK);
    const dialogElement = dialog.getElement();
    let focusCounter = 0;
    events.listen(dialogElement, 'focus', () => {
      focusCounter++;
    });
    iframeInput.focus();
    dialog.setVisible(true);
    dialog.setVisible(false);
    iframeInput.focus();
    dialog.setVisible(true);
    assertEquals(2, focusCounter);
  },

  testNoDisabledButtonFocus() {
    dialog.setVisible(false);
    const buttonEl =
        dialog.getButtonSet().getButton(Dialog.DefaultButtonKeys.OK);
    buttonEl.disabled = true;
    let focused = false;
    buttonEl.focus = () => {
      focused = true;
    };
    dialog.setVisible(true);
    assertFalse('Should not have called focus on disabled button', focused);
  },

  testNoTitleClose() {
    assertTrue(style.isElementShown(dialog.getTitleCloseElement()));
    dialog.setHasTitleCloseButton(false);
    assertFalse(style.isElementShown(dialog.getTitleCloseElement()));
  },

  testButtonClicksDispatchSelectEvents() {
    assertTrue(
        'Select event should be dispatched' +
            ' when clicking on an enabled button',
        checkSelectDispatchedOnButtonClick(false));
  },

  testDisabledButtonClicksDontDispatchSelectEvents() {
    assertFalse(
        'Select event should not be dispatched' +
            ' when clicking on a disabled button',
        checkSelectDispatchedOnButtonClick(true));
  },

  testEnterKeyDispatchesDefaultSelectEvents() {
    const okButton =
        dom.getElementsByTagName(TagName.BUTTON, dialog.getButtonElement())[1];
    assertNotEquals(okButton, null);
    let wasCalled = false;
    const callRecorder = () => {
      wasCalled = true;
    };
    events.listen(dialog, Dialog.EventType.SELECT, callRecorder);
    // Test that event is not dispatched when default button is disabled.
    okButton.disabled = true;
    testingEvents.fireKeySequence(dialog.getElement(), KeyCodes.ENTER);
    assertFalse(wasCalled);
    // Test that event is dispatched when default button is enabled.
    okButton.disabled = false;
    testingEvents.fireKeySequence(dialog.getElement(), KeyCodes.ENTER);
    assertTrue(wasCalled);
  },

  testEnterKeyOnDisabledDefaultButtonDoesNotDispatchSelectEvents() {
    const okButton =
        dom.getElementsByTagName(TagName.BUTTON, dialog.getButtonElement())[1];
    okButton.focus();

    const callRecorder = recordFunction();
    events.listen(dialog, Dialog.EventType.SELECT, callRecorder);

    okButton.disabled = true;
    testingEvents.fireKeySequence(okButton, KeyCodes.ENTER);
    assertEquals(0, callRecorder.getCallCount());

    okButton.disabled = false;
    testingEvents.fireKeySequence(okButton, KeyCodes.ENTER);
    assertEquals(1, callRecorder.getCallCount());
  },

  testEnterKeyDoesNothingOnSpecialFormElements() {
    checkEnterKeyDoesNothingOnSpecialFormElement(
        SafeHtml.create('textarea', {}, 'Hello dialog'), 'TEXTAREA');

    checkEnterKeyDoesNothingOnSpecialFormElement(
        SafeHtml.create('select', {}, 'Selection'), 'SELECT');

    checkEnterKeyDoesNothingOnSpecialFormElement(
        SafeHtml.create('a', {'href': 'http://google.com'}, 'Hello dialog'),
        'A');
  },

  testEscapeKeyDoesNothingOnSpecialFormElements() {
    dialog.setSafeHtmlContent(SafeHtml.create('select', {}, [
      SafeHtml.create('option', {}, 'Hello'),
      SafeHtml.create('option', {}, 'dialog'),
    ]));
    const select = dialog.getContentElement().getElementsByTagName('SELECT')[0];
    let wasCalled = false;
    const callRecorder = () => {
      wasCalled = true;
    };
    events.listen(dialog, Dialog.EventType.SELECT, callRecorder);

    // Escape does not fire on the enabled select box.
    testingEvents.fireKeySequence(select, KeyCodes.ESC);
    assertFalse(wasCalled);

    // Escape fires on the disabled select.
    select.disabled = true;
    testingEvents.fireKeySequence(select, KeyCodes.ESC);
    assertTrue(wasCalled);
  },

  testEscapeCloses() {
    // If escapeCloses is set to false, the dialog should ignore the escape key
    assertTrue(dialog.isEscapeToCancel());
    dialog.setEscapeToCancel(false);
    assertFalse(dialog.isEscapeToCancel());

    const buttons = new Dialog.ButtonSet();
    buttons.set(Dialog.DefaultButtonKeys.OK, 'OK', true);
    dialog.setButtonSet(buttons);
    testingEvents.fireKeySequence(dialog.getContentElement(), KeyCodes.ESC);
    assertTrue(dialog.isVisible());

    // Having a cancel button should make no difference, escape should still not
    // work.
    buttons.set(Dialog.DefaultButtonKeys.CANCEL, 'Foo!', false, true);
    dialog.setButtonSet(buttons);
    testingEvents.fireKeySequence(dialog.getContentElement(), KeyCodes.ESC);
    assertTrue(dialog.isVisible());
  },

  testKeydownClosesWithoutButtonSet() {
    // Clear button set
    dialog.setButtonSet(null);

    // Create a custom button.
    dialog.setSafeHtmlContent(
        SafeHtml.create('button', {'id': 'button', 'name': 'ok'}, 'OK'));
    let wasCalled = false;
    function called() {
      wasCalled = true;
    }
    const element = dom.getElement('button');
    events.listen(element, EventType.KEYPRESS, called);
    // Listen for 'Enter' on the button.
    // This tests using a dialog with no ButtonSet that has been set. Uses
    // a custom button.  The callback should be called with no exception thrown.
    testingEvents.fireKeySequence(element, KeyCodes.ENTER);
    assertTrue('Should have gotten event on the button.', wasCalled);
  },

  testEnterKeyWithoutDefaultDoesNotPreventPropagation() {
    const buttons = new Dialog.ButtonSet();
    buttons.set(Dialog.DefaultButtonKeys.CANCEL, 'Foo!', false);
    // Set a button set without a default selected button
    dialog.setButtonSet(buttons);
    dialog.setSafeHtmlContent(SafeHtml.create(
        'span', {'id': 'linkel', 'tabindex': '0'}, 'Link Span'));

    let call = false;
    function called() {
      call = true;
    }
    const element = document.getElementById('linkel');
    events.listen(element, EventType.KEYDOWN, called);
    testingEvents.fireKeySequence(element, KeyCodes.ENTER);

    assertTrue('Should have gotten event on the link', call);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testPreventDefaultedSelectCausesStopPropagation() {
    dialog.setButtonSet(Dialog.ButtonSet.OK_CANCEL);

    const callCount = 0;
    let keypressCount = 0;
    let keydownCount = 0;

    const preventDefaulter = (e) => {
      e.preventDefault();
    };

    events.listen(dialog, Dialog.EventType.SELECT, preventDefaulter);
    events.listen(document.body, EventType.KEYPRESS, () => {
      keypressCount++;
    });
    events.listen(document.body, EventType.KEYDOWN, () => {
      keydownCount++;
    });

    // Ensure that if the SELECT event is prevented, all key events
    // are still stopped from propagating.
    testingEvents.fireKeySequence(dialog.getElement(), KeyCodes.ENTER);
    assertEquals('The KEYPRESS should be stopped', 0, keypressCount);
    assertEquals('The KEYDOWN should not be stopped', 1, keydownCount);

    keypressCount = 0;
    keydownCount = 0;
    testingEvents.fireKeySequence(dialog.getElement(), KeyCodes.ESC);
    assertEquals('The KEYDOWN should be stopped', 0, keydownCount);
    // Note: Some browsers don't yield keypresses on escape, so don't check.

    events.unlisten(dialog, Dialog.EventType.SELECT, preventDefaulter);

    keypressCount = 0;
    keydownCount = 0;
    testingEvents.fireKeySequence(dialog.getElement(), KeyCodes.ENTER);
    assertEquals('The KEYPRESS should be stopped', 0, keypressCount);
    assertEquals('The KEYDOWN should not be stopped', 1, keydownCount);
  },

  testEnterKeyHandledInKeypress() {
    let inKeyPress = false;
    events.listen(document.body, EventType.KEYPRESS, () => {
      inKeyPress = true;
    }, true /* capture */);
    events.listen(document.body, EventType.KEYPRESS, () => {
      inKeyPress = false;
    }, false /* !capture */);
    let selectCalled = false;
    events.listen(dialog, Dialog.EventType.SELECT, () => {
      selectCalled = true;
      assertTrue(
          'Select must be dispatched during keypress to allow popups',
          inKeyPress);
    });

    testingEvents.fireKeySequence(dialog.getElement(), KeyCodes.ENTER);
    assertTrue(selectCalled);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testShiftTabAtTopSetsUpWrapAndDoesNotPreventPropagation() {
    /** @suppress {visibility} suppression added to enable type checking */
    dialog.setupBackwardTabWrap = recordFunction();
    let shiftTabRecorder = recordFunction();

    events.listen(dialog.getElement(), EventType.KEYDOWN, shiftTabRecorder);
    const shiftProperties = {shiftKey: true};
    testingEvents.fireKeySequence(
        dialog.getElement(), KeyCodes.TAB, shiftProperties);

    assertNotNull(
        'Should have gotten event on Shift+TAB',
        shiftTabRecorder.getLastCall());
    assertNotNull(
        'Backward tab wrap should have been set up',
        dialog.setupBackwardTabWrap.getLastCall());
  },

  testButtonsWithContentsDispatchSelectEvents() {
    const aButton =
        dom.getElementsByTagName(TagName.BUTTON, dialog.getButtonElement())[0];
    const aSpan = dom.createElement(TagName.SPAN);
    aButton.appendChild(aSpan);
    let wasCalled = false;
    const callRecorder = () => {
      wasCalled = true;
    };
    events.listen(dialog, Dialog.EventType.SELECT, callRecorder);
    testingEvents.fireClickSequence(aSpan);
    assertTrue(wasCalled);
  },

  testAfterHideEvent() {
    let wasCalled = false;
    const callRecorder = () => {
      wasCalled = true;
    };
    events.listen(dialog, Dialog.EventType.AFTER_HIDE, callRecorder);
    dialog.setVisible(false);
    assertTrue(wasCalled);
  },

  testAfterShowEvent() {
    dialog.setVisible(false);
    let wasCalled = false;
    const callRecorder = () => {
      wasCalled = true;
    };
    events.listen(dialog, Dialog.EventType.AFTER_SHOW, callRecorder);
    dialog.setVisible(true);
    assertTrue(wasCalled);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testCannedButtonSets() {
    dialog.setButtonSet(Dialog.ButtonSet.OK);
    assertButtons([Dialog.DefaultButtonKeys.OK]);

    dialog.setButtonSet(Dialog.ButtonSet.OK_CANCEL);
    assertButtons([
      Dialog.DefaultButtonKeys.OK,
      Dialog.DefaultButtonKeys.CANCEL,
    ]);

    dialog.setButtonSet(Dialog.ButtonSet.YES_NO);
    assertButtons([
      Dialog.DefaultButtonKeys.YES,
      Dialog.DefaultButtonKeys.NO,
    ]);

    dialog.setButtonSet(Dialog.ButtonSet.YES_NO_CANCEL);
    assertButtons([
      Dialog.DefaultButtonKeys.YES,
      Dialog.DefaultButtonKeys.NO,
      Dialog.DefaultButtonKeys.CANCEL,
    ]);

    dialog.setButtonSet(Dialog.ButtonSet.CONTINUE_SAVE_CANCEL);
    assertButtons([
      Dialog.DefaultButtonKeys.CONTINUE,
      Dialog.DefaultButtonKeys.SAVE,
      Dialog.DefaultButtonKeys.CANCEL,
    ]);
  },

  testFactoryButtonSets() {
    dialog.setButtonSet(Dialog.ButtonSet.createOk());
    assertButtons([Dialog.DefaultButtonKeys.OK]);

    dialog.setButtonSet(Dialog.ButtonSet.createOkCancel());
    assertButtons([
      Dialog.DefaultButtonKeys.OK,
      Dialog.DefaultButtonKeys.CANCEL,
    ]);

    dialog.setButtonSet(Dialog.ButtonSet.createYesNo());
    assertButtons([
      Dialog.DefaultButtonKeys.YES,
      Dialog.DefaultButtonKeys.NO,
    ]);

    dialog.setButtonSet(Dialog.ButtonSet.createYesNoCancel());
    assertButtons([
      Dialog.DefaultButtonKeys.YES,
      Dialog.DefaultButtonKeys.NO,
      Dialog.DefaultButtonKeys.CANCEL,
    ]);

    dialog.setButtonSet(Dialog.ButtonSet.createContinueSaveCancel());
    assertButtons([
      Dialog.DefaultButtonKeys.CONTINUE,
      Dialog.DefaultButtonKeys.SAVE,
      Dialog.DefaultButtonKeys.CANCEL,
    ]);
  },

  testDefaultButtonClassName() {
    const key = 'someKey';
    const msg = 'someMessage';
    let isDefault = false;
    const buttonSetOne = new Dialog.ButtonSet().set(key, msg, isDefault);
    dialog.setButtonSet(buttonSetOne);
    /** @suppress {visibility} suppression added to enable type checking */
    const defaultClassName = goog.getCssName(buttonSetOne.class_, 'default');
    const buttonOne = buttonSetOne.getButton(key);
    assertNotEquals(defaultClassName, buttonOne.className);
    isDefault = true;
    const buttonSetTwo = new Dialog.ButtonSet().set(key, msg, isDefault);
    dialog.setButtonSet(buttonSetTwo);
    const buttonTwo = buttonSetTwo.getButton(key);
    assertEquals(defaultClassName, buttonTwo.className);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testGetButton() {
    dialog.setButtonSet(Dialog.ButtonSet.OK);
    const buttons = document.getElementsByName(Dialog.DefaultButtonKeys.OK);
    assertEquals(
        buttons[0],
        dialog.getButtonSet().getButton(Dialog.DefaultButtonKeys.OK));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testGetAllButtons() {
    dialog.setButtonSet(Dialog.ButtonSet.YES_NO_CANCEL);
    const buttons =
        dom.getElementsByTagName(TagName.BUTTON, dialog.getElement());
    for (let i = 0; i < buttons.length; i++) {
      assertEquals(buttons[i], dialog.getButtonSet().getAllButtons()[i]);
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSetButtonEnabled() {
    const buttonSet = Dialog.ButtonSet.createYesNoCancel();
    dialog.setButtonSet(buttonSet);
    assertFalse(buttonSet.getButton(Dialog.DefaultButtonKeys.NO).disabled);
    buttonSet.setButtonEnabled(Dialog.DefaultButtonKeys.NO, false);
    assertTrue(buttonSet.getButton(Dialog.DefaultButtonKeys.NO).disabled);
    buttonSet.setButtonEnabled(Dialog.DefaultButtonKeys.NO, true);
    assertFalse(buttonSet.getButton(Dialog.DefaultButtonKeys.NO).disabled);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSetAllButtonsEnabled() {
    const buttonSet = Dialog.ButtonSet.createContinueSaveCancel();
    dialog.setButtonSet(buttonSet);
    const buttons = buttonSet.getAllButtons();
    for (let i = 0; i < buttons.length; i++) {
      assertFalse(buttons[i].disabled);
    }

    buttonSet.setAllButtonsEnabled(false);
    for (let i = 0; i < buttons.length; i++) {
      assertTrue(buttons[i].disabled);
    }

    buttonSet.setAllButtonsEnabled(true);
    for (let i = 0; i < buttons.length; i++) {
      assertFalse(buttons[i].disabled);
    }
  },

  testIframeMask() {
    const prevNumFrames =
        dom.getElementsByTagNameAndClass(TagName.IFRAME).length;
    // generate a new dialog
    dialog.dispose();
    /** @suppress {checkTypes} suppression added to enable type checking */
    dialog = new Dialog(null, true /* iframe mask */);
    dialog.setVisible(true);

    // Test that the dialog added one iframe to the document.
    // The absolute number of iframes should not be tested because,
    // in certain cases, the test runner itself can can add an iframe
    // to the document as part of a strategy not to block the UI for too long.
    // See goog.async.nextTick.getSetImmediateEmulator_.
    const curNumFrames =
        dom.getElementsByTagNameAndClass(TagName.IFRAME).length;
    assertEquals('No iframe mask created', prevNumFrames + 1, curNumFrames);
  },

  testNonModalDialog() {
    const prevNumFrames =
        dom.getElementsByTagNameAndClass(TagName.IFRAME).length;
    // generate a new dialog
    dialog.dispose();
    /** @suppress {checkTypes} suppression added to enable type checking */
    dialog = new Dialog(null, true /* iframe mask */);
    dialog.setModal(false);
    assertAriaHidden(false);
    dialog.setVisible(true);
    assertAriaHidden(true);

    // Test that the dialog did not change the number of iframes in the
    // document. The absolute number of iframes should not be tested because, in
    // certain cases, the test runner itself can can add an iframe to the
    // document as part of a strategy not to block the UI for too long. See
    // goog.async.nextTick.getSetImmediateEmulator_.
    const curNumFrames =
        dom.getElementsByTagNameAndClass(TagName.IFRAME).length;
    assertEquals(
        'Iframe mask created for modal dialog', prevNumFrames, curNumFrames);
  },

  testSwapModalForOpenDialog() {
    dialog.dispose();
    /** @suppress {checkTypes} suppression added to enable type checking */
    dialog = new Dialog(null, true /* iframe mask */);
    assertAriaHidden(false);
    dialog.setVisible(true);
    assertAriaHidden(true);
    dialog.setModal(false);
    assertAriaHidden(false);
    assertFalse(
        'IFrame bg element should not be in dom',
        dom.contains(document.body, dialog.getBackgroundIframe()));
    assertFalse(
        'bg element should not be in dom',
        dom.contains(document.body, dialog.getBackgroundElement()));

    dialog.setModal(true);
    assertAriaHidden(true);
    assertTrue(
        'IFrame bg element should be in dom',
        dom.contains(document.body, dialog.getBackgroundIframe()));
    assertTrue(
        'bg element should be in dom',
        dom.contains(document.body, dialog.getBackgroundElement()));

    assertEquals(
        'IFrame bg element is a child of body', document.body,
        dialog.getBackgroundIframe().parentNode);
    assertEquals(
        'bg element is a child of body', document.body,
        dialog.getBackgroundElement().parentNode);

    assertTrue(
        'IFrame bg element should visible',
        style.isElementShown(dialog.getBackgroundIframe()));
    assertTrue(
        'bg element should be visible',
        style.isElementShown(dialog.getBackgroundElement()));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testButtonSetOkFiresDialogEventOnEscape() {
    dialog.setButtonSet(Dialog.ButtonSet.OK);
    let wasCalled = false;
    const callRecorder = () => {
      wasCalled = true;
    };
    events.listen(dialog, Dialog.EventType.SELECT, callRecorder);
    testingEvents.fireKeySequence(dialog.getElement(), KeyCodes.ESC);
    assertTrue(wasCalled);
  },

  /**
     @suppress {missingProperties,visibility} suppression added to enable type
     checking
   */
  testHideButtons_afterRender() {
    dialog.setButtonSet(Dialog.ButtonSet.OK);
    assertTrue(style.isElementShown(dialog.buttonEl_));
    dialog.setButtonSet(null);
    assertFalse(style.isElementShown(dialog.buttonEl_));
    dialog.setButtonSet(Dialog.ButtonSet.OK);
    assertTrue(style.isElementShown(dialog.buttonEl_));
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testHideButtons_beforeRender() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.setButtonSet(null);
    dialog.setVisible(true);
    assertFalse(style.isElementShown(dialog.buttonEl_));
    dialog.setButtonSet(Dialog.ButtonSet.OK);
    assertTrue(style.isElementShown(dialog.buttonEl_));
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testHideButtons_beforeDecorate() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.setButtonSet(null);
    dialog.decorate(decorateTarget);
    dialog.setVisible(true);
    assertFalse(style.isElementShown(dialog.buttonEl_));
    dialog.setButtonSet(Dialog.ButtonSet.OK);
    assertTrue(style.isElementShown(dialog.buttonEl_));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAriaLabelledBy_render() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.render();
    assertTrue(!!dialog.getTitleTextElement().id);
    assertNotNull(dialog.getElement());
    assertEquals(
        dialog.getTitleTextElement().id,
        aria.getState(dialog.getElement(), 'labelledby'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAriaLabelledBy_decorate() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.decorate(decorateTarget);
    dialog.setVisible(true);
    assertTrue(!!dialog.getTitleTextElement().id);
    assertNotNull(dialog.getElement());
    assertEquals(
        dialog.getTitleTextElement().id,
        aria.getState(dialog.getElement(), 'labelledby'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPreferredAriaRole_renderDefault() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.render();
    assertNotNull(dialog.getElement());
    assertEquals(
        dialog.getPreferredAriaRole(), aria.getRole(dialog.getElement()));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPreferredAriaRole_decorateDefault() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.decorate(decorateTarget);
    assertNotNull(dialog.getElement());
    assertEquals(
        dialog.getPreferredAriaRole(), aria.getRole(dialog.getElement()));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPreferredAriaRole_renderOverride() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.setPreferredAriaRole(Role.ALERTDIALOG);
    dialog.render();
    assertNotNull(dialog.getElement());
    assertEquals(Role.ALERTDIALOG, aria.getRole(dialog.getElement()));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPreferredAriaRole_decorateOverride() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.setPreferredAriaRole(Role.ALERTDIALOG);
    dialog.decorate(decorateTarget);
    assertNotNull(dialog.getElement());
    assertEquals(Role.ALERTDIALOG, aria.getRole(dialog.getElement()));
  },

  testIsAriaDescribedByContent_falseSetsNoLabelForAriaDescribedBy() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.setIsAriaDescribedByContent(false);
    dialog.setTextContent('hello world');
    dialog.createDom();
    assertNotNull(dialog.getElement());
    assertFalse(
        aria.hasState(dialog.getElementStrict(), aria.State.DESCRIBEDBY));
  },

  testIsAriaDescribedByContent_trueSetsAriaDescribedByLabelToContentId() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.setIsAriaDescribedByContent(true);
    dialog.setTextContent('hello world');
    dialog.createDom();
    assertNotNull(dialog.getElement());
    assertTrue(
        aria.hasState(dialog.getElementStrict(), aria.State.DESCRIBEDBY));
    assertEquals(
        dialog.getContentElement().id,
        aria.getState(dialog.getElementStrict(), aria.State.DESCRIBEDBY));
  },

  testDefaultOpacityIsAppliedOnRender() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.render();
    assertEquals(0.5, style.getOpacity(dialog.getBackgroundElement()));
  },

  testDefaultOpacityIsAppliedOnDecorate() {
    dialog.dispose();

    dialog = new Dialog();
    dialog.decorate(decorateTarget);
    assertEquals(0.5, style.getOpacity(dialog.getBackgroundElement()));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDraggableStyle() {
    assertTrue(
        'draggable CSS class is set',
        classlist.contains(dialog.titleEl_, 'modal-dialog-title-draggable'));
    dialog.setDraggable(false);
    assertFalse(
        'draggable CSS class is removed',
        classlist.contains(dialog.titleEl_, 'modal-dialog-title-draggable'));
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testDraggingLifecycle() {
    dialog.dispose();

    dialog = new Dialog();
    /** @suppress {visibility} suppression added to enable type checking */
    dialog.setDraggerLimits_ = recordFunction();
    dialog.createDom();
    assertNull('dragger is not created in createDom', dialog.dragger_);

    dialog.setVisible(true);
    assertNotNull(
        'dragger is created when the dialog is rendered', dialog.dragger_);

    assertNull(
        'dragging limits are not set just before dragging',
        dialog.setDraggerLimits_.getLastCall());
    testingEvents.fireMouseDownEvent(dialog.titleEl_);
    assertNotNull(
        'dragging limits are set', dialog.setDraggerLimits_.getLastCall());

    dialog.exitDocument();
    assertNull('dragger is cleaned up in exitDocument', dialog.dragger_);
  },

  testDisposingVisibleDialogWithTransitionsDoesNotThrowException() {
    const transition = css3.fadeIn(dialog.getElement(), 0.1 /* duration */);

    dialog.setTransition(transition, transition, transition, transition);
    dialog.setVisible(true);
    dialog.dispose();
    // Nothing to assert. We only want to ensure that there is no error.
  },

  testEventsDuringAnimation() {
    dialog.dispose();
    dialog = new Dialog();
    dialog.render();
    dialog.setTransition(
        css3.fadeIn(dialog.getElement(), 1),
        css3.fadeIn(dialog.getBackgroundElement(), 1),
        css3.fadeOut(dialog.getElement(), 1),
        css3.fadeOut(dialog.getBackgroundElement(), 1));
    dialog.setVisible(true);
    assertTrue(dialog.isVisible());

    const buttonSet = dialog.getButtonSet();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const button = buttonSet.getButton(buttonSet.getDefault());

    // The button event fires while the animation is still going.
    testingEvents.fireClickSequence(button);
    mockClock.tick(2000);
    assertFalse(dialog.isVisible());
  },

  testHtmlContent() {
    dialog.setSafeHtmlContent(
        testing.newSafeHtmlForTest('<span class="theSpan">Hello</span>'));
    const spanEl = dom.getElementByClass('theSpan', dialog.getContentElement());
    assertEquals('Hello', dom.getTextContent(spanEl));
    assertEquals('<span class="theSpan">Hello</span>', dialog.getContent());
    assertEquals(
        '<span class="theSpan">Hello</span>',
        SafeHtml.unwrap(dialog.getSafeHtmlContent()));
  },

  testSetTextContent() {
    dialog.setTextContent('Dinner <3\nTogether');
    assertEquals('Dinner &lt;3<br>Together', dialog.getContent());
  },

  testFocus() {
    // Focus should go to the dialog element.
    document.body.focus();
    dialog.focus();
    assertEquals(dialog.getElement(), document.activeElement);
  },
});
