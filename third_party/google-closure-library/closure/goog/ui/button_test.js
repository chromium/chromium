/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ButtonTest');
goog.setTestOnly();

const Button = goog.require('goog.ui.Button');
const ButtonRenderer = goog.require('goog.ui.ButtonRenderer');
const ButtonSide = goog.require('goog.ui.ButtonSide');
const Component = goog.require('goog.ui.Component');
const EventType = goog.require('goog.events.EventType');
const GoogEvent = goog.require('goog.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const KeyHandler = goog.require('goog.events.KeyHandler');
const NativeButtonRenderer = goog.require('goog.ui.NativeButtonRenderer');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let sandbox;
let button;
let clonedButtonDom;
let demoButtonElement;

testSuite({
  setUp() {
    sandbox = dom.getElement('sandbox');
    button = new Button();
    demoButtonElement = dom.getElement('demoButton');
    clonedButtonDom = demoButtonElement.cloneNode(true);
  },

  tearDown() {
    button.dispose();
    demoButtonElement.parentNode.replaceChild(
        clonedButtonDom, demoButtonElement);
    dom.removeChildren(sandbox);
  },

  testConstructor() {
    assertNotNull('Button must not be null', button);
    assertEquals(
        'Renderer must default to expected value',
        NativeButtonRenderer.getInstance(), button.getRenderer());

    const fakeDomHelper = {};
    /** @suppress {checkTypes} suppression added to enable type checking */
    const testButton =
        new Button('Hello', ButtonRenderer.getInstance(), fakeDomHelper);
    assertEquals(
        'Content must have expected value', 'Hello', testButton.getContent());
    assertEquals(
        'Renderer must have expected value', ButtonRenderer.getInstance(),
        testButton.getRenderer());
    assertEquals(
        'DOM helper must have expected value', fakeDomHelper,
        testButton.getDomHelper());
    testButton.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetSetValue() {
    assertUndefined(
        'Button\'s value must default to undefined', button.getValue());
    button.setValue(17);
    assertEquals('Button must have expected value', 17, button.getValue());
    button.render(sandbox);
    assertEquals(
        'Button element must have expected value', '17',
        button.getElement().value);
    button.setValue('foo');
    assertEquals(
        'Button element must have updated value', 'foo',
        button.getElement().value);
    button.setValueInternal('bar');
    assertEquals(
        'Button must have new internal value', 'bar', button.getValue());
    assertEquals(
        'Button element must be unchanged', 'foo', button.getElement().value);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetSetTooltip() {
    assertUndefined(
        'Button\'s tooltip must default to undefined', button.getTooltip());
    button.setTooltip('Hello');
    assertEquals(
        'Button must have expected tooltip', 'Hello', button.getTooltip());
    button.render(sandbox);
    assertEquals(
        'Button element must have expected title', 'Hello',
        button.getElement().title);
    button.setTooltip('Goodbye');
    assertEquals(
        'Button element must have updated title', 'Goodbye',
        button.getElement().title);
    button.setTooltipInternal('World');
    assertEquals(
        'Button must have new internal tooltip', 'World', button.getTooltip());
    assertEquals(
        'Button element must be unchanged', 'Goodbye',
        button.getElement().title);
  },

  testSetCollapsed() {
    assertNull(
        'Button must not have any collapsed styling by default',
        button.getExtraClassNames());
    button.setCollapsed(ButtonSide.START);
    assertSameElements(
        'Button must have the start side collapsed',
        ['goog-button-collapse-left'], button.getExtraClassNames());
    button.render(sandbox);
    assertSameElements(
        'Button element must have the start side collapsed',
        ['goog-button', 'goog-button-collapse-left'],
        classlist.get(button.getElement()));
    button.setCollapsed(ButtonSide.BOTH);
    assertSameElements(
        'Button must have both sides collapsed',
        ['goog-button-collapse-left', 'goog-button-collapse-right'],
        button.getExtraClassNames());
    assertSameElements(
        'Button element must have both sides collapsed',
        [
          'goog-button',
          'goog-button-collapse-left',
          'goog-button-collapse-right',
        ],
        classlist.get(button.getElement()));
  },

  testDispose() {
    assertFalse('Button must not have been disposed of', button.isDisposed());
    button.render(sandbox);
    button.setValue('foo');
    button.setTooltip('bar');
    button.dispose();
    assertTrue('Button must have been disposed of', button.isDisposed());
    assertUndefined(
        'Button\'s value must have been deleted', button.getValue());
    assertUndefined(
        'Button\'s tooltip must have been deleted', button.getTooltip());
  },

  testBasicButtonBehavior() {
    let dispatchedActionCount = 0;
    const handleAction = () => {
      dispatchedActionCount++;
    };
    events.listen(button, Component.EventType.ACTION, handleAction);

    button.decorate(demoButtonElement);
    testingEvents.fireClickSequence(demoButtonElement);
    assertEquals(
        'Button must have dispatched ACTION on click', 1,
        dispatchedActionCount);

    dispatchedActionCount = 0;
    let e = new GoogEvent(KeyHandler.EventType.KEY, button);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.ENTER;
    button.handleKeyEvent(e);
    assertEquals(
        'Enabled button must have dispatched ACTION on Enter key', 1,
        dispatchedActionCount);

    dispatchedActionCount = 0;
    e = new GoogEvent(EventType.KEYUP, button);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.SPACE;
    button.handleKeyEvent(e);
    assertEquals(
        'Enabled button must have dispatched ACTION on Space key', 1,
        dispatchedActionCount);

    events.unlisten(button, Component.EventType.ACTION, handleAction);
  },

  testDisabledButtonBehavior() {
    let dispatchedActionCount = 0;
    const handleAction = () => {
      dispatchedActionCount++;
    };
    events.listen(button, Component.EventType.ACTION, handleAction);

    button.setEnabled(false);

    dispatchedActionCount = 0;
    button.handleKeyEvent({keyCode: KeyCodes.ENTER});
    assertEquals(
        'Disabled button must not dispatch ACTION on Enter key', 0,
        dispatchedActionCount);

    dispatchedActionCount = 0;
    button.handleKeyEvent({keyCode: KeyCodes.SPACE, type: EventType.KEYUP});
    assertEquals(
        'Disabled button must not have dispatched ACTION on Space', 0,
        dispatchedActionCount);

    events.unlisten(button, Component.EventType.ACTION, handleAction);
  },

  testSpaceFireActionOnKeyUp() {
    let dispatchedActionCount = 0;
    const handleAction = () => {
      dispatchedActionCount++;
    };
    events.listen(button, Component.EventType.ACTION, handleAction);

    dispatchedActionCount = 0;
    let e = new GoogEvent(KeyHandler.EventType.KEY, button);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.SPACE;
    button.handleKeyEvent(e);
    assertEquals(
        'Button must not have dispatched ACTION on Space keypress', 0,
        dispatchedActionCount);
    assertTrue(
        'The default action (scrolling) must have been prevented ' +
            'for Space keypress',
        e.defaultPrevented);

    dispatchedActionCount = 0;
    e = new GoogEvent(EventType.KEYUP, button);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.SPACE;
    button.handleKeyEvent(e);
    assertEquals(
        'Button must have dispatched ACTION on Space keyup', 1,
        dispatchedActionCount);

    events.unlisten(button, Component.EventType.ACTION, handleAction);
  },

  testEnterFireActionOnKeyPress() {
    let dispatchedActionCount = 0;
    const handleAction = () => {
      dispatchedActionCount++;
    };
    events.listen(button, Component.EventType.ACTION, handleAction);

    dispatchedActionCount = 0;
    let e = new GoogEvent(KeyHandler.EventType.KEY, button);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.ENTER;
    button.handleKeyEvent(e);
    assertEquals(
        'Button must have dispatched ACTION on Enter keypress', 1,
        dispatchedActionCount);

    dispatchedActionCount = 0;
    e = new GoogEvent(EventType.KEYUP, button);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.ENTER;
    button.handleKeyEvent(e);
    assertEquals(
        'Button must not have dispatched ACTION on Enter keyup', 0,
        dispatchedActionCount);

    events.unlisten(button, Component.EventType.ACTION, handleAction);
  },

  testSetAriaLabel() {
    assertNull(
        'Button must not have aria label by default', button.getAriaLabel());
    button.setAriaLabel('Button 1');
    button.render();
    assertEquals(
        'Button element must have expected aria-label', 'Button 1',
        button.getElement().getAttribute('aria-label'));
    button.setAriaLabel('Button 2');
    assertEquals(
        'Button element must have updated aria-label', 'Button 2',
        button.getElement().getAttribute('aria-label'));
  },

  testSetAriaLabel_decorate() {
    assertNull(
        'Button must not have aria label by default', button.getAriaLabel());
    button.setAriaLabel('Button 1');
    button.decorate(demoButtonElement);
    const el = button.getElementStrict();
    assertEquals(
        'Button element must have expected aria-label', 'Button 1',
        el.getAttribute('aria-label'));
    assertEquals(
        'Button element must have expected aria-role', 'button',
        el.getAttribute('role'));
    button.setAriaLabel('Button 2');
    assertEquals(
        'Button element must have updated aria-label', 'Button 2',
        el.getAttribute('aria-label'));
    assertEquals(
        'Button element must have expected aria-role', 'button',
        el.getAttribute('role'));
  },
});
