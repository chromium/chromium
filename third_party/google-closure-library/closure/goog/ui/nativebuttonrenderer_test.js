/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.NativeButtonRendererTest');
goog.setTestOnly();

const Button = goog.require('goog.ui.Button');
const Component = goog.require('goog.ui.Component');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const NativeButtonRenderer = goog.require('goog.ui.NativeButtonRenderer');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const rendererasserts = goog.require('goog.testing.ui.rendererasserts');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

let sandbox;
let renderer;
let expectedFailures;
let button;

testSuite({
  setUpPage() {
    sandbox = dom.getElement('sandbox');
    renderer = NativeButtonRenderer.getInstance();
    expectedFailures = new ExpectedFailures();
  },

  setUp() {
    button = new Button('Hello', renderer);
  },

  tearDown() {
    button.dispose();
    dom.removeChildren(sandbox);
    expectedFailures.handleTearDown();
  },

  testConstructor() {
    assertNotNull('Renderer must not be null', renderer);
  },

  testGetAriaRole() {
    assertUndefined('ARIA role must be undefined', renderer.getAriaRole());
  },

  testCreateDom() {
    button.setTooltip('Hello, world!');
    button.setValue('foo');
    const element = renderer.createDom(button);
    assertNotNull('Element must not be null', element);
    assertEquals(
        'Element must be a button', String(TagName.BUTTON), element.tagName);
    assertSameElements(
        'Button element must have expected class name', ['goog-button'],
        classlist.get(element));
    assertFalse('Button element must be enabled', element.disabled);
    assertEquals(
        'Button element must have expected title', 'Hello, world!',
        element.title);
    // Expected to fail on IE.
    expectedFailures.expectFailureFor(userAgent.IE);
    try {
      // See http://www.fourmilab.ch/fourmilog/archives/2007-03/000824.html
      // for a description of the problem.
      assertEquals(
          'Button element must have expected value', 'foo', element.value);
      assertEquals(
          'Button element must have expected contents', 'Hello',
          element.innerHTML);
    } catch (e) {
      expectedFailures.handleException(e);
    }
    assertFalse(
        'Button must not handle its own mouse events',
        button.isHandleMouseEvents());
    assertFalse(
        'Button must not support the custom FOCUSED state',
        button.isSupportedState(Component.State.FOCUSED));
  },

  testCanDecorate() {
    sandbox.innerHTML = '<button id="buttonElement">Button</button>\n' +
        '<input id="inputButton" type="button" value="Input Button">\n' +
        '<input id="inputSubmit" type="submit" value="Input Submit">\n' +
        '<input id="inputReset" type="reset" value="Input Reset">\n' +
        '<input id="inputText" type="text" size="10">\n' +
        '<div id="divButton" class="goog-button">Hello</div>';

    assertTrue(
        'Must be able to decorate <button>',
        renderer.canDecorate(dom.getElement('buttonElement')));
    assertTrue(
        'Must be able to decorate <input type="button">',
        renderer.canDecorate(dom.getElement('inputButton')));
    assertTrue(
        'Must be able to decorate <input type="submit">',
        renderer.canDecorate(dom.getElement('inputSubmit')));
    assertTrue(
        'Must be able to decorate <input type="reset">',
        renderer.canDecorate(dom.getElement('inputReset')));
    assertFalse(
        'Must not be able to decorate <input type="text">',
        renderer.canDecorate(dom.getElement('inputText')));
    assertFalse(
        'Must not be able to decorate <div class="goog-button">',
        renderer.canDecorate(dom.getElement('divButton')));
  },

  testDecorate() {
    sandbox.innerHTML =
        '<button id="foo" title="Hello!" value="bar">Foo Button</button>\n' +
        '<button id="disabledButton" value="bah" disabled="disabled">Disabled' +
        '</button>';

    let element = renderer.decorate(button, dom.getElement('foo'));
    assertEquals(
        'Decorated element must be as expected', dom.getElement('foo'),
        element);
    assertEquals(
        'Decorated button title must have expected value', 'Hello!',
        button.getTooltip());
    // Expected to fail on IE.
    expectedFailures.expectFailureFor(userAgent.IE);
    try {
      // See http://www.fourmilab.ch/fourmilog/archives/2007-03/000824.html
      // for a description of the problem.
      assertEquals(
          'Decorated button value must have expected value', 'bar',
          button.getValue());
    } catch (e) {
      expectedFailures.handleException(e);
    }
    assertFalse(
        'Button must not handle its own mouse events',
        button.isHandleMouseEvents());
    assertFalse(
        'Button must not support the custom FOCUSED state',
        button.isSupportedState(Component.State.FOCUSED));

    element = renderer.decorate(button, dom.getElement('disabledButton'));
    assertFalse('Decorated button must be disabled', button.isEnabled());
    assertSameElements(
        'Decorated button must have expected class names',
        ['goog-button', 'goog-button-disabled'], classlist.get(element));
    // Expected to fail on IE.
    expectedFailures.expectFailureFor(userAgent.IE);
    try {
      // See http://www.fourmilab.ch/fourmilog/archives/2007-03/000824.html
      // for a description of the problem.
      assertEquals(
          'Decorated button value must have expected value', 'bah',
          button.getValue());
    } catch (e) {
      expectedFailures.handleException(e);
    }
    assertFalse(
        'Button must not handle its own mouse events',
        button.isHandleMouseEvents());
    assertFalse(
        'Button must not support the custom FOCUSED state',
        button.isSupportedState(Component.State.FOCUSED));
  },

  testInitializeDom() {
    let dispatchedActionCount = 0;
    const handleAction = () => {
      dispatchedActionCount++;
    };
    events.listen(button, Component.EventType.ACTION, handleAction);

    button.render(sandbox);
    testingEvents.fireClickSequence(button.getElement());
    assertEquals(
        'Button must have dispatched ACTION on click', 1,
        dispatchedActionCount);

    events.unlisten(button, Component.EventType.ACTION, handleAction);
  },

  testIsFocusable() {
    assertTrue(
        'Enabled button must be focusable', renderer.isFocusable(button));
    button.setEnabled(false);
    assertFalse(
        'Disabled button must not be focusable', renderer.isFocusable(button));
  },

  testSetState() {
    button.render(sandbox);
    assertFalse(
        'Button element must not be disabled', button.getElement().disabled);
    renderer.setState(button, Component.State.DISABLED, true);
    assertTrue('Button element must be disabled', button.getElement().disabled);
  },

  testGetValue() {
    sandbox.innerHTML = '<button id="foo" value="blah">Hello</button>';
    // Expected to fail on IE.
    expectedFailures.expectFailureFor(userAgent.IE);
    try {
      // See http://www.fourmilab.ch/fourmilog/archives/2007-03/000824.html
      // for a description of the problem.
      assertEquals(
          'Value must be as expected', 'blah',
          renderer.getValue(dom.getElement('foo')));
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testSetValue() {
    button.render(sandbox);
    renderer.setValue(button.getElement(), 'What?');
    assertEquals(
        'Button must have expected value', 'What?',
        renderer.getValue(button.getElement()));
  },

  testDoesntCallGetCssClassInConstructor() {
    rendererasserts.assertNoGetCssClassCallsInConstructor(NativeButtonRenderer);
  },
});
