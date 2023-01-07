/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.LabelInputTest');
goog.setTestOnly();

const EventType = goog.require('goog.events.EventType');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const LabelInput = goog.require('goog.ui.LabelInput');
const MockClock = goog.require('goog.testing.MockClock');
const State = goog.require('goog.a11y.aria.State');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let labelInput;
let mockClock;

function assertLabelValue(labelInput, expectedLabel) {
  assertEquals(
      `label should have aria-label attribute '${expectedLabel}'`,
      expectedLabel, aria.getState(labelInput.getElement(), State.LABEL));
  // When browsers support the placeholder attribute, we use that instead of
  // the value property - and this test will fail.
  if (!isPlaceholderSupported()) {
    assertEquals(
        'label is updated', expectedLabel, labelInput.getElement().value);
  } else {
    assertEquals('value is empty', '', labelInput.getElement().value);
  }
}

/**
 * @suppress {strictMissingProperties} suppression added to enable type
 * checking
 */
function isPlaceholderSupported() {
  if (dom.getElement('i').placeholder != null) {
    return true;
  }
}

testSuite({
  setUp() {
    mockClock = new MockClock(true);
  },

  tearDown() {
    mockClock.dispose();
    labelInput.dispose();
  },

  testGetLabel() {
    labelInput = new LabelInput();
    assertEquals('no label', '', labelInput.getLabel());

    labelInput = new LabelInput('search');
    assertEquals('label is given in the ctor', 'search', labelInput.getLabel());
  },

  testSetLabel() {
    labelInput = new LabelInput();
    labelInput.setLabel('search');
    assertEquals('label is set', 'search', labelInput.getLabel());

    labelInput.createDom();
    labelInput.enterDocument();
    mockClock.tick(10);
    assertNotNull(labelInput.getElement());
    assertLabelValue(labelInput, 'search');

    labelInput.setLabel('new label');
    assertLabelValue(labelInput, 'new label');
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testPlaceholderAttribute() {
    labelInput = new LabelInput();
    labelInput.setLabel('search');

    // Some browsers don't support the placeholder attribute, in which case we
    // this test will fail.
    if (!isPlaceholderSupported()) {
      return;
    }

    labelInput.createDom();
    labelInput.enterDocument();
    mockClock.tick(10);
    assertEquals(
        'label should have placeholder attribute \'search\'', 'search',
        labelInput.getElement().placeholder);

    labelInput.setLabel('new label');
    assertEquals(
        'label should have aria-label attribute \'new label\'', 'new label',
        labelInput.getElement().placeholder);
  },

  /**
     @suppress {strictMissingProperties,checkTypes} suppression added to enable
     type checking
   */
  testDecorateElementWithExistingPlaceholderAttribute() {
    labelInput = new LabelInput();
    labelInput.setLabel('search');

    labelInput.decorate(dom.getElement('p'));
    labelInput.enterDocument();
    mockClock.tick(10);

    // The presence of an existing placeholder doesn't necessarily mean the
    // browser supports placeholders. Make sure labels are used for browsers
    // without placeholder support:
    if (isPlaceholderSupported()) {
      assertEquals(
          'label should have placeholder attribute \'search\'', 'search',
          labelInput.getElement().placeholder);
    } else {
      assertNotNull(labelInput.getElement());
      assertEquals(
          'label is rendered', 'search', labelInput.getElement().value);
      assertEquals(
          'label should have aria-label attribute \'search\'', 'search',
          aria.getState(labelInput.getElement(), State.LABEL));
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testDecorateElementWithFocus() {
    labelInput = new LabelInput();
    labelInput.setLabel('search');

    const decoratedElement = dom.getElement('i');

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    decoratedElement.value = '';
    decoratedElement.focus();

    labelInput.decorate(decoratedElement);
    labelInput.enterDocument();
    mockClock.tick(10);

    assertEquals(
        'label for pre-focused input should not have LABEL_CLASS_NAME', -1,
        labelInput.getElement().className.indexOf(
            labelInput.labelCssClassName));

    if (!isPlaceholderSupported()) {
      assertEquals(
          'label rendered for pre-focused element', '',
          labelInput.getElement().value);
      // NOTE(user): element.blur() doesn't seem to trigger the BLUR event
      // in IE in the test environment. This could be related to the IE issues
      // with testClassName() below.
      events.fireBrowserEvent(
          new GoogTestingEvent(EventType.BLUR, decoratedElement));
      mockClock.tick(10);
      assertEquals(
          'label not rendered for blurred element', 'search',
          labelInput.getElement().value);
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testDecorateElementWithFocusDelay() {
    if (isPlaceholderSupported()) {
      return;  // Delay only affects the older browsers.
    }
    const placeholder = 'search';

    labelInput = new LabelInput();
    labelInput.setLabel(placeholder);
    const delay = 150;
    /** @suppress {visibility} suppression added to enable type checking */
    labelInput.labelRestoreDelayMs = delay;

    const decoratedElement = dom.getElement('i');

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    decoratedElement.value = '';
    decoratedElement.focus();

    labelInput.decorate(decoratedElement);
    labelInput.enterDocument();
    // wait for all initial setup to settle
    mockClock.tick(delay);

    // NOTE(user): element.blur() doesn't seem to trigger the BLUR event in
    // IE in the test environment. This could be related to the IE issues with
    // testClassName() below.
    events.fireBrowserEvent(
        new GoogTestingEvent(EventType.BLUR, decoratedElement));

    mockClock.tick(delay - 1);
    assertEquals(
        'label should not be restored before labelRestoreDelay', '',
        labelInput.getElement().value);

    mockClock.tick(1);
    assertEquals(
        'label not rendered for blurred element with labelRestoreDelay',
        placeholder, labelInput.getElement().value);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testClassName() {
    labelInput = new LabelInput();

    // EDGE/IE always fails this test, suspect it is a focus issue.
    if (userAgent.EDGE_OR_IE) {
      return;
    }
    // FF does not perform focus if the window is not active in the first place.
    if (userAgent.GECKO && document.hasFocus && !document.hasFocus()) {
      return;
    }

    labelInput.decorate(dom.getElement('i'));
    labelInput.setLabel('search');

    const el = labelInput.getElement();
    assertTrue(
        'label before focus should have LABEL_CLASS_NAME',
        classlist.contains(el, labelInput.labelCssClassName));

    labelInput.getElement().focus();

    assertFalse(
        'label after focus should not have LABEL_CLASS_NAME',
        classlist.contains(el, labelInput.labelCssClassName));

    labelInput.getElement().blur();

    assertTrue(
        'label after blur should have LABEL_CLASS_NAME',
        classlist.contains(el, labelInput.labelCssClassName));
  },

  testEnable() {
    labelInput = new LabelInput();
    labelInput.createDom();
    labelInput.enterDocument();

    const labelElement = labelInput.getElement();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const disabledClass =
        goog.getCssName(labelInput.labelCssClassName, 'disabled');

    assertTrue('label should be enabled', labelInput.isEnabled());
    assertFalse(
        'label should not have the disabled class',
        classlist.contains(labelElement, disabledClass));

    labelInput.setEnabled(false);
    assertFalse('label should be disabled', labelInput.isEnabled());
    assertTrue(
        'label should have the disabled class',
        classlist.contains(labelElement, disabledClass));

    labelInput.setEnabled(true);
    assertTrue('label should be enabled', labelInput.isEnabled());
    assertFalse(
        'label should not have the disabled class',
        classlist.contains(labelElement, disabledClass));
  },
});
