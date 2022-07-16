/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.CheckboxTest');
goog.setTestOnly();

const Checkbox = goog.require('goog.ui.Checkbox');
const CheckboxRenderer = goog.require('goog.ui.CheckboxRenderer');
const Component = goog.require('goog.ui.Component');
const ControlRenderer = goog.require('goog.ui.ControlRenderer');
const KeyCodes = goog.require('goog.events.KeyCodes');
const Role = goog.require('goog.a11y.aria.Role');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const decorate = goog.require('goog.ui.decorate');
const dom = goog.require('goog.dom');
const googEvents = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let checkbox;

/**
 * A subclass of `CheckboxRenderer` that overrides `getKeyEventTarget` for
 * testing purposes.
 */
class TestCheckboxRenderer extends CheckboxRenderer {
  /** @param {!Element} keyEventTarget */
  constructor(keyEventTarget) {
    super();

    /** @private @const {!Element} */
    this.keyEventTarget_ = keyEventTarget;
  }

  /** @override */
  getKeyEventTarget() {
    return this.keyEventTarget_;
  }
}

/**
 * @suppress {strictMissingProperties} suppression added to enable type
 * checking
 */
function validateCheckBox(span, state, disabled = undefined) {
  const testCheckbox = decorate(span);
  assertNotNull('checkbox created', testCheckbox);
  assertEquals('decorate was successful', Checkbox, testCheckbox.constructor);
  assertEquals(
      `checkbox state should be: ${state}`, state, testCheckbox.getChecked());
  assertEquals(
      'checkbox is ' + (!disabled ? 'enabled' : 'disabled'), !disabled,
      testCheckbox.isEnabled());
  testCheckbox.dispose();
}

testSuite({
  setUp() {
    checkbox = new Checkbox();
  },

  tearDown() {
    checkbox.dispose();
  },

  testClassNames() {
    checkbox.createDom();

    checkbox.setChecked(false);
    assertSameElements(
        'classnames of unchecked checkbox',
        ['goog-checkbox', 'goog-checkbox-unchecked'],
        classlist.get(checkbox.getElement()));

    checkbox.setChecked(true);
    assertSameElements(
        'classnames of checked checkbox',
        ['goog-checkbox', 'goog-checkbox-checked'],
        classlist.get(checkbox.getElement()));

    checkbox.setChecked(null);
    assertSameElements(
        'classnames of partially checked checkbox',
        ['goog-checkbox', 'goog-checkbox-undetermined'],
        classlist.get(checkbox.getElement()));

    checkbox.setEnabled(false);
    assertSameElements(
        'classnames of partially checked disabled checkbox',
        [
          'goog-checkbox', 'goog-checkbox-undetermined',
          'goog-checkbox-disabled'
        ],
        classlist.get(checkbox.getElement()));
  },

  testIsEnabled() {
    assertTrue('enabled by default', checkbox.isEnabled());
    checkbox.setEnabled(false);
    assertFalse('has been disabled', checkbox.isEnabled());
  },

  testSetEnabled_setsTabIndexOnKeyEventTargetOnly() {
    const keyEventTarget = dom.createElement(TagName.DIV);
    document.body.appendChild(keyEventTarget);

    try {
      checkbox = new Checkbox(
          /* opt_checked= */ undefined, /* opt_domHelper= */ undefined,
          new TestCheckboxRenderer(keyEventTarget));
      checkbox.createDom();

      checkbox.setEnabled(false);
      assertNull(
          'Checkbox element must not have a tabIndex',
          checkbox.getElement().getAttribute('tabIndex'));
      assertFalse(
          'Checkbox\'s key event target element must not support keyboard focus',
          dom.isFocusableTabIndex(keyEventTarget));

      checkbox.setEnabled(true);
      assertNull(
          'Checkbox element must not have a tabIndex',
          checkbox.getElement().getAttribute('tabIndex'));
      assertTrue(
          'Checkbox\'s key event target element must support keyboard focus',
          dom.isFocusableTabIndex(keyEventTarget));

    } finally {
      document.body.removeChild(keyEventTarget);
    }
  },

  testCheckedState() {
    assertTrue(
        'unchecked by default',
        !checkbox.isChecked() && checkbox.isUnchecked() &&
            !checkbox.isUndetermined());

    checkbox.setChecked(true);
    assertTrue(
        'set to checked',
        checkbox.isChecked() && !checkbox.isUnchecked() &&
            !checkbox.isUndetermined());

    checkbox.setChecked(null);
    assertTrue(
        'set to partially checked',
        !checkbox.isChecked() && !checkbox.isUnchecked() &&
            checkbox.isUndetermined());
  },

  testToggle() {
    checkbox.setChecked(null);
    checkbox.toggle();
    assertTrue('undetermined -> checked', checkbox.getChecked());
    checkbox.toggle();
    assertFalse('checked -> unchecked', checkbox.getChecked());
    checkbox.toggle();
    assertTrue('unchecked -> checked', checkbox.getChecked());
  },

  testEvents() {
    checkbox.render();

    let events = [];
    googEvents.listen(
        checkbox,
        [
          Component.EventType.ACTION,
          Component.EventType.CHECK,
          Component.EventType.UNCHECK,
          Component.EventType.CHANGE,
        ],
        (e) => {
          events.push(e.type);
        });

    checkbox.setEnabled(false);
    testingEvents.fireClickSequence(checkbox.getElement());
    assertArrayEquals('disabled => no events', [], events);
    assertFalse('checked state did not change', checkbox.getChecked());
    events = [];

    checkbox.setEnabled(true);
    testingEvents.fireClickSequence(checkbox.getElement());
    assertArrayEquals(
        'ACTION+CHECK+CHANGE fired',
        [
          Component.EventType.ACTION,
          Component.EventType.CHECK,
          Component.EventType.CHANGE,
        ],
        events);
    assertTrue('checkbox became checked', checkbox.getChecked());
    events = [];

    testingEvents.fireClickSequence(checkbox.getElement());
    assertArrayEquals(
        'ACTION+UNCHECK+CHANGE fired',
        [
          Component.EventType.ACTION,
          Component.EventType.UNCHECK,
          Component.EventType.CHANGE,
        ],
        events);
    assertFalse('checkbox became unchecked', checkbox.getChecked());
    events = [];

    googEvents.listen(checkbox, Component.EventType.CHECK, (e) => {
      e.preventDefault();
    });
    testingEvents.fireClickSequence(checkbox.getElement());
    assertArrayEquals(
        'ACTION+CHECK fired',
        [Component.EventType.ACTION, Component.EventType.CHECK], events);
    assertFalse('toggling has been prevented', checkbox.getChecked());
  },

  testCheckboxAriaLabelledby() {
    const label = dom.createElement(TagName.DIV);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const label2 = dom.createElement(TagName.DIV, {id: checkbox.makeId('foo')});
    document.body.appendChild(label);
    document.body.appendChild(label2);
    try {
      checkbox.setChecked(false);
      checkbox.setLabel(label);
      checkbox.render(label);
      assertNotNull(checkbox.getElement());
      assertEquals(
          label.id, aria.getState(checkbox.getElement(), State.LABELLEDBY));

      checkbox.setLabel(label2);
      assertEquals(
          label2.id, aria.getState(checkbox.getElement(), State.LABELLEDBY));
    } finally {
      document.body.removeChild(label);
      document.body.removeChild(label2);
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testLabel() {
    const label = dom.createElement(TagName.DIV);
    document.body.appendChild(label);
    try {
      checkbox.setChecked(false);
      checkbox.setLabel(label);
      checkbox.render(label);

      // Clicking on label toggles checkbox.
      testingEvents.fireClickSequence(label);
      assertTrue(
          'checkbox toggled if the label is clicked', checkbox.getChecked());
      testingEvents.fireClickSequence(checkbox.getElement());
      assertFalse('checkbox toggled if it is clicked', checkbox.getChecked());

      // Test that mouse events on the label have the correct effect on the
      // checkbox state when it is enabled.
      checkbox.setEnabled(true);
      testingEvents.fireMouseOverEvent(label);
      assertTrue(checkbox.hasState(Component.State.HOVER));
      assertContains(
          'checkbox gets hover state on mouse over', 'goog-checkbox-hover',
          classlist.get(checkbox.getElement()));
      testingEvents.fireMouseDownEvent(label);
      assertTrue(checkbox.hasState(Component.State.ACTIVE));
      assertContains(
          'checkbox gets active state on label mousedown',
          'goog-checkbox-active', classlist.get(checkbox.getElement()));
      testingEvents.fireMouseOutEvent(checkbox.getElement());
      assertFalse(checkbox.hasState(Component.State.HOVER));
      assertNotContains(
          'checkbox does not have hover state after mouse out',
          'goog-checkbox-hover', classlist.get(checkbox.getElement()));
      assertFalse(checkbox.hasState(Component.State.ACTIVE));
      assertNotContains(
          'checkbox does not have active state after mouse out',
          'goog-checkbox-active', classlist.get(checkbox.getElement()));

      // Test label mouse events on disabled checkbox.
      checkbox.setEnabled(false);
      testingEvents.fireMouseOverEvent(label);
      assertFalse(checkbox.hasState(Component.State.HOVER));
      assertNotContains(
          'disabled checkbox does not get hover state on mouseover',
          'goog-checkbox-hover', classlist.get(checkbox.getElement()));
      testingEvents.fireMouseDownEvent(label);
      assertFalse(checkbox.hasState(Component.State.ACTIVE));
      assertNotContains(
          'disabled checkbox does not get active state mousedown',
          'goog-checkbox-active', classlist.get(checkbox.getElement()));
      testingEvents.fireMouseOutEvent(checkbox.getElement());
      assertFalse(checkbox.hasState(Component.State.ACTIVE));
      assertNotContains(
          'checkbox does not get stuck in hover state', 'goog-checkbox-hover',
          classlist.get(checkbox.getElement()));

      // Making the label null prevents it from affecting checkbox state.
      checkbox.setEnabled(true);
      checkbox.setLabel(null);
      testingEvents.fireClickSequence(label);
      assertFalse('label element deactivated', checkbox.getChecked());
      testingEvents.fireClickSequence(checkbox.getElement());
      assertTrue('checkbox still active', checkbox.getChecked());
    } finally {
      document.body.removeChild(label);
    }
  },

  testLabel_setAgain() {
    const label = dom.createElement(TagName.DIV);
    document.body.appendChild(label);
    try {
      checkbox.setChecked(false);
      checkbox.setLabel(label);
      checkbox.render(label);

      checkbox.getElement().focus();
      checkbox.setLabel(label);
      assertEquals(
          'checkbox should not have lost focus', checkbox.getElement(),
          document.activeElement);
    } finally {
      document.body.removeChild(label);
    }
  },

  testConstructor() {
    assertEquals(
        'state is unchecked', Checkbox.State.UNCHECKED, checkbox.getChecked());

    const testCheckboxWithState = new Checkbox(Checkbox.State.UNDETERMINED);
    assertNotNull('checkbox created with custom state', testCheckboxWithState);
    assertEquals(
        'checkbox state is undetermined', Checkbox.State.UNDETERMINED,
        testCheckboxWithState.getChecked());
    testCheckboxWithState.dispose();
  },

  testCustomRenderer() {
    const cssClass = 'my-custom-checkbox';
    const renderer =
        ControlRenderer.getCustomRenderer(CheckboxRenderer, cssClass);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const customCheckbox = new Checkbox(undefined, undefined, renderer);
    customCheckbox.createDom();
    assertElementsEquals(
        ['my-custom-checkbox', 'my-custom-checkbox-unchecked'],
        classlist.get(customCheckbox.getElement()));
    customCheckbox.setChecked(true);
    assertElementsEquals(
        ['my-custom-checkbox', 'my-custom-checkbox-checked'],
        classlist.get(customCheckbox.getElement()));
    customCheckbox.setChecked(null);
    assertElementsEquals(
        ['my-custom-checkbox', 'my-custom-checkbox-undetermined'],
        classlist.get(customCheckbox.getElement()));
    customCheckbox.dispose();
  },

  testGetAriaRole() {
    checkbox.createDom();
    assertNotNull(checkbox.getElement());
    assertEquals(
        'Checkbox\'s ARIA role should be \'checkbox\'', Role.CHECKBOX,
        aria.getRole(checkbox.getElement()));
  },

  testCreateDomUpdateAriaState() {
    checkbox.createDom();
    assertNotNull(checkbox.getElement());
    assertEquals(
        'Checkbox must have default false ARIA state aria-checked', 'false',
        aria.getState(checkbox.getElement(), State.CHECKED));

    checkbox.setChecked(Checkbox.State.CHECKED);
    assertEquals(
        'Checkbox must have true ARIA state aria-checked', 'true',
        aria.getState(checkbox.getElement(), State.CHECKED));

    checkbox.setChecked(Checkbox.State.UNCHECKED);
    assertEquals(
        'Checkbox must have false ARIA state aria-checked', 'false',
        aria.getState(checkbox.getElement(), State.CHECKED));

    checkbox.setChecked(Checkbox.State.UNDETERMINED);
    assertEquals(
        'Checkbox must have mixed ARIA state aria-checked', 'mixed',
        aria.getState(checkbox.getElement(), State.CHECKED));
  },

  testDecorateUpdateAriaState() {
    const decorateSpan = dom.getElement('decorate');
    checkbox.decorate(decorateSpan);

    assertEquals(
        'Checkbox must have default false ARIA state aria-checked', 'false',
        aria.getState(checkbox.getElement(), State.CHECKED));

    checkbox.setChecked(Checkbox.State.CHECKED);
    assertEquals(
        'Checkbox must have true ARIA state aria-checked', 'true',
        aria.getState(checkbox.getElement(), State.CHECKED));

    checkbox.setChecked(Checkbox.State.UNCHECKED);
    assertEquals(
        'Checkbox must have false ARIA state aria-checked', 'false',
        aria.getState(checkbox.getElement(), State.CHECKED));

    checkbox.setChecked(Checkbox.State.UNDETERMINED);
    assertEquals(
        'Checkbox must have mixed ARIA state aria-checked', 'mixed',
        aria.getState(checkbox.getElement(), State.CHECKED));
  },

  testSpaceKey() {
    const normalSpan = dom.getElement('normal');

    checkbox.decorate(normalSpan);
    assertEquals(
        'default state is unchecked', Checkbox.State.UNCHECKED,
        checkbox.getChecked());
    testingEvents.fireKeySequence(normalSpan, KeyCodes.SPACE);
    assertEquals(
        'SPACE toggles checkbox to be checked', Checkbox.State.CHECKED,
        checkbox.getChecked());
    testingEvents.fireKeySequence(normalSpan, KeyCodes.SPACE);
    assertEquals(
        'another SPACE toggles checkbox to be unchecked',
        Checkbox.State.UNCHECKED, checkbox.getChecked());

    // Enter for example doesn't work
    testingEvents.fireKeySequence(normalSpan, KeyCodes.ENTER);
    assertEquals(
        'Enter leaves checkbox unchecked', Checkbox.State.UNCHECKED,
        checkbox.getChecked());
  },

  testSpaceKeyFiresEvents() {
    const normalSpan = dom.getElement('normal');

    checkbox.decorate(normalSpan);
    let events = [];
    googEvents.listen(
        checkbox,
        [
          Component.EventType.ACTION,
          Component.EventType.CHECK,
          Component.EventType.UNCHECK,
          Component.EventType.CHANGE,
        ],
        (e) => {
          events.push(e.type);
        });

    assertEquals(
        'Unexpected default state.', Checkbox.State.UNCHECKED,
        checkbox.getChecked());
    testingEvents.fireKeySequence(normalSpan, KeyCodes.SPACE);
    assertArrayEquals(
        'Unexpected events fired when checking with spacebar.',
        [
          Component.EventType.ACTION,
          Component.EventType.CHECK,
          Component.EventType.CHANGE,
        ],
        events);
    assertEquals(
        'Unexpected state after checking.', Checkbox.State.CHECKED,
        checkbox.getChecked());

    events = [];
    testingEvents.fireKeySequence(normalSpan, KeyCodes.SPACE);
    assertArrayEquals(
        'Unexpected events fired when unchecking with spacebar.',
        [
          Component.EventType.ACTION,
          Component.EventType.UNCHECK,
          Component.EventType.CHANGE,
        ],
        events);
    assertEquals(
        'Unexpected state after unchecking.', Checkbox.State.UNCHECKED,
        checkbox.getChecked());

    events = [];
    googEvents.listenOnce(checkbox, Component.EventType.CHECK, (e) => {
      e.preventDefault();
    });
    testingEvents.fireKeySequence(normalSpan, KeyCodes.SPACE);
    assertArrayEquals(
        'Unexpected events fired when checking with spacebar and ' +
            'the check event is cancelled.',
        [Component.EventType.ACTION, Component.EventType.CHECK], events);
    assertEquals(
        'Unexpected state after check event is cancelled.',
        Checkbox.State.UNCHECKED, checkbox.getChecked());
  },

  testDecorate() {
    const normalSpan = dom.getElement('normal');
    const checkedSpan = dom.getElement('checked');
    const uncheckedSpan = dom.getElement('unchecked');
    const undeterminedSpan = dom.getElement('undetermined');
    const disabledSpan = dom.getElement('disabled');

    validateCheckBox(normalSpan, Checkbox.State.UNCHECKED);
    validateCheckBox(checkedSpan, Checkbox.State.CHECKED);
    validateCheckBox(uncheckedSpan, Checkbox.State.UNCHECKED);
    validateCheckBox(undeterminedSpan, Checkbox.State.UNDETERMINED);
    validateCheckBox(disabledSpan, Checkbox.State.UNCHECKED, true);
  },

  testSetAriaLabel() {
    assertNull(
        'Checkbox must not have aria label by default',
        checkbox.getAriaLabel());
    checkbox.setAriaLabel('Checkbox 1');
    checkbox.render();
    const el = checkbox.getElementStrict();
    assertEquals(
        'Checkbox element must have expected aria-label', 'Checkbox 1',
        el.getAttribute('aria-label'));
    assertEquals(
        'Checkbox element must have expected aria-role', 'checkbox',
        el.getAttribute('role'));
    checkbox.setAriaLabel('Checkbox 2');
    assertEquals(
        'Checkbox element must have updated aria-label', 'Checkbox 2',
        el.getAttribute('aria-label'));
    assertEquals(
        'Checkbox element must have expected aria-role', 'checkbox',
        el.getAttribute('role'));
  },
});
