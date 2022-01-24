/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ControlRendererTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const Control = goog.require('goog.ui.Control');
const ControlRenderer = goog.require('goog.ui.ControlRenderer');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const NodeType = goog.require('goog.dom.NodeType');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const Role = goog.require('goog.a11y.aria.Role');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const googObject = goog.require('goog.object');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let control;
let controlRenderer;
let testRenderer;
let propertyReplacer;
let sandbox;
let expectedFailures;

/**
 * A subclass of ControlRenderer that overrides `getAriaRole` and
 * `getStructuralCssClass` for testing purposes.
 */
class TestRenderer extends ControlRenderer {
  constructor() {
    super();
    ControlRenderer.call(this);
  }

  /** @override */
  getAriaRole() {
    return Role.BUTTON;
  }

  /** @override */
  getCssClass() {
    return TestRenderer.CSS_CLASS;
  }

  /** @override */
  getStructuralCssClass() {
    return 'goog-base';
  }

  /**
   * @override
   * @suppress {checkTypes} suppression added to enable type checking
   */
  getIe6ClassCombinations() {
    return TestRenderer.IE6_CLASS_COMBINATIONS;
  }
}

goog.addSingletonGetter(TestRenderer);

TestRenderer.CSS_CLASS = 'goog-button';

TestRenderer.IE6_CLASS_COMBINATIONS = [
  ['combined', 'goog-base-hover', 'goog-button'],
  ['combined', 'goog-base-disabled', 'goog-button'],
  ['combined', 'combined2', 'goog-base-hover', 'goog-base-rtl', 'goog-button'],
];

/** @return {boolean} Whether we're on Mac Safari 3.x. */
function isMacSafari3() {
  return false;
}

/** @return {boolean} Whether we're on IE6 or lower. */
function isIe6() {
  return false;
}

testSuite({
  setUpPage() {
    sandbox = dom.getElement('sandbox');
    expectedFailures = new ExpectedFailures();
  },

  setUp() {
    control = new Control('Hello');
    controlRenderer = ControlRenderer.getInstance();
    testRenderer = TestRenderer.getInstance();
    propertyReplacer = new PropertyReplacer();
  },

  tearDown() {
    propertyReplacer.reset();
    control.dispose();
    expectedFailures.handleTearDown();
    control = null;
    controlRenderer = null;
    testRenderer = null;
    dom.removeChildren(sandbox);
  },

  testConstructor() {
    assertNotNull(
        'ControlRenderer singleton instance must not be null', controlRenderer);
    assertNotNull(
        'TestRenderer singleton instance must not be null', testRenderer);
  },

  testGetCustomRenderer() {
    const cssClass = 'special-css-class';
    const renderer =
        ControlRenderer.getCustomRenderer(ControlRenderer, cssClass);
    assertEquals(
        'Renderer should have returned the custom CSS class.', cssClass,
        renderer.getCssClass());
  },

  testGetAriaRole() {
    assertUndefined(
        'ControlRenderer\'s ARIA role must be undefined',
        controlRenderer.getAriaRole());
    assertEquals(
        'TestRenderer\'s ARIA role must have expected value', Role.BUTTON,
        testRenderer.getAriaRole());
  },

  testCreateDom() {
    assertHTMLEquals(
        'ControlRenderer must create correct DOM',
        '<div class="goog-control">Hello</div>',
        dom.getOuterHtml(controlRenderer.createDom(control)));
    assertHTMLEquals(
        'TestRenderer must create correct DOM',
        '<div class="goog-button goog-base">Hello</div>',
        dom.getOuterHtml(testRenderer.createDom(control)));
  },

  testGetContentElement() {
    assertEquals(
        'getContentElement() must return its argument', sandbox,
        controlRenderer.getContentElement(sandbox));
  },

  testEnableExtraClassName() {
    // enableExtraClassName() must be a no-op if control has no DOM.
    controlRenderer.enableExtraClassName(control, 'foo', true);

    control.createDom();
    const element = control.getElement();

    controlRenderer.enableExtraClassName(control, 'foo', true);
    assertSameElements(
        'Extra class name must have been added', ['goog-control', 'foo'],
        classlist.get(element));

    controlRenderer.enableExtraClassName(control, 'foo', true);
    assertSameElements(
        'Enabling existing extra class name must be a no-op',
        ['goog-control', 'foo'], classlist.get(element));

    controlRenderer.enableExtraClassName(control, 'bar', false);
    assertSameElements(
        'Disabling nonexistent class name must be a no-op',
        ['goog-control', 'foo'], classlist.get(element));

    controlRenderer.enableExtraClassName(control, 'foo', false);
    assertSameElements(
        'Extra class name must have been removed', ['goog-control'],
        classlist.get(element));
  },

  testCanDecorate() {
    assertTrue('canDecorate() must return true', controlRenderer.canDecorate());
  },

  testDecorate() {
    sandbox.innerHTML = '<div id="foo">Hello, world!</div>';
    const foo = dom.getElement('foo');
    const element = controlRenderer.decorate(control, foo);

    assertEquals('decorate() must return its argument', foo, element);
    assertEquals('Decorated control\'s ID must be set', 'foo', control.getId());
    assertTrue(
        'Decorated control\'s content must be a text node',
        control.getContent().nodeType == NodeType.TEXT);
    assertEquals(
        'Decorated control\'s content must have expected value',
        'Hello, world!', control.getContent().nodeValue);
    assertEquals(
        'Decorated control\'s state must be as expected', 0x00,
        control.getState());
    assertSameElements(
        'Decorated element\'s classes must be as expected', ['goog-control'],
        classlist.get(element));
  },

  testDecorateComplexDom() {
    sandbox.innerHTML = '<div id="foo"><i>Hello</i>,<b>world</b>!</div>';
    const foo = dom.getElement('foo');
    const element = controlRenderer.decorate(control, foo);

    assertEquals('decorate() must return its argument', foo, element);
    assertEquals('Decorated control\'s ID must be set', 'foo', control.getId());
    assertTrue(
        'Decorated control\'s content must be an array',
        Array.isArray(control.getContent()));
    assertEquals(
        'Decorated control\'s content must have expected length', 4,
        control.getContent().length);
    assertEquals(
        'Decorated control\'s state must be as expected', 0x00,
        control.getState());
    assertSameElements(
        'Decorated element\'s classes must be as expected', ['goog-control'],
        classlist.get(element));
  },

  testDecorateWithClasses() {
    sandbox.innerHTML =
        '<div id="foo" class="app goog-base-disabled goog-base-hover"></div>';
    const foo = dom.getElement('foo');

    control.addClassName('extra');
    const element = testRenderer.decorate(control, foo);

    assertEquals('decorate() must return its argument', foo, element);
    assertEquals('Decorated control\'s ID must be set', 'foo', control.getId());
    assertNull(
        'Decorated control\'s content must be null', control.getContent());
    assertEquals(
        'Decorated control\'s state must be as expected',
        Component.State.DISABLED | Component.State.HOVER, control.getState());
    assertSameElements(
        'Decorated element\'s classes must be as expected',
        [
          'app',
          'extra',
          'goog-base',
          'goog-base-disabled',
          'goog-base-hover',
          'goog-button',
        ],
        classlist.get(element));
  },

  testDecorateOptimization() {
    // Temporarily replace goog.dom.classlist.set().
    propertyReplacer.set(classlist, 'set', () => {
      fail('goog.dom.classlist.set() must not be called');
    });

    // Since foo has all required classes, goog.dom.classlist.set() must not be
    // called at all.
    sandbox.innerHTML = '<div id="foo" class="goog-control">Foo</div>';
    controlRenderer.decorate(control, dom.getElement('foo'));

    // Since bar has all required classes, goog.dom.classlist.set() must not be
    // called at all.
    sandbox.innerHTML = '<div id="bar" class="goog-base goog-button">Bar' +
        '</div>';
    testRenderer.decorate(control, dom.getElement('bar'));

    // Since baz has all required classes, goog.dom.classlist.set() must not be
    // called at all.
    sandbox.innerHTML = '<div id="baz" class="goog-base goog-button ' +
        'goog-button-disabled">Baz</div>';
    testRenderer.decorate(control, dom.getElement('baz'));
  },

  testInitializeDom() {
    const renderer = new ControlRenderer();

    // Replace setRightToLeft().
    renderer.setRightToLeft = () => {
      fail('setRightToLeft() must not be called');
    };

    // When a control with default render direction enters the document,
    // setRightToLeft() must not be called.
    control.setRenderer(renderer);
    control.render(sandbox);


    assertTrue(
        'Enabled, visible, focusable control must have tab index',
        dom.isFocusableTabIndex(control.getElement()));
  },

  testInitializeDomDecorated() {
    const renderer = new ControlRenderer();

    // Replace setRightToLeft().
    renderer.setRightToLeft = () => {
      fail('setRightToLeft() must not be called');
    };

    sandbox.innerHTML = '<div id="foo" class="goog-control">Foo</div>';

    // When a control with default render direction enters the document,
    // setRightToLeft() must not be called.
    control.setRenderer(renderer);
    control.decorate(dom.getElement('foo'));

    assertTrue(
        'Enabled, visible, focusable control must have tab index',
        dom.isFocusableTabIndex(control.getElement()));

  },

  testInitializeDomDisabledBiDi() {
    const renderer = new ControlRenderer();

    // Replace setFocusable().
    renderer.setFocusable = () => {
      fail('setFocusable() must not be called');
    };

    // When a disabled control enters the document, setFocusable() must not
    // be called.
    control.setEnabled(false);
    control.setRightToLeft(true);
    control.setRenderer(renderer);
    control.render(sandbox);

    // When a right-to-left control enters the document, special stying must
    // be applied.
    assertSameElements(
        'BiDi control must have right-to-left class',
        ['goog-control', 'goog-control-disabled', 'goog-control-rtl'],
        classlist.get(control.getElement()));
  },

  testInitializeDomDisabledBiDiDecorated() {
    const renderer = new ControlRenderer();

    // Replace setFocusable().
    renderer.setFocusable = () => {
      fail('setFocusable() must not be called');
    };

    sandbox.innerHTML = '<div dir="rtl">\n' +
        '  <div id="foo" class="goog-control-disabled">Foo</div>\n' +
        '</div>\n';

    // When a disabled control enters the document, setFocusable() must not
    // be called.
    control.setRenderer(renderer);
    control.decorate(dom.getElement('foo'));

    // When a right-to-left control enters the document, special stying must
    // be applied.
    assertSameElements(
        'BiDi control must have right-to-left class',
        ['goog-control', 'goog-control-disabled', 'goog-control-rtl'],
        classlist.get(control.getElement()));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaRole() {
    sandbox.innerHTML = '<div id="foo">Foo</div><div id="bar">Bar</div>';

    const foo = dom.getElement('foo');
    assertNotNull(foo);
    controlRenderer.setAriaRole(foo);
    assertEvaluatesToFalse('The role should be empty.', aria.getRole(foo));
    const bar = dom.getElement('bar');
    assertNotNull(bar);
    testRenderer.setAriaRole(bar);
    assertEquals(
        'Element must have expected ARIA role', Role.BUTTON, aria.getRole(bar));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStatesHidden() {
    sandbox.innerHTML = '<div id="foo">Foo</div><div id="bar">Bar</div>';
    const foo = dom.getElement('foo');

    control.setVisible(true);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-hidden.', '',
        aria.getState(foo, State.HIDDEN));

    control.setVisible(false);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-hidden.', 'true',
        aria.getState(foo, State.HIDDEN));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStatesDisabled() {
    sandbox.innerHTML = '<div id="foo">Foo</div><div id="bar">Bar</div>';
    const foo = dom.getElement('foo');

    control.setEnabled(true);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-disabled.', '',
        aria.getState(foo, State.DISABLED));

    control.setEnabled(false);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-disabled.', 'true',
        aria.getState(foo, State.DISABLED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStatesSelected() {
    sandbox.innerHTML = '<div id="foo">Foo</div><div id="bar">Bar</div>';
    const foo = dom.getElement('foo');
    control.setSupportedState(Component.State.SELECTED, true);

    control.setSelected(true);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-selected.', 'true',
        aria.getState(foo, State.SELECTED));

    control.setSelected(false);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-selected.', 'false',
        aria.getState(foo, State.SELECTED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStatesChecked() {
    sandbox.innerHTML = '<div id="foo">Foo</div><div id="bar">Bar</div>';
    const foo = dom.getElement('foo');
    control.setSupportedState(Component.State.CHECKED, true);

    control.setChecked(true);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-checked.', 'true',
        aria.getState(foo, State.CHECKED));

    control.setChecked(false);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-checked.', 'false',
        aria.getState(foo, State.CHECKED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStatesExpanded() {
    sandbox.innerHTML = '<div id="foo">Foo</div><div id="bar">Bar</div>';
    const foo = dom.getElement('foo');
    control.setSupportedState(Component.State.OPENED, true);

    control.setOpen(true);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-expanded.', 'true',
        aria.getState(foo, State.EXPANDED));

    control.setOpen(false);
    controlRenderer.setAriaStates(control, foo);

    assertEquals(
        'ControlRenderer did not set aria-expanded.', 'false',
        aria.getState(foo, State.EXPANDED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAllowTextSelection() {
    sandbox.innerHTML = '<div id="foo"><span>Foo</span></div>';
    const foo = dom.getElement('foo');

    controlRenderer.setAllowTextSelection(foo, false);
    assertTrue(
        'Parent element must be unselectable on all browsers',
        style.isUnselectable(foo));
    if (userAgent.IE) {
      assertTrue(
          'On IE and Opera, child element must also be unselectable',
          style.isUnselectable(foo.firstChild));
    } else {
      assertFalse(
          'On browsers other than IE and Opera, the child element ' +
              'must not be unselectable',
          style.isUnselectable(foo.firstChild));
    }

    controlRenderer.setAllowTextSelection(foo, true);
    assertFalse('Parent element must be selectable', style.isUnselectable(foo));
    assertFalse(
        'Child element must be unselectable',
        style.isUnselectable(foo.firstChild));
  },

  testSetRightToLeft() {
    sandbox.innerHTML = '<div id="foo">Foo</div><div id="bar">Bar</div>';

    const foo = dom.getElement('foo');
    controlRenderer.setRightToLeft(foo, true);
    assertSameElements(
        'Element must have right-to-left class applied', ['goog-control-rtl'],
        classlist.get(foo));
    controlRenderer.setRightToLeft(foo, false);
    assertSameElements(
        'Element must not have right-to-left class applied', [],
        classlist.get(foo));

    const bar = dom.getElement('bar');
    testRenderer.setRightToLeft(bar, true);
    assertSameElements(
        'Element must have right-to-left class applied', ['goog-base-rtl'],
        classlist.get(bar));
    testRenderer.setRightToLeft(bar, false);
    assertSameElements(
        'Element must not have right-to-left class applied', [],
        classlist.get(bar));
  },

  testIsFocusable() {
    control.render(sandbox);
    assertTrue(
        'Control\'s key event target must be focusable',
        controlRenderer.isFocusable(control));

  },

  testIsFocusableForNonFocusableControl() {
    control.setSupportedState(Component.State.FOCUSED, false);
    control.render(sandbox);
    assertFalse(
        'Non-focusable control\'s key event target must not be ' +
            'focusable',
        controlRenderer.isFocusable(control));
  },

  testIsFocusableForControlWithoutKeyEventTarget() {
    // Unrendered control has no key event target.
    assertNull(
        'Unrendered control must not have key event target',
        control.getKeyEventTarget());
    assertFalse(
        'isFocusable() must return null if no key event target',
        controlRenderer.isFocusable(control));
  },

  testSetFocusable() {
    control.render(sandbox);
    controlRenderer.setFocusable(control, false);
    assertFalse(
        'Control\'s key event target must not have tab index',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));
    controlRenderer.setFocusable(control, true);
    assertTrue(
        'Control\'s key event target must have focusable tab index',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));
  },

  testSetFocusableForNonFocusableControl() {
    control.setSupportedState(Component.State.FOCUSED, false);
    control.render(sandbox);
    assertFalse(
        'Non-focusable control\'s key event target must not be ' +
            'focusable',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));
    controlRenderer.setFocusable(control, true);
    assertFalse(
        'Non-focusable control\'s key event target must not be ' +
            'focusable, even after calling setFocusable(true)',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));
  },

  testSetVisible() {
    sandbox.innerHTML = '<div id="foo">Foo</div>';
    const foo = dom.getElement('foo');
    assertTrue('Element must be visible', foo.style.display != 'none');
    controlRenderer.setVisible(foo, true);
    assertEquals(
        'ControlRenderer did not set aria-hidden.', 'false',
        aria.getState(foo, State.HIDDEN));
    assertTrue('Element must still be visible', foo.style.display != 'none');
    controlRenderer.setVisible(foo, false);
    assertEquals(
        'ControlRenderer did not set aria-hidden.', 'true',
        aria.getState(foo, State.HIDDEN));
    assertTrue('Element must be hidden', foo.style.display == 'none');
  },

  testSetState() {
    control.setRenderer(testRenderer);
    control.createDom();
    const element = control.getElement();
    assertNotNull(element);
    assertSameElements(
        'Control must have expected class names', ['goog-button', 'goog-base'],
        classlist.get(element));
    assertEquals(
        'Control must not have disabled ARIA state', '',
        aria.getState(element, State.DISABLED));

    testRenderer.setState(control, Component.State.DISABLED, true);
    assertSameElements(
        'Control must have disabled class name',
        ['goog-button', 'goog-base', 'goog-base-disabled'],
        classlist.get(element));
    assertEquals(
        'Control must have disabled ARIA state', 'true',
        aria.getState(element, State.DISABLED));

    testRenderer.setState(control, Component.State.DISABLED, false);
    assertSameElements(
        'Control must no longer have disabled class name',
        ['goog-button', 'goog-base'], classlist.get(element));
    assertEquals(
        'Control must not have disabled ARIA state', 'false',
        aria.getState(element, State.DISABLED));

    testRenderer.setState(control, 0xFFFFFF, true);
    assertSameElements(
        'Class names must be unchanged for invalid state',
        ['goog-button', 'goog-base'], classlist.get(element));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUpdateAriaStateDisabled() {
    control.createDom();
    const element = control.getElement();
    assertNotNull(element);
    controlRenderer.updateAriaState(element, Component.State.DISABLED, true);
    assertEquals(
        'Control must have disabled ARIA state', 'true',
        aria.getState(element, State.DISABLED));

    controlRenderer.updateAriaState(element, Component.State.DISABLED, false);
    assertEquals(
        'Control must no longer have disabled ARIA state', 'false',
        aria.getState(element, State.DISABLED));
  },

  testSetAriaStatesRender_ariaStateDisabled() {
    control.setEnabled(false);
    const renderer = new ControlRenderer();
    control.setRenderer(renderer);
    control.render(sandbox);
    const element = control.getElement();
    assertNotNull(element);
    assertFalse('Control must be disabled', control.isEnabled());
    assertEquals(
        'Control must have disabled ARIA state', 'true',
        aria.getState(element, State.DISABLED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStatesDecorate_ariaStateDisabled() {
    sandbox.innerHTML = '<div id="foo" class="app goog-base-disabled"></div>';
    const element = dom.getElement('foo');

    control.setRenderer(testRenderer);
    control.decorate(element);
    assertNotNull(element);
    assertFalse('Control must be disabled', control.isEnabled());
    assertEquals(
        'Control must have disabled ARIA state', 'true',
        aria.getState(element, State.DISABLED));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUpdateAriaStateSelected() {
    control.createDom();
    const element = control.getElement();
    assertNotNull(element);
    controlRenderer.updateAriaState(element, Component.State.SELECTED, true);
    assertEquals(
        'Control must have selected ARIA state', 'true',
        aria.getState(element, State.SELECTED));

    controlRenderer.updateAriaState(element, Component.State.SELECTED, false);
    assertEquals(
        'Control must no longer have selected ARIA state', 'false',
        aria.getState(element, State.SELECTED));
  },

  testSetAriaStatesRender_ariaStateSelected() {
    control.setSupportedState(Component.State.SELECTED, true);
    control.setSelected(true);

    const renderer = new ControlRenderer();
    control.setRenderer(renderer);
    control.render(sandbox);
    const element = control.getElement();
    assertNotNull(element);
    assertTrue('Control must be selected', control.isSelected());
    assertEquals(
        'Control must have selected ARIA state', 'true',
        aria.getState(element, State.SELECTED));
  },

  testSetAriaStatesRender_ariaStateNotSelected() {
    control.setSupportedState(Component.State.SELECTED, true);

    const renderer = new ControlRenderer();
    control.setRenderer(renderer);
    control.render(sandbox);
    const element = control.getElement();
    assertNotNull(element);
    assertFalse('Control must not be selected', control.isSelected());
    assertEquals(
        'Control must have not-selected ARIA state', 'false',
        aria.getState(element, State.SELECTED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStatesDecorate_ariaStateSelected() {
    control.setSupportedState(Component.State.SELECTED, true);

    sandbox.innerHTML =
        '<div id="foo" class="app goog-control-selected"></div>';
    const element = dom.getElement('foo');

    control.setRenderer(controlRenderer);
    control.decorate(element);
    assertNotNull(element);
    assertTrue('Control must be selected', control.isSelected());
    assertEquals(
        'Control must have selected ARIA state', 'true',
        aria.getState(element, State.SELECTED));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUpdateAriaStateChecked() {
    control.createDom();
    const element = control.getElement();
    assertNotNull(element);
    controlRenderer.updateAriaState(element, Component.State.CHECKED, true);
    assertEquals(
        'Control must have checked ARIA state', 'true',
        aria.getState(element, State.CHECKED));

    controlRenderer.updateAriaState(element, Component.State.CHECKED, false);
    assertEquals(
        'Control must no longer have checked ARIA state', 'false',
        aria.getState(element, State.CHECKED));
  },

  testSetAriaStatesRender_ariaStateChecked() {
    control.setSupportedState(Component.State.CHECKED, true);
    control.setChecked(true);

    const renderer = new ControlRenderer();
    control.setRenderer(renderer);
    control.render(sandbox);
    const element = control.getElement();
    assertNotNull(element);
    assertTrue('Control must be checked', control.isChecked());
    assertEquals(
        'Control must have checked ARIA state', 'true',
        aria.getState(element, State.CHECKED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStatesDecorate_ariaStateChecked() {
    sandbox.innerHTML = '<div id="foo" class="app goog-control-checked"></div>';
    const element = dom.getElement('foo');

    control.setSupportedState(Component.State.CHECKED, true);
    control.decorate(element);
    assertNotNull(element);
    assertTrue('Control must be checked', control.isChecked());
    assertEquals(
        'Control must have checked ARIA state', 'true',
        aria.getState(element, State.CHECKED));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUpdateAriaStateOpened() {
    control.createDom();
    const element = control.getElement();
    assertNotNull(element);
    controlRenderer.updateAriaState(element, Component.State.OPENED, true);
    assertEquals(
        'Control must have expanded ARIA state', 'true',
        aria.getState(element, State.EXPANDED));

    controlRenderer.updateAriaState(element, Component.State.OPENED, false);
    assertEquals(
        'Control must no longer have expanded ARIA state', 'false',
        aria.getState(element, State.EXPANDED));
  },

  testSetAriaStatesRender_ariaStateOpened() {
    control.setSupportedState(Component.State.OPENED, true);
    control.setOpen(true);

    const renderer = new ControlRenderer();
    control.setRenderer(renderer);
    control.render(sandbox);
    const element = control.getElement();
    assertNotNull(element);
    assertTrue('Control must be opened', control.isOpen());
    assertEquals(
        'Control must have expanded ARIA state', 'true',
        aria.getState(element, State.EXPANDED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStatesDecorate_ariaStateOpened() {
    sandbox.innerHTML = '<div id="foo" class="app goog-base-open"></div>';
    const element = dom.getElement('foo');

    control.setSupportedState(Component.State.OPENED, true);
    control.setRenderer(testRenderer);
    control.decorate(element);
    assertNotNull(element);
    assertTrue('Control must be opened', control.isOpen());
    assertEquals(
        'Control must have expanded ARIA state', 'true',
        aria.getState(element, State.EXPANDED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStateRoleNotInMap() {
    sandbox.innerHTML = '<div id="foo" role="option">Hello, world!</div>';
    control.setRenderer(controlRenderer);
    control.setSupportedState(Component.State.CHECKED, true);
    const element = dom.getElement('foo');
    control.decorate(element);
    assertEquals(
        'Element should have ARIA role option.', Role.OPTION,
        aria.getRole(element));
    control.setStateInternal(Component.State.DISABLED, true);
    controlRenderer.setAriaStates(control, element);
    assertEquals(
        'Element should have aria-disabled true', 'true',
        aria.getState(element, State.DISABLED));
    control.setStateInternal(Component.State.CHECKED, true);
    controlRenderer.setAriaStates(control, element);
    assertEquals(
        'Element should have aria-checked true', 'true',
        aria.getState(element, State.CHECKED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStateRoleInMapMatches() {
    sandbox.innerHTML = '<div id="foo" role="checkbox">Hello, world!</div>';
    control.setRenderer(controlRenderer);
    control.setSupportedState(Component.State.CHECKED, true);
    const element = dom.getElement('foo');
    control.decorate(element);
    assertEquals(
        'Element should have ARIA role checkbox.', Role.CHECKBOX,
        aria.getRole(element));
    control.setStateInternal(Component.State.DISABLED, true);
    controlRenderer.setAriaStates(control, element);
    assertEquals(
        'Element should have aria-disabled true', 'true',
        aria.getState(element, State.DISABLED));
    control.setStateInternal(Component.State.CHECKED, true);
    controlRenderer.setAriaStates(control, element);
    assertEquals(
        'Element should have aria-checked true', 'true',
        aria.getState(element, State.CHECKED));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetAriaStateRoleInMapNotMatches() {
    sandbox.innerHTML = '<div id="foo" role="button">Hello, world!</div>';
    control.setRenderer(controlRenderer);
    control.setSupportedState(Component.State.CHECKED, true);
    const element = dom.getElement('foo');
    control.decorate(element);
    assertEquals(
        'Element should have ARIA role button.', Role.BUTTON,
        aria.getRole(element));
    control.setStateInternal(Component.State.DISABLED, true);
    controlRenderer.setAriaStates(control, element);
    assertEquals(
        'Element should have aria-disabled true', 'true',
        aria.getState(element, State.DISABLED));
    control.setStateInternal(Component.State.CHECKED, true);
    controlRenderer.setAriaStates(control, element);
    assertEquals(
        'Element should have aria-pressed true', 'true',
        aria.getState(element, State.PRESSED));
    assertEquals(
        'Element should not have aria-checked', '',
        aria.getState(element, State.CHECKED));
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testToggleAriaStateMap() {
    const map = googObject.create(
        Role.BUTTON, State.PRESSED, Role.CHECKBOX, State.CHECKED,
        Role.MENU_ITEM, State.SELECTED, Role.MENU_ITEM_CHECKBOX, State.CHECKED,
        Role.MENU_ITEM_RADIO, State.CHECKED, Role.RADIO, State.CHECKED,
        Role.TAB, State.SELECTED, Role.TREEITEM, State.SELECTED);
    for (const key in map) {
      assertTrue(
          'Toggle ARIA state map incorrect.',
          key in ControlRenderer.TOGGLE_ARIA_STATE_MAP_);
      assertEquals(
          'Toggle ARIA state map incorrect.', map[key],
          ControlRenderer.TOGGLE_ARIA_STATE_MAP_[key]);
    }
  },

  testSetContent() {
    dom.setTextContent(sandbox, 'Hello, world!');
    controlRenderer.setContent(sandbox, 'Not so fast!');
    assertEquals(
        'Element must contain expected text value', 'Not so fast!',
        dom.getTextContent(sandbox));
  },

  testSetContentNull() {
    dom.setTextContent(sandbox, 'Hello, world!');
    controlRenderer.setContent(sandbox, null);
    assertEquals(
        'Element must have no child nodes', 0, sandbox.childNodes.length);
    assertEquals(
        'Element must contain expected text value', '',
        dom.getTextContent(sandbox));
  },

  testSetContentEmpty() {
    dom.setTextContent(sandbox, 'Hello, world!');
    controlRenderer.setContent(sandbox, '');
    assertEquals(
        'Element must not have children', 0, sandbox.childNodes.length);
    assertEquals(
        'Element must contain expected text value', '',
        dom.getTextContent(sandbox));
  },

  testSetContentWhitespace() {
    dom.setTextContent(sandbox, 'Hello, world!');
    controlRenderer.setContent(sandbox, ' ');
    assertEquals('Element must have one child', 1, sandbox.childNodes.length);
    assertEquals(
        'Child must be a text node', NodeType.TEXT,
        sandbox.firstChild.nodeType);
    assertEquals(
        'Element must contain expected text value', ' ',
        dom.getTextContent(sandbox));
  },

  testSetContentTextNode() {
    dom.setTextContent(sandbox, 'Hello, world!');
    controlRenderer.setContent(sandbox, document.createTextNode('Text'));
    assertEquals('Element must have one child', 1, sandbox.childNodes.length);
    assertEquals(
        'Child must be a text node', NodeType.TEXT,
        sandbox.firstChild.nodeType);
    assertEquals(
        'Element must contain expected text value', 'Text',
        dom.getTextContent(sandbox));
  },

  testSetContentElementNode() {
    dom.setTextContent(sandbox, 'Hello, world!');
    controlRenderer.setContent(
        sandbox, dom.createDom(TagName.DIV, {id: 'foo'}, 'Foo'));
    assertEquals('Element must have one child', 1, sandbox.childNodes.length);
    assertEquals(
        'Child must be an element node', NodeType.ELEMENT,
        sandbox.firstChild.nodeType);
    assertHTMLEquals(
        'Element must contain expected HTML', '<div id="foo">Foo</div>',
        sandbox.innerHTML);
  },

  testSetContentArray() {
    dom.setTextContent(sandbox, 'Hello, world!');
    controlRenderer.setContent(
        sandbox, ['Hello, ', dom.createDom(TagName.B, null, 'world'), '!']);
    assertEquals(
        'Element must have three children', 3, sandbox.childNodes.length);
    assertEquals(
        '1st child must be a text node', NodeType.TEXT,
        sandbox.childNodes[0].nodeType);
    assertEquals(
        '2nd child must be an element', NodeType.ELEMENT,
        sandbox.childNodes[1].nodeType);
    assertEquals(
        '3rd child must be a text node', NodeType.TEXT,
        sandbox.childNodes[2].nodeType);
    assertHTMLEquals(
        'Element must contain expected HTML', 'Hello, <b>world</b>!',
        sandbox.innerHTML);
  },

  testSetContentNodeList() {
    dom.setTextContent(sandbox, 'Hello, world!');
    const div = dom.createDom(
        TagName.DIV, null, 'Hello, ', dom.createDom(TagName.B, null, 'world'),
        '!');
    controlRenderer.setContent(sandbox, div.childNodes);
    assertEquals(
        'Element must have three children', 3, sandbox.childNodes.length);
    assertEquals(
        '1st child must be a text node', NodeType.TEXT,
        sandbox.childNodes[0].nodeType);
    assertEquals(
        '2nd child must be an element', NodeType.ELEMENT,
        sandbox.childNodes[1].nodeType);
    assertEquals(
        '3rd child must be a text node', NodeType.TEXT,
        sandbox.childNodes[2].nodeType);
    assertHTMLEquals(
        'Element must contain expected HTML', 'Hello, <b>world</b>!',
        sandbox.innerHTML);
  },

  testGetKeyEventTarget() {
    assertNull(
        'Key event target for unrendered control must be null',
        controlRenderer.getKeyEventTarget(control));
    control.createDom();
    assertEquals(
        'Key event target for rendered control must be its element',
        control.getElement(), controlRenderer.getKeyEventTarget(control));
  },

  testGetCssClass() {
    assertEquals(
        'ControlRenderer\'s CSS class must have expected value',
        ControlRenderer.CSS_CLASS, controlRenderer.getCssClass());
    assertEquals(
        'TestRenderer\'s CSS class must have expected value',
        TestRenderer.CSS_CLASS, testRenderer.getCssClass());
  },

  testGetStructuralCssClass() {
    assertEquals(
        'ControlRenderer\'s structural class must be its CSS class',
        controlRenderer.getCssClass(), controlRenderer.getStructuralCssClass());
    assertEquals(
        'TestRenderer\'s structural class must have expected value',
        'goog-base', testRenderer.getStructuralCssClass());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetClassNames() {
    // These tests use assertArrayEquals, because the order is significant.
    assertArrayEquals(
        'ControlRenderer must return expected class names ' +
            'in the expected order',
        ['goog-control'], controlRenderer.getClassNames(control));
    assertArrayEquals(
        'TestRenderer must return expected class names ' +
            'in the expected order',
        ['goog-button', 'goog-base'], testRenderer.getClassNames(control));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetClassNamesForControlWithState() {
    control.setStateInternal(Component.State.HOVER | Component.State.ACTIVE);

    // These tests use assertArrayEquals, because the order is significant.
    assertArrayEquals(
        'ControlRenderer must return expected class names ' +
            'in the expected order',
        ['goog-control', 'goog-control-hover', 'goog-control-active'],
        controlRenderer.getClassNames(control));
    assertArrayEquals(
        'TestRenderer must return expected class names ' +
            'in the expected order',
        ['goog-button', 'goog-base', 'goog-base-hover', 'goog-base-active'],
        testRenderer.getClassNames(control));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetClassNamesForControlWithExtraClassNames() {
    control.addClassName('foo');
    control.addClassName('bar');

    // These tests use assertArrayEquals, because the order is significant.
    assertArrayEquals(
        'ControlRenderer must return expected class names ' +
            'in the expected order',
        ['goog-control', 'foo', 'bar'], controlRenderer.getClassNames(control));
    assertArrayEquals(
        'TestRenderer must return expected class names ' +
            'in the expected order',
        ['goog-button', 'goog-base', 'foo', 'bar'],
        testRenderer.getClassNames(control));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetClassNamesForControlWithStateAndExtraClassNames() {
    control.setStateInternal(Component.State.HOVER | Component.State.ACTIVE);
    control.addClassName('foo');
    control.addClassName('bar');

    // These tests use assertArrayEquals, because the order is significant.
    assertArrayEquals(
        'ControlRenderer must return expected class names ' +
            'in the expected order',
        [
          'goog-control',
          'goog-control-hover',
          'goog-control-active',
          'foo',
          'bar',
        ],
        controlRenderer.getClassNames(control));
    assertArrayEquals(
        'TestRenderer must return expected class names ' +
            'in the expected order',
        [
          'goog-button',
          'goog-base',
          'goog-base-hover',
          'goog-base-active',
          'foo',
          'bar',
        ],
        testRenderer.getClassNames(control));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetClassNamesForState() {
    // These tests use assertArrayEquals, because the order is significant.
    assertArrayEquals(
        'ControlRenderer must return expected class names ' +
            'in the expected order',
        ['goog-control-hover', 'goog-control-checked'],
        controlRenderer.getClassNamesForState(
            Component.State.HOVER | Component.State.CHECKED));
    assertArrayEquals(
        'TestRenderer must return expected class names ' +
            'in the expected order',
        ['goog-base-hover', 'goog-base-checked'],
        testRenderer.getClassNamesForState(
            Component.State.HOVER | Component.State.CHECKED));
  },

  /**
     @suppress {missingProperties,visibility,checkTypes} suppression added to
     enable type checking
   */
  testGetClassForState() {
    const renderer = new ControlRenderer();
    assertUndefined(
        'State-to-class map must not exist until first use',
        renderer.classByState_);
    assertEquals(
        'Renderer must return expected class name for SELECTED',
        'goog-control-selected',
        renderer.getClassForState(Component.State.SELECTED));
    assertUndefined(
        'Renderer must return undefined for invalid state',
        renderer.getClassForState('foo'));
  },

  /**
     @suppress {missingProperties,visibility} suppression added to enable type
     checking
   */
  testGetStateFromClass() {
    const renderer = new ControlRenderer();
    assertUndefined(
        'Class-to-state map must not exist until first use',
        renderer.stateByClass_);
    assertEquals(
        'Renderer must return expected state', Component.State.SELECTED,
        renderer.getStateFromClass('goog-control-selected'));
    assertEquals(
        'Renderer must return 0x00 for unknown class', 0x00,
        renderer.getStateFromClass('goog-control-annoyed'));
  },

  testIe6ClassCombinationsCreateDom() {
    control.setRenderer(testRenderer);

    control.enableClassName('combined', true);

    control.createDom();
    const element = control.getElement();

    testRenderer.setState(control, Component.State.DISABLED, true);
    let expectedClasses =
        ['combined', 'goog-base', 'goog-base-disabled', 'goog-button'];
    assertSameElements(
        'Non IE6 browsers should not have a combined class', expectedClasses,
        classlist.get(element));

    testRenderer.setState(control, Component.State.DISABLED, false);
    testRenderer.setState(control, Component.State.HOVER, true);
    expectedClasses =
        ['combined', 'goog-base', 'goog-base-hover', 'goog-button'];
    if (isIe6()) {
      assertSameElements(
          'IE6 and lower should have one combined class',
          expectedClasses.concat(['combined_goog-base-hover_goog-button']),
          classlist.get(element));
    } else {
      assertSameElements(
          'Non IE6 browsers should not have a combined class', expectedClasses,
          classlist.get(element));
    }

    testRenderer.setRightToLeft(element, true);
    testRenderer.enableExtraClassName(control, 'combined2', true);
    expectedClasses = [
      'combined',
      'combined2',
      'goog-base',
      'goog-base-hover',
      'goog-base-rtl',
      'goog-button',
    ];
    if (isIe6()) {
      assertSameElements(
          'IE6 and lower should have two combined class',
          expectedClasses.concat([
            'combined_goog-base-hover_goog-button',
            'combined_combined2_goog-base-hover_goog-base-rtl_goog-button',
          ]),
          classlist.get(element));
    } else {
      assertSameElements(
          'Non IE6 browsers should not have a combined class', expectedClasses,
          classlist.get(element));
    }
  },

  testIe6ClassCombinationsDecorate() {
    sandbox.innerHTML = '<div id="foo" class="combined goog-base-hover"></div>';
    const foo = dom.getElement('foo');

    const element = testRenderer.decorate(control, foo);

    const expectedClasses =
        ['combined', 'goog-base', 'goog-base-hover', 'goog-button'];
    if (isIe6()) {
      assertSameElements(
          'IE6 and lower should have one combined class',
          expectedClasses.concat(['combined_goog-base-hover_goog-button']),
          classlist.get(element));
    } else {
      assertSameElements(
          'Non IE6 browsers should not have a combined class', expectedClasses,
          classlist.get(element));
    }
  },
});
