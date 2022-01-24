/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ControlTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const Component = goog.require('goog.ui.Component');
const Control = goog.require('goog.ui.Control');
const ControlRenderer = goog.require('goog.ui.ControlRenderer');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const PointerFallbackEventType = goog.require('goog.events.PointerFallbackEventType');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const googArray = goog.require('goog.array');
const googEvents = goog.require('goog.events');
const googObject = goog.require('goog.object');
const googString = goog.require('goog.string');
const registry = goog.require('goog.ui.registry');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

// Disabled due to problems on farm.
const testFocus = false;

let control;

const ALL_EVENTS = googObject.getValues(Component.EventType);
const events = {};
let expectedFailures;
let sandbox;

/** A dummy renderer, for testing purposes. */
class TestRenderer extends ControlRenderer {
  constructor() {
    super();
    ControlRenderer.call(this);
  }
}

/** Resets the global counter for events dispatched by test objects. */
function resetEventCount() {
  googObject.clear(events);
}

/**
 * Increments the global counter for events of this type.
 * @param {googEvents.Event} e Event to count.
 */
function countEvent(e) {
  const type = e.type;
  const target = e.target;

  if (!events[target]) {
    events[target] = {};
  }

  if (events[target][type]) {
    events[target][type]++;
  } else {
    events[target][type] = 1;
  }
}

/**
 * Returns the number of times test objects dispatched events of the given
 * type since the global counter was last reset.
 * @param {Control} target Event target.
 * @param {string} type Event type.
 * @return {number} Number of events of this type.
 */
function getEventCount(target, type) {
  return events[target] && events[target][type] || 0;
}

/**
 * Returns true if no events were dispatched since the last reset.
 * @return {boolean} Whether no events have been dispatched since the last
 *     reset.
 */
function noEventsDispatched() {
  return !events || googObject.isEmpty(events);
}

/**
 * Returns the number of event listeners created by the control.
 * @param {Control} control Control whose event listers are to be counted.
 * @return {number} Number of event listeners.
 * @suppress {visibility} suppression added to enable type checking
 */
function getListenerCount(control) {
  return control.googUiComponentHandler_ ?
      googObject.getCount(control.getHandler().keys_) :
      0;
}

/**
 * Simulates a mousedown event on the given element, including focusing it.
 * @param {Element} element Element on which to simulate mousedown.
 * @param {BrowserEvent.MouseButton=} button Mouse button; defaults to
 *     `BrowserEvent.MouseButton.LEFT`.
 * @return {boolean} Whether the event was allowed to proceed.
 */
function fireMouseDownAndFocus(element, button = undefined) {
  /** @suppress {checkTypes} suppression added to enable type checking */
  const result = testingEvents.fireMouseDownEvent(element, button);
  if (result) {
    // Browsers move focus for all buttons, not just the left button.
    element.focus();
  }
  return result;
}

function assertClickSequenceFires(msg) {
  const actionCount = getEventCount(control, Component.EventType.ACTION);
  testingEvents.fireClickSequence(control.getKeyEventTarget());
  assertEquals(
      msg, actionCount + 1, getEventCount(control, Component.EventType.ACTION));
}

function assertIsolatedClickFires(msg) {
  const actionCount = getEventCount(control, Component.EventType.ACTION);
  testingEvents.fireClickEvent(control.getKeyEventTarget());
  assertEquals(
      msg, actionCount + 1, getEventCount(control, Component.EventType.ACTION));
}

function assertIsolatedClickDoesNotFire(msg) {
  const actionCount = getEventCount(control, Component.EventType.ACTION);
  testingEvents.fireClickEvent(control.getKeyEventTarget());
  assertEquals(
      msg, actionCount, getEventCount(control, Component.EventType.ACTION));
}

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
    sandbox = document.getElementById('sandbox');
  },

  /** Initializes the testcase prior to execution. */
  setUp() {
    control = new Control('Hello');
    control.setDispatchTransitionEvents(Component.State.ALL, true);
    googEvents.listen(control, ALL_EVENTS, countEvent);
  },

  /** Cleans up after executing the testcase. */
  tearDown() {
    control.dispose();
    expectedFailures.handleTearDown();
    dom.removeChildren(sandbox);
    resetEventCount();
  },

  /** Tests the {@link Control} constructor. */
  testConstructor() {
    assertNotNull('Constructed control must not be null', control);
    assertEquals(
        'Content must have expected value', 'Hello', control.getContent());
    assertEquals(
        'Renderer must default to the registered renderer',
        registry.getDefaultRenderer(Control), control.getRenderer());

    const content = dom.createDom(
        TagName.DIV, null, 'Hello', dom.createDom(TagName.B, null, 'World'));
    const testRenderer = new TestRenderer();
    const fakeDomHelper = {};
    /** @suppress {checkTypes} suppression added to enable type checking */
    const foo = new Control(content, testRenderer, fakeDomHelper);
    assertNotNull('Constructed object must not be null', foo);
    assertEquals('Content must have expected value', content, foo.getContent());
    assertEquals(
        'Renderer must have expected value', testRenderer, foo.getRenderer());
    assertEquals(
        'DOM helper must have expected value', fakeDomHelper,
        foo.getDomHelper());
    foo.dispose();
  },

  /**
   * Tests {@link Control#getHandler}.
   * @suppress {visibility} suppression added
   *      to enable type checking
   */
  testGetHandler() {
    assertUndefined(
        'Event handler must be undefined before getHandler() ' +
            'is called',
        control.googUiComponentHandler_);
    /** @suppress {visibility} suppression added to enable type checking */
    const handler = control.getHandler();
    assertNotNull('Event handler must not be null', handler);
    assertEquals(
        'getHandler() must return the same instance if called again', handler,
        control.getHandler());
  },

  /** Tests {@link Control#isHandleMouseEvents}. */
  testIsHandleMouseEvents() {
    assertTrue(
        'Controls must handle their own mouse events by default',
        control.isHandleMouseEvents());
  },

  /** Tests {@link Control#setHandleMouseEvents}. */
  testSetHandleMouseEvents() {
    assertTrue(
        'Control must handle its own mouse events by default',
        control.isHandleMouseEvents());
    control.setHandleMouseEvents(false);
    assertFalse(
        'Control must no longer handle its own mouse events',
        control.isHandleMouseEvents());
    control.setHandleMouseEvents(true);
    assertTrue(
        'Control must once again handle its own mouse events',
        control.isHandleMouseEvents());
    control.render(sandbox);
    assertTrue(
        'Rendered control must handle its own mouse events',
        control.isHandleMouseEvents());
    control.setHandleMouseEvents(false);
    assertFalse(
        'Rendered control must no longer handle its own mouse events',
        control.isHandleMouseEvents());
    control.setHandleMouseEvents(true);
    assertTrue(
        'Rendered control must once again handle its own mouse events',
        control.isHandleMouseEvents());
  },

  /** Tests {@link Control#getKeyEventTarget}. */
  testGetKeyEventTarget() {
    assertNull(
        'Key event target of control without DOM must be null',
        control.getKeyEventTarget());
    control.createDom();
    assertEquals(
        'Key event target of control with DOM must be its element',
        control.getElement(), control.getKeyEventTarget());
  },

  /**
   * Tests {@link Control#getKeyHandler}.
   * @suppress {visibility} suppression
   *      added to enable type checking
   */
  testGetKeyHandler() {
    assertUndefined(
        'Key handler must be undefined before getKeyHandler() ' +
            'is called',
        control.keyHandler_);
    /** @suppress {visibility} suppression added to enable type checking */
    const keyHandler = control.getKeyHandler();
    assertNotNull('Key handler must not be null', keyHandler);
    assertEquals(
        'getKeyHandler() must return the same instance if called ' +
            'again',
        keyHandler, control.getKeyHandler());
  },

  /** Tests {@link Control#getRenderer}. */
  testGetRenderer() {
    assertEquals(
        'Renderer must be the default registered renderer',
        registry.getDefaultRenderer(Control), control.getRenderer());
  },

  /** Tests {@link Control#setRenderer}. */
  testSetRenderer() {
    control.createDom();
    assertNotNull('Control must have a DOM', control.getElement());
    assertFalse('Control must not be in the document', control.isInDocument());
    assertEquals(
        'Renderer must be the default registered renderer',
        registry.getDefaultRenderer(Control), control.getRenderer());

    const testRenderer = new TestRenderer();
    control.setRenderer(testRenderer);
    assertNull(
        'Control must not have a DOM after its renderer is reset',
        control.getElement());
    assertFalse(
        'Control still must not be in the document', control.isInDocument());
    assertEquals(
        'Renderer must have expected value', testRenderer,
        control.getRenderer());

    control.render(sandbox);
    assertTrue('Control must be in the document', control.isInDocument());

    assertThrows(
        'Resetting the renderer after the control has entered ' +
            'the document must throw error',
        () => {
          control.setRenderer({});
        });
  },

  /** Tests {@link Control#getExtraClassNames}. */
  testGetExtraClassNames() {
    assertNull(
        'Control must not have any extra class names by default',
        control.getExtraClassNames());
  },

  /**
   * Tests {@link Control#addExtraClassName} and
   * {@link Control#removeExtraClassName}.
   */
  testAddRemoveClassName() {
    assertNull(
        'Control must not have any extra class names by default',
        control.getExtraClassNames());
    control.addClassName('foo');
    assertArrayEquals(
        'Control must have expected extra class names', ['foo'],
        control.getExtraClassNames());
    assertNull('Control must not have a DOM', control.getElement());

    control.createDom();
    assertSameElements(
        'Control\'s element must have expected class names',
        ['goog-control', 'foo'], classlist.get(control.getElement()));

    control.addClassName('bar');
    assertArrayEquals(
        'Control must have expected extra class names', ['foo', 'bar'],
        control.getExtraClassNames());
    assertSameElements(
        'Control\'s element must have expected class names',
        ['goog-control', 'foo', 'bar'], classlist.get(control.getElement()));

    control.addClassName('bar');
    assertArrayEquals(
        'Adding the same class name again must be a no-op', ['foo', 'bar'],
        control.getExtraClassNames());
    assertSameElements(
        'Adding the same class name again must be a no-op',
        ['goog-control', 'foo', 'bar'], classlist.get(control.getElement()));

    control.addClassName(null);
    assertArrayEquals(
        'Adding null class name must be a no-op', ['foo', 'bar'],
        control.getExtraClassNames());
    assertSameElements(
        'Adding null class name must be a no-op',
        ['goog-control', 'foo', 'bar'], classlist.get(control.getElement()));

    control.removeClassName(null);
    assertArrayEquals(
        'Removing null class name must be a no-op', ['foo', 'bar'],
        control.getExtraClassNames());
    assertSameElements(
        'Removing null class name must be a no-op',
        ['goog-control', 'foo', 'bar'], classlist.get(control.getElement()));

    control.removeClassName('foo');
    assertArrayEquals(
        'Control must have expected extra class names', ['bar'],
        control.getExtraClassNames());
    assertSameElements(
        'Control\'s element must have expected class names',
        ['goog-control', 'bar'], classlist.get(control.getElement()));

    control.removeClassName('bar');
    assertNull(
        'Control must not have any extra class names',
        control.getExtraClassNames());
    assertSameElements(
        'Control\'s element must have expected class names', ['goog-control'],
        classlist.get(control.getElement()));
  },

  /** Tests {@link Control#enableClassName}. */
  testEnableClassName() {
    assertNull(
        'Control must not have any extra class names by default',
        control.getExtraClassNames());

    control.enableClassName('foo', true);
    assertArrayEquals(
        'Control must have expected extra class names', ['foo'],
        control.getExtraClassNames());

    control.enableClassName('bar', true);
    assertArrayEquals(
        'Control must have expected extra class names', ['foo', 'bar'],
        control.getExtraClassNames());

    control.enableClassName('bar', true);
    assertArrayEquals(
        'Enabling the same class name again must be a no-op', ['foo', 'bar'],
        control.getExtraClassNames());

    control.enableClassName(null);
    assertArrayEquals(
        'Enabling null class name must be a no-op', ['foo', 'bar'],
        control.getExtraClassNames());

    control.enableClassName('foo', false);
    assertArrayEquals(
        'Control must have expected extra class names', ['bar'],
        control.getExtraClassNames());

    control.enableClassName('bar', false);
    assertNull(
        'Control must not have any extra class names',
        control.getExtraClassNames());
  },

  /** Tests {@link Control#createDom}. */
  testCreateDom() {
    assertNull('Control must not have a DOM by default', control.getElement());
    assertFalse(
        'Control must not allow text selection by default',
        control.isAllowTextSelection());
    assertTrue('Control must be visible by default', control.isVisible());

    control.createDom();
    assertNotNull('Control must have a DOM', control.getElement());
    assertTrue(
        'Control\'s element must be unselectable',
        style.isUnselectable(control.getElement()));
    assertTrue(
        'Control\'s element must be visible',
        control.getElement().style.display != 'none');

    control.setAllowTextSelection(true);
    control.createDom();
    assertFalse(
        'Control\'s element must be selectable',
        style.isUnselectable(control.getElement()));

    control.setVisible(false);
    control.createDom();
    assertTrue(
        'Control\'s element must be hidden',
        control.getElement().style.display == 'none');
  },

  /** Tests {@link Control#getContentElement}. */
  testGetContentElement() {
    assertNull(
        'Unrendered control must not have a content element',
        control.getContentElement());
    control.createDom();
    assertEquals(
        'Control\'s content element must equal its root element',
        control.getElement(), control.getContentElement());
  },

  /** Tests {@link Control#canDecorate}. */
  testCanDecorate() {
    assertTrue(control.canDecorate(dom.createElement(TagName.DIV)));
  },

  /** Tests {@link Control#decorateInternal}. */
  testDecorateInternal() {
    sandbox.innerHTML = '<div id="foo">Hello, <b>World</b>!</div>';
    const foo = dom.getElement('foo');
    control.decorate(foo);
    assertEquals(
        'Decorated control\'s element must have expected value', foo,
        control.getElement());
    assertTrue(
        'Element must be unselectable',
        style.isUnselectable(control.getElement()));
    assertTrue(
        'Element must be visible',
        control.getElement().style.display != 'none');
  },

  /**
   * Tests {@link Control#decorateInternal} with a control that
   * allows text selection.
   */
  testDecorateInternalForSelectableControl() {
    sandbox.innerHTML = '<div id="foo">Hello, <b>World</b>!</div>';
    const foo = dom.getElement('foo');
    control.setAllowTextSelection(true);
    control.decorate(foo);
    assertEquals(
        'Decorated control\'s element must have expected value', foo,
        control.getElement());
    assertFalse(
        'Element must be selectable',
        style.isUnselectable(control.getElement()));
    assertTrue('Control must be visible', control.isVisible());
  },

  /** Tests {@link Control#decorateInternal} with a hidden element. */
  testDecorateInternalForHiddenElement() {
    sandbox.innerHTML = '<div id="foo" style="display:none">Hello!</div>';
    const foo = dom.getElement('foo');
    control.decorate(foo);
    assertEquals(
        'Decorated control\'s element must have expected value', foo,
        control.getElement());
    assertTrue(
        'Element must be unselectable',
        style.isUnselectable(control.getElement()));
    assertFalse('Control must be hidden', control.isVisible());
  },

  /**
   * Tests {@link Control#enterDocument}.
   * @suppress {visibility} suppression
   *      added to enable type checking
   */
  testEnterDocument() {
    control.render(sandbox);
    assertTrue('Control must be in the document', control.isInDocument());
    if (userAgent.IE && !userAgent.isVersionOrHigher(9)) {
      assertEquals(
          'Control must have 6 mouse & 3 key event listeners on IE8', 9,
          getListenerCount(control));
    } else {
      assertEquals(
          'Control must have 5 mouse and 3 key event listeners', 8,
          getListenerCount(control));
    }
    assertEquals(
        'Control\'s key event handler must be attached to its ' +
            'key event target',
        control.getKeyEventTarget(), control.getKeyHandler().element_);
  },

  /**
   * Tests {@link Control#enterDocument} for a control that doesn't
   * handle mouse events.
   * @suppress {visibility} suppression added to enable type checking
   */
  testEnterDocumentForControlWithoutMouseHandling() {
    control.setHandleMouseEvents(false);
    control.render(sandbox);
    assertTrue('Control must be in the document', control.isInDocument());
    assertEquals(
        'Control must have 3 key event listeners', 3,
        getListenerCount(control));
    assertEquals(
        'Control\'s key event handler must be attached to its ' +
            'key event target',
        control.getKeyEventTarget(), control.getKeyHandler().element_);
  },

  /**
   * Tests {@link Control#enterDocument} for a control that isn't
   * focusable.
   * @suppress {visibility} suppression added to enable type checking
   */
  testEnterDocumentForNonFocusableControl() {
    control.setSupportedState(Component.State.FOCUSED, false);
    control.render(sandbox);
    assertTrue('Control must be in the document', control.isInDocument());
    if (userAgent.IE && !userAgent.isVersionOrHigher(9)) {
      assertEquals(
          'Control must have 6 mouse event listeners on IE8', 6,
          getListenerCount(control));
    } else {
      assertEquals(
          'Control must have 5 mouse event listeners', 5,
          getListenerCount(control));
    }
    assertUndefined(
        'Control must not have a key event handler', control.keyHandler_);
  },

  /**
   * Tests {@link Control#enterDocument} for a control that doesn't
   * need to do any event handling.
   * @suppress {visibility} suppression added to enable type checking
   */
  testEnterDocumentForControlWithoutEventHandlers() {
    control.setHandleMouseEvents(false);
    control.setSupportedState(Component.State.FOCUSED, false);
    control.render(sandbox);
    assertTrue('Control must be in the document', control.isInDocument());
    assertEquals(
        'Control must have 0 event listeners', 0, getListenerCount(control));
    assertUndefined(
        'Control must not have an event handler',
        control.googUiComponentHandler_);
    assertUndefined(
        'Control must not have a key event handler', control.keyHandler_);
  },

  /**
   * Tests {@link Control#exitDocument}.
   * @suppress {visibility} suppression
   *      added to enable type checking
   */
  testExitDocument() {
    control.render(sandbox);
    assertTrue('Control must be in the document', control.isInDocument());
    if (userAgent.IE && !userAgent.isVersionOrHigher(9)) {
      assertEquals(
          'Control must have 6 mouse & 3 key event listeners on IE8', 9,
          getListenerCount(control));
    } else {
      assertEquals(
          'Control must have 5 mouse and 3 key event listeners', 8,
          getListenerCount(control));
    }
    assertEquals(
        'Control\'s key event handler must be attached to its ' +
            'key event target',
        control.getKeyEventTarget(), control.getKeyHandler().element_);
    assertTrue(
        'Control\'s element must support keyboard focus',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));

    control.exitDocument();
    assertFalse(
        'Control must no longer be in the document', control.isInDocument());
    assertEquals(
        'Control must have no event listeners', 0, getListenerCount(control));
    assertNull(
        'Control\'s key event handler must be unattached',
        control.getKeyHandler().element_);
    assertFalse(
        'Control\'s element must no longer support keyboard focus',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));
  },

  /**
   * Tests {@link Control#dispose}.
   * @suppress {visibility} suppression added to
   *      enable type checking
   */
  testDispose() {
    control.render(sandbox);
    /** @suppress {visibility} suppression added to enable type checking */
    const handler = control.getHandler();
    /** @suppress {visibility} suppression added to enable type checking */
    const keyHandler = control.getKeyHandler();
    control.dispose();
    assertFalse(
        'Control must no longer be in the document', control.isInDocument());
    assertTrue('Control must have been disposed of', control.isDisposed());
    assertUndefined('Renderer must have been deleted', control.getRenderer());
    assertNull('Content must be nulled out', control.getContent());
    assertTrue(
        'Event handler must have been disposed of', handler.isDisposed());
    assertUndefined(
        'Event handler must have been deleted',
        control.googUiComponentHandler_);
    assertTrue(
        'Key handler must have been disposed of', keyHandler.isDisposed());
    assertUndefined('Key handler must have been deleted', control.keyHandler_);
    assertNull(
        'Extra class names must have been nulled out',
        control.getExtraClassNames());
  },

  /** Tests {@link Control#getContent}. */
  testGetContent() {
    assertNull(
        'Empty control must have null content',
        (new Control(null)).getContent());
    assertEquals(
        'Control must have expected content', 'Hello', control.getContent());
    control.render(sandbox);
    assertEquals(
        'Control must have expected content after rendering', 'Hello',
        control.getContent());
  },

  /**
   * Tests {@link Control#getContent}.
   * @suppress {checkTypes} suppression added
   *      to enable type checking
   */
  testGetContentForDecoratedControl() {
    sandbox.innerHTML = '<div id="empty"></div>\n' +
        '<div id="text">Hello, world!</div>\n' +
        '<div id="element"><span>Foo</span></div>\n' +
        '<div id="nodelist">Hello, <b>world</b>!</div>\n';

    const empty = new Control(null);
    empty.decorate(dom.getElement('empty'));
    assertNull(
        'Content of control decorating empty DIV must be null',
        empty.getContent());
    empty.dispose();

    const text = new Control(null);
    text.decorate(dom.getElement('text'));
    assertEquals(
        'Content of control decorating DIV with text contents ' +
            'must be as expected',
        'Hello, world!', text.getContent().nodeValue);
    text.dispose();

    const element = new Control(null);
    element.decorate(dom.getElement('element'));
    assertEquals(
        'Content of control decorating DIV with element child ' +
            'must be as expected',
        dom.getElement('element').firstChild, element.getContent());
    element.dispose();

    const nodelist = new Control(null);
    nodelist.decorate(dom.getElement('nodelist'));
    assertSameElements(
        'Content of control decorating DIV with mixed ' +
            'contents must be as expected',
        dom.getElement('nodelist').childNodes, nodelist.getContent());
    nodelist.dispose();
  },

  /** Tests {@link Control#setAriaLabel}. */
  testSetAriaLabel_render() {
    assertNull(
        'Controls must not have any aria label by default',
        control.getAriaLabel());

    control.setAriaLabel('label');
    assertEquals(
        'Control must have aria label', 'label', control.getAriaLabel());

    control.render(sandbox);

    const elem = control.getElementStrict();
    assertEquals(
        'Element must have control\'s aria label after rendering', 'label',
        aria.getLabel(elem));

    control.setAriaLabel('new label');
    assertEquals(
        'Element must have the new aria label', 'new label',
        aria.getLabel(elem));
  },

  /** Tests {@link Control#setAriaLabel}. */
  testSetAriaLabel_decorate() {
    assertNull(
        'Controls must not have any aria label by default',
        control.getAriaLabel());

    control.setAriaLabel('label');
    assertEquals(
        'Control must have aria label', 'label', control.getAriaLabel());

    sandbox.innerHTML = '<div id="nodelist" role="button">' +
        'Hello, <b>world</b>!</div>';
    control.decorate(dom.getElement('nodelist'));

    const elem = control.getElementStrict();
    assertEquals(
        'Element must have control\'s aria label after rendering', 'label',
        aria.getLabel(elem));
    assertEquals(
        'Element must have the correct role', 'button',
        elem.getAttribute('role'));

    control.setAriaLabel('new label');
    assertEquals(
        'Element must have the new aria label', 'new label',
        aria.getLabel(elem));
  },

  /** Tests {@link Control#setContent}. */
  testSetContent() {
    control.setContent('Bye');
    assertEquals(
        'Unrendered control control must have expected contents', 'Bye',
        control.getContent());
    assertNull('No DOM must be created by setContent', control.getElement());

    control.createDom();
    assertEquals(
        'Rendered control\'s DOM must have expected contents', 'Bye',
        control.getElement().innerHTML);

    control.setContent(null);
    assertNull(
        'Rendered control must have expected contents', control.getContent());
    assertEquals(
        'Rendered control\'s DOM must have expected contents', '',
        control.getElement().innerHTML);

    control.setContent([
      dom.createDom(
          TagName.DIV, null, dom.createDom(TagName.SPAN, null, 'Hello')),
      'World',
    ]);
    assertHTMLEquals(
        'Control\'s DOM must be updated', '<div><span>Hello</span></div>World',
        control.getElement().innerHTML);
  },

  /** Tests {@link Control#setContentInternal}. */
  testSetContentInternal() {
    control.render(sandbox);
    assertEquals(
        'Control must have expected content after rendering', 'Hello',
        control.getContent());
    control.setContentInternal('Bye');
    assertEquals(
        'Control must have expected contents', 'Bye', control.getContent());
    assertEquals(
        'Control\'s DOM must be unchanged', 'Hello',
        control.getElement().innerHTML);
  },

  /** Tests {@link Control#getCaption}. */
  testGetCaption() {
    assertEquals(
        'Empty control\'s caption must be empty string', '',
        (new Control(null)).getCaption());

    assertEquals(
        'Caption must have expected value', 'Hello', control.getCaption());

    sandbox.innerHTML = '<div id="nodelist">Hello, <b>world</b>!</div>';
    control.decorate(dom.getElement('nodelist'));
    assertEquals(
        'Caption must have expected value', 'Hello, world!',
        control.getCaption());

    const arrayContent =
        googArray.clone(dom.safeHtmlToNode(testing.newSafeHtmlForTest(
                                               ' <b> foo</b><i>  bar</i> '))
                            .childNodes);
    control.setContent(arrayContent);
    assertEquals(
        'whitespaces must be normalized in the caption', 'foo bar',
        control.getCaption());

    control.setContent('\xa0foo');
    assertEquals(
        'indenting spaces must be kept', '\xa0foo', control.getCaption());
  },

  /** Tests {@link Control#setCaption}. */
  testSetCaption() {
    control.setCaption('Hello, world!');
    assertEquals(
        'Control must have a string caption "Hello, world!"', 'Hello, world!',
        control.getCaption());
  },

  /** Tests {@link Control#setRightToLeft}. */
  testSetRightToLeft() {
    control.createDom();
    assertFalse(
        'Control\'s element must not have right-to-left class',
        classlist.contains(control.getElement(), 'goog-control-rtl'));
    control.setRightToLeft(true);
    assertTrue(
        'Control\'s element must have right-to-left class',
        classlist.contains(control.getElement(), 'goog-control-rtl'));
    control.render(sandbox);
    assertThrows(
        'Changing the render direction of a control already in ' +
            'the document is an error',
        () => {
          control.setRightToLeft(false);
        });
  },

  /** Tests {@link Control#isAllowTextSelection}. */
  testIsAllowTextSelection() {
    assertFalse(
        'Controls must not allow text selection by default',
        control.isAllowTextSelection());
  },

  /** Tests {@link Control#setAllowTextSelection}. */
  testSetAllowTextSelection() {
    assertFalse(
        'Controls must not allow text selection by default',
        control.isAllowTextSelection());

    control.setAllowTextSelection(true);
    assertTrue(
        'Control must allow text selection', control.isAllowTextSelection());

    control.setAllowTextSelection(false);
    assertFalse(
        'Control must no longer allow text selection',
        control.isAllowTextSelection());

    control.render(sandbox);

    assertFalse(
        'Control must not allow text selection even after rendered',
        control.isAllowTextSelection());

    control.setAllowTextSelection(true);
    assertTrue(
        'Control must once again allow text selection',
        control.isAllowTextSelection());
  },

  /** Tests {@link Control#isVisible}. */
  testIsVisible() {
    assertTrue('Controls must be visible by default', control.isVisible());
  },

  /** Tests {@link Control#setVisible} before it is rendered. */
  testSetVisible() {
    assertFalse(
        'setVisible(true) must return false if already visible',
        control.setVisible(true));
    assertTrue('No events must have been dispatched', noEventsDispatched());

    assertTrue(
        'setVisible(false) must return true if previously visible',
        control.setVisible(false));
    assertEquals(
        'One HIDE event must have been dispatched', 1,
        getEventCount(control, Component.EventType.HIDE));
    assertFalse('Control must no longer be visible', control.isVisible());

    assertTrue(
        'setVisible(true) must return true if previously hidden',
        control.setVisible(true));
    assertEquals(
        'One SHOW event must have been dispatched', 1,
        getEventCount(control, Component.EventType.SHOW));
    assertTrue('Control must be visible', control.isVisible());
  },

  /** Tests {@link Control#setVisible} after it is rendered. */
  testSetVisibleForRenderedControl() {
    control.render(sandbox);
    assertTrue(
        'No events must have been dispatched during rendering',
        noEventsDispatched());

    assertFalse(
        'setVisible(true) must return false if already visible',
        control.setVisible(true));
    assertTrue('No events must have been dispatched', noEventsDispatched());
    assertTrue(
        'Control\'s element must be visible',
        control.getElement().style.display != 'none');

    assertTrue(
        'setVisible(false) must return true if previously visible',
        control.setVisible(false));
    assertEquals(
        'One HIDE event must have been dispatched', 1,
        getEventCount(control, Component.EventType.HIDE));
    assertFalse('Control must no longer be visible', control.isVisible());
    assertTrue(
        'Control\'s element must be hidden',
        control.getElement().style.display == 'none');

    assertTrue(
        'setVisible(true) must return true if previously hidden',
        control.setVisible(true));
    assertEquals(
        'One SHOW event must have been dispatched', 1,
        getEventCount(control, Component.EventType.SHOW));
    assertTrue('Control must be visible', control.isVisible());
    assertTrue(
        'Control\'s element must be visible',
        control.getElement().style.display != 'none');
  },

  /**
   * Tests {@link Control#setVisible} for disabled non-focusable
   * controls.
   */
  testSetVisibleForDisabledNonFocusableControl() {
    // Hidden, disabled, non-focusable control becoming visible.
    control.setEnabled(false);
    control.setSupportedState(Component.State.FOCUSED, false);
    control.render(sandbox);
    assertTrue('Control must be visible', control.isVisible());
    assertFalse(
        'Control must not have a tab index',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));

    // Visible, disabled, non-focusable control becoming hidden.
    control.getKeyEventTarget().focus();
    assertEquals(
        'Control must not have dispatched FOCUS', 0,
        getEventCount(control, Component.EventType.FOCUS));
    assertFalse('Control must not have keyboard focus', control.isFocused());
    control.setVisible(false);
    assertFalse('Control must be hidden', control.isVisible());
    assertFalse(
        'Control must not have a tab index',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));
    assertEquals(
        'Control must have dispatched HIDE', 1,
        getEventCount(control, Component.EventType.HIDE));
    assertEquals(
        'Control must not have dispatched BLUR', 0,
        getEventCount(control, Component.EventType.BLUR));
  },

  /** Tests {@link Control#setVisible} for disabled focusable controls. */
  testSetVisibleForDisabledFocusableControl() {
    // Hidden, disabled, focusable control becoming visible.
    control.setEnabled(false);
    control.setSupportedState(Component.State.FOCUSED, true);
    control.render(sandbox);
    assertTrue('Control must be visible', control.isVisible());
    assertFalse(
        'Control must not have a tab index',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));

    // Visible, disabled, focusable control becoming hidden.
    control.getKeyEventTarget().focus();
    assertEquals(
        'Control must not have dispatched FOCUS', 0,
        getEventCount(control, Component.EventType.FOCUS));
    assertFalse('Control must not have keyboard focus', control.isFocused());
    control.setVisible(false);
    assertFalse('Control must be hidden', control.isVisible());
    assertFalse(
        'Control must not have a tab index',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));
    assertEquals(
        'Control must have dispatched HIDE', 1,
        getEventCount(control, Component.EventType.HIDE));
    assertEquals(
        'Control must not have dispatched BLUR', 0,
        getEventCount(control, Component.EventType.BLUR));
  },

  /**
   * Tests {@link Control#setVisible} for enabled non-focusable
   * controls.
   */
  testSetVisibleForEnabledNonFocusableControl() {
    // Hidden, enabled, non-focusable control becoming visible.
    control.setEnabled(true);
    control.setSupportedState(Component.State.FOCUSED, false);
    control.render(sandbox);
    assertTrue('Control must be visible', control.isVisible());
    assertFalse(
        'Control must not have a tab index',
        dom.isFocusableTabIndex(control.getKeyEventTarget()));

    if (testFocus) {
      // Visible, enabled, non-focusable control becoming hidden.
      control.getKeyEventTarget().focus();
      assertEquals(
          'Control must not have dispatched FOCUS', 0,
          getEventCount(control, Component.EventType.FOCUS));
      assertFalse('Control must not have keyboard focus', control.isFocused());
      control.setVisible(false);
      assertFalse('Control must be hidden', control.isVisible());
      assertFalse(
          'Control must not have a tab index',
          dom.isFocusableTabIndex(control.getKeyEventTarget()));
      assertEquals(
          'Control must have dispatched HIDE', 1,
          getEventCount(control, Component.EventType.HIDE));
      assertEquals(
          'Control must not have dispatched BLUR', 0,
          getEventCount(control, Component.EventType.BLUR));
    }
  },

  /** Tests {@link Control#setVisible} for enabled focusable controls. */
  testSetVisibleForEnabledFocusableControl() {
    // Hidden, enabled, focusable control becoming visible.
    control.setEnabled(true);
    control.setSupportedState(Component.State.FOCUSED, true);
    control.render(sandbox);
    assertTrue('Control must be visible', control.isVisible());

    if (testFocus) {
      // Mac Safari currently doesn't support tabIndex on arbitrary
      // elements.
      assertTrue(
          'Control must have a tab index',
          dom.isFocusableTabIndex(control.getKeyEventTarget()));

      // Visible, enabled, focusable control becoming hidden.
      control.getKeyEventTarget().focus();

      // Expected to fail on IE.
      expectedFailures.expectFailureFor(userAgent.IE);
      try {
        // IE dispatches focus and blur events asynchronously!
        assertEquals(
            'Control must have dispatched FOCUS', 1,
            getEventCount(control, Component.EventType.FOCUS));
        assertTrue('Control must have keyboard focus', control.isFocused());
      } catch (e) {
        expectedFailures.handleException(e);
      }

      control.setVisible(false);
      assertFalse('Control must be hidden', control.isVisible());
      assertFalse(
          'Control must not have a tab index',
          dom.isFocusableTabIndex(control.getKeyEventTarget()));
      assertEquals(
          'Control must have dispatched HIDE', 1,
          getEventCount(control, Component.EventType.HIDE));

      // Expected to fail on IE.
      expectedFailures.expectFailureFor(userAgent.IE);
      try {
        // IE dispatches focus and blur events asynchronously!
        assertEquals(
            'Control must have dispatched BLUR', 1,
            getEventCount(control, Component.EventType.BLUR));
        assertFalse(
            'Control must no longer have keyboard focus', control.isFocused());
      } catch (e) {
        expectedFailures.handleException(e);
      }
    }
  },

  /** Tests {@link Control#isEnabled}. */
  testIsEnabled() {
    assertTrue('Controls must be enabled by default', control.isEnabled());
  },

  /** Tests {@link Control#setEnabled}. */
  testSetEnabled() {
    control.render(sandbox);
    control.setHighlighted(true);
    control.setActive(true);
    control.getKeyEventTarget().focus();

    resetEventCount();

    control.setEnabled(true);
    assertTrue('No events must have been dispatched', noEventsDispatched());
    assertTrue('Control must be enabled', control.isEnabled());
    assertTrue('Control must be highlighted', control.isHighlighted());
    assertTrue('Control must be active', control.isActive());
    let elem = control.getElementStrict();
    assertTrue(
        'Control element must not have aria-disabled',
        googString.isEmptyOrWhitespace(aria.getState(elem, State.DISABLED)));
    assertEquals(
        'Control element must have a tabIndex of 0', 0,
        googString.toNumber(elem.getAttribute('tabIndex') || ''));

    if (testFocus) {
      // Expected to fail on IE and Mac Safari 3.  IE calls focus handlers
      // asynchronously, and Mac Safari 3 doesn't support keyboard focus.
      expectedFailures.expectFailureFor(userAgent.IE);
      assertTrue('Control must be focused', control.isFocused());
    }

    resetEventCount();

    control.setEnabled(false);
    assertEquals(
        'One DISABLE event must have been dispatched', 1,
        getEventCount(control, Component.EventType.DISABLE));
    assertFalse('Control must be disabled', control.isEnabled());
    assertFalse('Control must not be highlighted', control.isHighlighted());
    assertFalse('Control must not be active', control.isActive());
    assertFalse('Control must not be focused', control.isFocused());
    assertEquals(
        'Control element must have aria-disabled true', 'true',
        aria.getState(control.getElementStrict(), State.DISABLED));
    assertNull(
        'Control element must not have a tabIndex',
        control.getElement().getAttribute('tabIndex'));

    control.setEnabled(true);
    control.exitDocument();
    const cssClass = goog.getCssName(ControlRenderer.CSS_CLASS, 'disabled');
    const element = dom.createDom(TagName.DIV, {tabIndex: 0});
    element.className = cssClass;
    dom.appendChild(sandbox, element);
    control.decorate(element);
    assertEquals(
        'Control element must have aria-disabled true', 'true',
        aria.getState(control.getElementStrict(), State.DISABLED));
    assertNull(
        'Control element must not have a tabIndex',
        control.getElement().getAttribute('tabIndex'));
    control.setEnabled(true);
    elem = control.getElementStrict();
    assertEquals(
        'Control element must have aria-disabled false', 'false',
        aria.getState(elem, State.DISABLED));
    assertEquals(
        'Control element must have tabIndex 0', 0,
        googString.toNumber(elem.getAttribute('tabIndex') || ''));
  },

  /**
   * Tests {@link Control#setState} when using
   * Component.State.DISABLED.
   */
  testSetStateWithDisabled() {
    control.render(sandbox);
    control.setHighlighted(true);
    control.setActive(true);
    control.getKeyEventTarget().focus();

    resetEventCount();

    control.setState(Component.State.DISABLED, false);
    assertTrue('No events must have been dispatched', noEventsDispatched());
    assertTrue('Control must be enabled', control.isEnabled());
    assertTrue('Control must be highlighted', control.isHighlighted());
    assertTrue('Control must be active', control.isActive());
    assertTrue(
        'Control element must not have aria-disabled',
        googString.isEmptyOrWhitespace(
            aria.getState(control.getElementStrict(), State.DISABLED)));
    assertEquals(
        'Control element must have a tabIndex of 0', 0,
        googString.toNumber(
            control.getElement().getAttribute('tabIndex') || ''));

    if (testFocus) {
      // Expected to fail on IE and Mac Safari 3.  IE calls focus handlers
      // asynchronously, and Mac Safari 3 doesn't support keyboard focus.
      expectedFailures.expectFailureFor(userAgent.IE);
      try {
        assertTrue('Control must be focused', control.isFocused());
      } catch (e) {
        expectedFailures.handleException(e);
      }
    }

    resetEventCount();

    control.setState(Component.State.DISABLED, true);
    assertEquals(
        'One DISABLE event must have been dispatched', 1,
        getEventCount(control, Component.EventType.DISABLE));
    assertFalse('Control must be disabled', control.isEnabled());
    assertFalse('Control must not be highlighted', control.isHighlighted());
    assertFalse('Control must not be active', control.isActive());
    assertFalse('Control must not be focused', control.isFocused());
    assertEquals(
        'Control element must have aria-disabled true', 'true',
        aria.getState(control.getElementStrict(), State.DISABLED));
    assertNull(
        'Control element must not have a tabIndex',
        control.getElement().getAttribute('tabIndex'));

    control.setState(Component.State.DISABLED, false);
    control.exitDocument();
    const cssClass = goog.getCssName(ControlRenderer.CSS_CLASS, 'disabled');
    const element = dom.createDom(TagName.DIV, {tabIndex: 0});
    element.className = cssClass;
    dom.appendChild(sandbox, element);
    control.decorate(element);
    assertEquals(
        'Control element must have aria-disabled true', 'true',
        aria.getState(control.getElementStrict(), State.DISABLED));
    assertNull(
        'Control element must not have a tabIndex',
        control.getElement().getAttribute('tabIndex'));
    control.setState(Component.State.DISABLED, false);
    let elem = control.getElementStrict();
    assertEquals(
        'Control element must have aria-disabled false', 'false',
        aria.getState(elem, State.DISABLED));
    assertEquals(
        'Control element must have tabIndex 0', 0,
        googString.toNumber(elem.getAttribute('tabIndex') || ''));
  },

  /** Tests {@link Control#setEnabled} when the control has a parent. */
  testSetEnabledWithParent() {
    const child = new Control(null);
    child.setDispatchTransitionEvents(Component.State.ALL, true);
    control.addChild(child, true /* opt_render */);
    control.setEnabled(false);

    resetEventCount();

    assertFalse('Parent must be disabled', control.isEnabled());
    assertTrue('Child must be enabled', child.isEnabled());

    child.setEnabled(false);
    assertTrue(
        'No events must have been dispatched when child is disabled',
        noEventsDispatched());
    assertTrue('Child must still be enabled', child.isEnabled());

    resetEventCount();

    control.setEnabled(true);
    assertEquals(
        'One ENABLE event must have been dispatched by the parent', 1,
        getEventCount(control, Component.EventType.ENABLE));
    assertTrue('Parent must be enabled', control.isEnabled());
    assertTrue('Child must still be enabled', child.isEnabled());

    resetEventCount();

    child.setEnabled(false);
    assertEquals(
        'One DISABLE event must have been dispatched by the child', 1,
        getEventCount(child, Component.EventType.DISABLE));
    assertTrue('Parent must still be enabled', control.isEnabled());
    assertFalse('Child must now be disabled', child.isEnabled());

    resetEventCount();

    control.setEnabled(false);
    assertEquals(
        'One DISABLE event must have been dispatched by the parent', 1,
        getEventCount(control, Component.EventType.DISABLE));
    assertFalse('Parent must now be disabled', control.isEnabled());
    assertFalse('Child must still be disabled', child.isEnabled());

    child.dispose();
  },

  /** Tests {@link Control#isHighlighted}. */
  testIsHighlighted() {
    assertFalse(
        'Controls must not be highlighted by default', control.isHighlighted());
  },

  /** Tests {@link Control#setHighlighted}. */
  testSetHighlighted() {
    control.setSupportedState(Component.State.HOVER, false);

    control.setHighlighted(true);
    assertFalse(
        'Control must not be highlighted, because it isn\'t ' +
            'highlightable',
        control.isHighlighted());
    assertTrue(
        'Control must not have dispatched any events', noEventsDispatched());

    control.setSupportedState(Component.State.HOVER, true);

    control.setHighlighted(true);
    assertTrue('Control must be highlighted', control.isHighlighted());
    assertEquals(
        'Control must have dispatched a HIGHLIGHT event', 1,
        getEventCount(control, Component.EventType.HIGHLIGHT));

    control.setHighlighted(true);
    assertTrue('Control must still be highlighted', control.isHighlighted());
    assertEquals(
        'Control must not dispatch more HIGHLIGHT events', 1,
        getEventCount(control, Component.EventType.HIGHLIGHT));

    control.setHighlighted(false);
    assertFalse('Control must not be highlighted', control.isHighlighted());
    assertEquals(
        'Control must have dispatched an UNHIGHLIGHT event', 1,
        getEventCount(control, Component.EventType.UNHIGHLIGHT));
    control.setEnabled(false);
    assertFalse('Control must be disabled', control.isEnabled());

    control.setHighlighted(true);
    assertTrue(
        'Control must be highlighted, even when disabled',
        control.isHighlighted());
    assertEquals(
        'Control must have dispatched another HIGHLIGHT event', 2,
        getEventCount(control, Component.EventType.HIGHLIGHT));
  },

  /** Tests {@link Control#isActive}. */
  testIsActive() {
    assertFalse('Controls must not be active by default', control.isActive());
  },

  /** Tests {@link Control#setActive}. */
  testSetActive() {
    control.setSupportedState(Component.State.ACTIVE, false);

    control.setActive(true);
    assertFalse(
        'Control must not be active, because it isn\'t activateable',
        control.isActive());
    assertTrue(
        'Control must not have dispatched any events', noEventsDispatched());

    control.setSupportedState(Component.State.ACTIVE, true);

    control.setActive(true);
    assertTrue('Control must be active', control.isActive());
    assertEquals(
        'Control must have dispatched an ACTIVATE event', 1,
        getEventCount(control, Component.EventType.ACTIVATE));

    control.setActive(true);
    assertTrue('Control must still be active', control.isActive());
    assertEquals(
        'Control must not dispatch more ACTIVATE events', 1,
        getEventCount(control, Component.EventType.ACTIVATE));

    control.setEnabled(false);
    assertFalse('Control must be disabled', control.isEnabled());
    assertFalse('Control must not be active', control.isActive());
    assertEquals(
        'Control must have dispatched a DEACTIVATE event', 1,
        getEventCount(control, Component.EventType.DEACTIVATE));
  },

  /**
     Tests disposing the control from an action event handler.
     @suppress {visibility} suppression added to enable type checking
   */
  testDisposeOnAction() {
    googEvents.listen(control, Component.EventType.ACTION, (e) => {
      control.dispose();
    });

    // Control must not throw an exception if disposed of in an ACTION event
    // handler.
    control.performActionInternal();
    control.setActive(true);
    assertTrue('Control should have been disposed of', control.isDisposed());
  },

  /** Tests {@link Control#isSelected}. */
  testIsSelected() {
    assertFalse(
        'Controls must not be selected by default', control.isSelected());
  },

  /** Tests {@link Control#setSelected}. */
  testSetSelected() {
    control.setSupportedState(Component.State.SELECTED, false);

    control.setSelected(true);
    assertFalse(
        'Control must not be selected, because it isn\'t selectable',
        control.isSelected());
    assertTrue(
        'Control must not have dispatched any events', noEventsDispatched());

    control.setSupportedState(Component.State.SELECTED, true);

    control.setSelected(true);
    assertTrue('Control must be selected', control.isSelected());
    assertEquals(
        'Control must have dispatched a SELECT event', 1,
        getEventCount(control, Component.EventType.SELECT));

    control.setSelected(true);
    assertTrue('Control must still be selected', control.isSelected());
    assertEquals(
        'Control must not dispatch more SELECT events', 1,
        getEventCount(control, Component.EventType.SELECT));

    control.setSelected(false);
    assertFalse('Control must not be selected', control.isSelected());
    assertEquals(
        'Control must have dispatched an UNSELECT event', 1,
        getEventCount(control, Component.EventType.UNSELECT));
    control.setEnabled(false);
    assertFalse('Control must be disabled', control.isEnabled());

    control.setSelected(true);
    assertTrue(
        'Control must be selected, even when disabled', control.isSelected());
    assertEquals(
        'Control must have dispatched another SELECT event', 2,
        getEventCount(control, Component.EventType.SELECT));
  },

  /** Tests {@link Control#isChecked}. */
  testIsChecked() {
    assertFalse('Controls must not be checked by default', control.isChecked());
  },

  /** Tests {@link Control#setChecked}. */
  testSetChecked() {
    control.setSupportedState(Component.State.CHECKED, false);

    control.setChecked(true);
    assertFalse(
        'Control must not be checked, because it isn\'t checkable',
        control.isChecked());
    assertTrue(
        'Control must not have dispatched any events', noEventsDispatched());

    control.setSupportedState(Component.State.CHECKED, true);

    control.setChecked(true);
    assertTrue('Control must be checked', control.isChecked());
    assertEquals(
        'Control must have dispatched a CHECK event', 1,
        getEventCount(control, Component.EventType.CHECK));

    control.setChecked(true);
    assertTrue('Control must still be checked', control.isChecked());
    assertEquals(
        'Control must not dispatch more CHECK events', 1,
        getEventCount(control, Component.EventType.CHECK));

    control.setChecked(false);
    assertFalse('Control must not be checked', control.isChecked());
    assertEquals(
        'Control must have dispatched an UNCHECK event', 1,
        getEventCount(control, Component.EventType.UNCHECK));
    control.setEnabled(false);
    assertFalse('Control must be disabled', control.isEnabled());

    control.setChecked(true);
    assertTrue(
        'Control must be checked, even when disabled', control.isChecked());
    assertEquals(
        'Control must have dispatched another CHECK event', 2,
        getEventCount(control, Component.EventType.CHECK));
  },

  /** Tests {@link Control#isFocused}. */
  testIsFocused() {
    assertFalse('Controls must not be focused by default', control.isFocused());
  },

  /** Tests {@link Control#setFocused}. */
  testSetFocused() {
    control.setSupportedState(Component.State.FOCUSED, false);

    control.setFocused(true);
    assertFalse(
        'Control must not be focused, because it isn\'t focusable',
        control.isFocused());
    assertTrue(
        'Control must not have dispatched any events', noEventsDispatched());

    control.setSupportedState(Component.State.FOCUSED, true);

    control.setFocused(true);
    assertTrue('Control must be focused', control.isFocused());
    assertEquals(
        'Control must have dispatched a FOCUS event', 1,
        getEventCount(control, Component.EventType.FOCUS));

    control.setFocused(true);
    assertTrue('Control must still be focused', control.isFocused());
    assertEquals(
        'Control must not dispatch more FOCUS events', 1,
        getEventCount(control, Component.EventType.FOCUS));

    control.setFocused(false);
    assertFalse('Control must not be focused', control.isFocused());
    assertEquals(
        'Control must have dispatched an BLUR event', 1,
        getEventCount(control, Component.EventType.BLUR));
    control.setEnabled(false);
    assertFalse('Control must be disabled', control.isEnabled());

    control.setFocused(true);
    assertTrue(
        'Control must be focused, even when disabled', control.isFocused());
    assertEquals(
        'Control must have dispatched another FOCUS event', 2,
        getEventCount(control, Component.EventType.FOCUS));
  },

  /** Tests {@link Control#isOpen}. */
  testIsOpen() {
    assertFalse('Controls must not be open by default', control.isOpen());
  },

  /** Tests {@link Control#setOpen}. */
  testSetOpen() {
    control.setSupportedState(Component.State.OPENED, false);

    control.setOpen(true);
    assertFalse(
        'Control must not be opened, because it isn\'t openable',
        control.isOpen());
    assertTrue(
        'Control must not have dispatched any events', noEventsDispatched());

    control.setSupportedState(Component.State.OPENED, true);

    control.setOpen(true);
    assertTrue('Control must be opened', control.isOpen());
    assertEquals(
        'Control must have dispatched a OPEN event', 1,
        getEventCount(control, Component.EventType.OPEN));

    control.setOpen(true);
    assertTrue('Control must still be opened', control.isOpen());
    assertEquals(
        'Control must not dispatch more OPEN events', 1,
        getEventCount(control, Component.EventType.OPEN));

    control.setOpen(false);
    assertFalse('Control must not be opened', control.isOpen());
    assertEquals(
        'Control must have dispatched an CLOSE event', 1,
        getEventCount(control, Component.EventType.CLOSE));
    control.setEnabled(false);
    assertFalse('Control must be disabled', control.isEnabled());

    control.setOpen(true);
    assertTrue('Control must be opened, even when disabled', control.isOpen());
    assertEquals(
        'Control must have dispatched another OPEN event', 2,
        getEventCount(control, Component.EventType.OPEN));
  },

  /** Tests {@link Control#getState}. */
  testGetState() {
    assertEquals(
        'Controls must be in the default state', 0x00, control.getState());
  },

  /**
   * Tests {@link Control#hasState}.
   * @suppress {checkTypes} suppression added
   *      to enable type checking
   */
  testHasState() {
    assertFalse(
        'Control must not be disabled',
        control.hasState(Component.State.DISABLED));
    assertFalse(
        'Control must not be in the HOVER state',
        control.hasState(Component.State.HOVER));
    assertFalse(
        'Control must not be active', control.hasState(Component.State.ACTIVE));
    assertFalse(
        'Control must not be selected',
        control.hasState(Component.State.SELECTED));
    assertFalse(
        'Control must not be checked',
        control.hasState(Component.State.CHECKED));
    assertFalse(
        'Control must not be focused',
        control.hasState(Component.State.FOCUSED));
    assertFalse(
        'Control must not be open', control.hasState(Component.State.OPEN));
  },

  /** Tests {@link Control#setState}. */
  testSetState() {
    control.createDom();
    control.setSupportedState(Component.State.ACTIVE, false);

    assertFalse(
        'Control must not be active', control.hasState(Component.State.ACTIVE));
    control.setState(Component.State.ACTIVE, true);
    assertFalse(
        'Control must still be inactive (because it doesn\'t ' +
            'support the ACTIVE state)',
        control.hasState(Component.State.ACTIVE));

    control.setSupportedState(Component.State.ACTIVE, true);

    control.setState(Component.State.ACTIVE, true);
    assertTrue(
        'Control must be active', control.hasState(Component.State.ACTIVE));
    assertTrue(
        'Control must have the active CSS style',
        classlist.contains(control.getElement(), 'goog-control-active'));

    control.setState(Component.State.ACTIVE, true);
    assertTrue(
        'Control must still be active',
        control.hasState(Component.State.ACTIVE));
    assertTrue(
        'Control must still have the active CSS style',
        classlist.contains(control.getElement(), 'goog-control-active'));

    assertTrue('No events must have been dispatched', noEventsDispatched());
  },

  /** Tests {@link Control#setStateInternal}. */
  testSetStateInternal() {
    control.setStateInternal(0x00);
    assertEquals('State should be 0x00', 0x00, control.getState());
    control.setStateInternal(0x17);
    assertEquals('State should be 0x17', 0x17, control.getState());
  },

  /** Tests {@link Control#isSupportedState}. */
  testIsSupportedState() {
    assertTrue(
        'Control must support DISABLED',
        control.isSupportedState(Component.State.DISABLED));
    assertTrue(
        'Control must support HOVER',
        control.isSupportedState(Component.State.HOVER));
    assertTrue(
        'Control must support ACTIVE',
        control.isSupportedState(Component.State.ACTIVE));
    assertTrue(
        'Control must support FOCUSED',
        control.isSupportedState(Component.State.FOCUSED));
    assertFalse(
        'Control must no support SELECTED',
        control.isSupportedState(Component.State.SELECTED));
    assertFalse(
        'Control must no support CHECKED',
        control.isSupportedState(Component.State.CHECKED));
    assertFalse(
        'Control must no support OPENED',
        control.isSupportedState(Component.State.OPENED));
  },

  /** Tests {@link Control#setSupportedState}. */
  testSetSupportedState() {
    control.setSupportedState(Component.State.HOVER, true);
    assertTrue(
        'Control must still support HOVER',
        control.isSupportedState(Component.State.HOVER));

    control.setSupportedState(Component.State.HOVER, false);
    assertFalse(
        'Control must no longer support HOVER',
        control.isSupportedState(Component.State.HOVER));

    control.setState(Component.State.ACTIVE, true);
    control.setSupportedState(Component.State.ACTIVE, false);
    assertFalse(
        'Control must no longer support ACTIVE',
        control.isSupportedState(Component.State.ACTIVE));
    assertFalse(
        'Control must no longer be in the ACTIVE state',
        control.hasState(Component.State.ACTIVE));

    control.render(sandbox);

    control.setSupportedState(Component.State.FOCUSED, true);
    control.setState(Component.State.FOCUSED, true);

    assertThrows(
        'Must not be able to disable support for the FOCUSED ' +
            'state for a control that\'s already in the document and focused',
        () => {
          control.setSupportedState(Component.State.FOCUSED, false);
        });

    assertTrue('No events must have been dispatched', noEventsDispatched());
  },

  /** Tests {@link Control#isAutoState}. */
  testIsAutoState() {
    assertTrue(
        'Control must have DISABLED as an auto-state',
        control.isAutoState(Component.State.DISABLED));
    assertTrue(
        'Control must have HOVER as an auto-state',
        control.isAutoState(Component.State.HOVER));
    assertTrue(
        'Control must have ACTIVE as an auto-state',
        control.isAutoState(Component.State.ACTIVE));
    assertTrue(
        'Control must have FOCUSED as an auto-state',
        control.isAutoState(Component.State.FOCUSED));

    assertFalse(
        'Control must not have SELECTED as an auto-state',
        control.isAutoState(Component.State.SELECTED));
    assertFalse(
        'Control must not have CHECKED as an auto-state',
        control.isAutoState(Component.State.CHECKED));
    assertFalse(
        'Control must not have OPENED as an auto-state',
        control.isAutoState(Component.State.OPENED));

    assertTrue('No events must have been dispatched', noEventsDispatched());
  },

  /** Tests {@link Control#setAutoStates}. */
  testSetAutoStates() {
    control.setAutoStates(Component.State.HOVER, false);
    assertFalse(
        'Control must not have HOVER as an auto-state',
        control.isAutoState(Component.State.HOVER));

    control.setAutoStates(
        Component.State.ACTIVE | Component.State.FOCUSED, false);
    assertFalse(
        'Control must not have ACTIVE as an auto-state',
        control.isAutoState(Component.State.ACTIVE));
    assertFalse(
        'Control must not have FOCUSED as an auto-state',
        control.isAutoState(Component.State.FOCUSED));

    control.setSupportedState(Component.State.FOCUSED, false);
    control.setAutoStates(Component.State.FOCUSED, true);
    assertFalse(
        'Control must not have FOCUSED as an auto-state if it no ' +
            'longer supports FOCUSED',
        control.isAutoState(Component.State.FOCUSED));

    assertTrue('No events must have been dispatched', noEventsDispatched());
  },

  /** Tests {@link Control#isDispatchTransitionEvents}. */
  testIsDispatchTransitionEvents() {
    assertTrue(
        'Control must dispatch DISABLED transition events',
        control.isDispatchTransitionEvents(Component.State.DISABLED));
    assertTrue(
        'Control must dispatch HOVER transition events',
        control.isDispatchTransitionEvents(Component.State.HOVER));
    assertTrue(
        'Control must dispatch ACTIVE transition events',
        control.isDispatchTransitionEvents(Component.State.ACTIVE));
    assertTrue(
        'Control must dispatch FOCUSED transition events',
        control.isDispatchTransitionEvents(Component.State.FOCUSED));

    assertFalse(
        'Control must not dispatch SELECTED transition events',
        control.isDispatchTransitionEvents(Component.State.SELECTED));
    assertFalse(
        'Control must not dispatch CHECKED transition events',
        control.isDispatchTransitionEvents(Component.State.CHECKED));
    assertFalse(
        'Control must not dispatch OPENED transition events',
        control.isDispatchTransitionEvents(Component.State.OPENED));

    assertTrue('No events must have been dispatched', noEventsDispatched());
  },

  /** Tests {@link Control#setDispatchTransitionEvents}. */
  testSetDispatchTransitionEvents() {
    control.setDispatchTransitionEvents(Component.State.HOVER, false);
    assertFalse(
        'Control must not dispatch HOVER transition events',
        control.isDispatchTransitionEvents(Component.State.HOVER));

    control.setSupportedState(Component.State.SELECTED, true);
    control.setDispatchTransitionEvents(Component.State.SELECTED, true);
    assertTrue(
        'Control must dispatch SELECTED transition events',
        control.isDispatchTransitionEvents(Component.State.SELECTED));

    assertTrue('No events must have been dispatched', noEventsDispatched());
  },

  /**
   * Tests {@link Control#isTransitionAllowed}.
   * @suppress {visibility} suppression added to enable type checking
   */
  testIsTransitionAllowed() {
    assertTrue(
        'Control must support the HOVER state',
        control.isSupportedState(Component.State.HOVER));
    assertFalse(
        'Control must not be in the HOVER state',
        control.hasState(Component.State.HOVER));
    assertTrue(
        'Control must dispatch HOVER transition events',
        control.isDispatchTransitionEvents(Component.State.HOVER));

    assertTrue(
        'Control must be allowed to transition to the HOVER state',
        control.isTransitionAllowed(Component.State.HOVER, true));
    assertEquals(
        'Control must have dispatched one HIGHLIGHT event', 1,
        getEventCount(control, Component.EventType.HIGHLIGHT));
    assertFalse(
        'Control must not be highlighted',
        control.hasState(Component.State.HOVER));

    control.setState(Component.State.HOVER, true);
    control.setDispatchTransitionEvents(Component.State.HOVER, false);

    assertTrue(
        'Control must be allowed to transition from the HOVER state',
        control.isTransitionAllowed(Component.State.HOVER, false));
    assertEquals(
        'Control must not have dispatched any UNHIGHLIGHT events', 0,
        getEventCount(control, Component.EventType.UNHIGHLIGHT));
    assertTrue(
        'Control must still be highlighted',
        control.hasState(Component.State.HOVER));

    control.setSupportedState(Component.State.FOCUSED, false);
    resetEventCount();

    assertFalse(
        'Control doesn\'t support the FOCUSED state',
        control.isSupportedState(Component.State.FOCUSED));
    assertFalse(
        'Control must not be FOCUSED',
        control.hasState(Component.State.FOCUSED));
    assertFalse(
        'Control must not be allowed to transition to the FOCUSED ' +
            'state',
        control.isTransitionAllowed(Component.State.FOCUSED, true));
    assertEquals(
        'Control must not have dispatched any FOCUS events', 0,
        getEventCount(control, Component.EventType.FOCUS));

    control.setEnabled(false);
    resetEventCount();

    assertTrue(
        'Control must support the DISABLED state',
        control.isSupportedState(Component.State.DISABLED));
    assertTrue(
        'Control must be DISABLED', control.hasState(Component.State.DISABLED));
    assertFalse(
        'Control must not be allowed to transition to the DISABLED ' +
            'state, because it is already there',
        control.isTransitionAllowed(Component.State.DISABLED, true));
    assertEquals(
        'Control must not have dispatched any ENABLE events', 0,
        getEventCount(control, Component.EventType.ENABLE));
  },

  /** Tests {@link Control#handleKeyEvent}. */
  testHandleKeyEvent() {
    control.render();
    control.isVisible = control.isEnabled = () => true;

    testingEvents.fireKeySequence(control.getKeyEventTarget(), KeyCodes.A);

    assertEquals(
        'Control must not have dispatched an ACTION event', 0,
        getEventCount(control, Component.EventType.ACTION));

    testingEvents.fireKeySequence(control.getKeyEventTarget(), KeyCodes.ENTER);
    assertEquals(
        'Control must have dispatched an ACTION event', 1,
        getEventCount(control, Component.EventType.ACTION));
  },

  /**
   * Tests {@link Control#performActionInternal}.
   * @suppress {visibility} suppression added to enable type checking
   */
  testPerformActionInternal() {
    assertFalse('Control must not be checked', control.isChecked());
    assertFalse('Control must not be selected', control.isSelected());
    assertFalse('Control must not be open', control.isOpen());

    control.performActionInternal();

    assertFalse('Control must not be checked', control.isChecked());
    assertFalse('Control must not be selected', control.isSelected());
    assertFalse('Control must not be open', control.isOpen());
    assertEquals(
        'Control must have dispatched an ACTION event', 1,
        getEventCount(control, Component.EventType.ACTION));

    control.setSupportedState(Component.State.CHECKED, true);
    control.setSupportedState(Component.State.SELECTED, true);
    control.setSupportedState(Component.State.OPENED, true);

    control.performActionInternal();

    assertTrue('Control must be checked', control.isChecked());
    assertTrue('Control must be selected', control.isSelected());
    assertTrue('Control must be open', control.isOpen());
    assertEquals(
        'Control must have dispatched a CHECK event', 1,
        getEventCount(control, Component.EventType.CHECK));
    assertEquals(
        'Control must have dispatched a SELECT event', 1,
        getEventCount(control, Component.EventType.SELECT));
    assertEquals(
        'Control must have dispatched a OPEN event', 1,
        getEventCount(control, Component.EventType.OPEN));
    assertEquals(
        'Control must have dispatched another ACTION event', 2,
        getEventCount(control, Component.EventType.ACTION));

    control.performActionInternal();

    assertFalse('Control must not be checked', control.isChecked());
    assertTrue('Control must be selected', control.isSelected());
    assertFalse('Control must not be open', control.isOpen());
    assertEquals(
        'Control must have dispatched an UNCHECK event', 1,
        getEventCount(control, Component.EventType.UNCHECK));
    assertEquals(
        'Control must not have dispatched an UNSELECT event', 0,
        getEventCount(control, Component.EventType.UNSELECT));
    assertEquals(
        'Control must have dispatched a CLOSE event', 1,
        getEventCount(control, Component.EventType.CLOSE));
    assertEquals(
        'Control must have dispatched another ACTION event', 3,
        getEventCount(control, Component.EventType.ACTION));
  },

  /** Tests {@link Control#handleMouseOver}. */
  testHandleMouseOver() {
    control.setContent(dom.createDom(TagName.SPAN, {id: 'caption'}, 'Hello'));
    control.render(sandbox);

    const element = control.getElement();
    const caption = dom.getElement('caption');

    // Verify baseline assumptions.
    assertTrue(
        'Caption must be contained within the control',
        dom.contains(element, caption));
    assertTrue('Control must be enabled', control.isEnabled());
    assertTrue(
        'HOVER must be an auto-state',
        control.isAutoState(Component.State.HOVER));
    assertFalse(
        'Control must not start out highlighted', control.isHighlighted());

    // Scenario 1:  relatedTarget is contained within the control's DOM.
    testingEvents.fireMouseOverEvent(element, caption);
    assertTrue(
        'No events must have been dispatched for internal mouse move',
        noEventsDispatched());
    assertFalse(
        'Control must not be highlighted for internal mouse move',
        control.isHighlighted());
    resetEventCount();

    // Scenario 2:  preventDefault() is called on the ENTER event.
    const key = googEvents.listen(control, Component.EventType.ENTER, (e) => {
      e.preventDefault();
    });
    testingEvents.fireMouseOverEvent(element, sandbox);
    assertEquals(
        'Control must have dispatched 1 ENTER event', 1,
        getEventCount(control, Component.EventType.ENTER));
    assertFalse(
        'Control must not be highlighted if ENTER is canceled',
        control.isHighlighted());
    googEvents.unlistenByKey(key);
    resetEventCount();

    // Scenario 3:  Control is disabled.
    control.setEnabled(false);
    testingEvents.fireMouseOverEvent(element, sandbox);
    assertEquals(
        'Control must dispatch ENTER event on mouseover even if ' +
            'disabled',
        1, getEventCount(control, Component.EventType.ENTER));
    assertFalse(
        'Control must not be highlighted if it is disabled',
        control.isHighlighted());
    control.setEnabled(true);
    resetEventCount();

    // Scenario 4:  HOVER is not an auto-state.
    control.setAutoStates(Component.State.HOVER, false);
    testingEvents.fireMouseOverEvent(element, sandbox);
    assertEquals(
        'Control must dispatch ENTER event on mouseover even if ' +
            'HOVER is not an auto-state',
        1, getEventCount(control, Component.EventType.ENTER));
    assertFalse(
        'Control must not be highlighted if HOVER isn\'t an auto-' +
            'state',
        control.isHighlighted());
    control.setAutoStates(Component.State.HOVER, true);
    resetEventCount();

    // Scenario 5:  All is well.
    testingEvents.fireMouseOverEvent(element, sandbox);
    assertEquals(
        'Control must dispatch ENTER event on mouseover', 1,
        getEventCount(control, Component.EventType.ENTER));
    assertEquals(
        'Control must dispatch HIGHLIGHT event on mouseover', 1,
        getEventCount(control, Component.EventType.HIGHLIGHT));
    assertTrue('Control must be highlighted', control.isHighlighted());
    resetEventCount();

    // Scenario 6: relatedTarget is null
    control.setHighlighted(false);
    testingEvents.fireMouseOverEvent(element, null);
    assertEquals(
        'Control must dispatch ENTER event on mouseover', 1,
        getEventCount(control, Component.EventType.ENTER));
    assertEquals(
        'Control must dispatch HIGHLIGHT event on mouseover', 1,
        getEventCount(control, Component.EventType.HIGHLIGHT));
    assertTrue('Control must be highlighted', control.isHighlighted());
    resetEventCount();
  },

  /** Tests {@link Control#handleMouseOut}. */
  testHandleMouseOut() {
    control.setContent(dom.createDom(TagName.SPAN, {id: 'caption'}, 'Hello'));
    control.setHighlighted(true);
    control.setActive(true);

    resetEventCount();

    control.render(sandbox);

    const element = control.getElement();
    const caption = dom.getElement('caption');

    // Verify baseline assumptions.
    assertTrue(
        'Caption must be contained within the control',
        dom.contains(element, caption));
    assertTrue('Control must be enabled', control.isEnabled());
    assertTrue(
        'HOVER must be an auto-state',
        control.isAutoState(Component.State.HOVER));
    assertTrue(
        'ACTIVE must be an auto-state',
        control.isAutoState(Component.State.ACTIVE));
    assertTrue('Control must start out highlighted', control.isHighlighted());
    assertTrue('Control must start out active', control.isActive());

    // Scenario 1:  relatedTarget is contained within the control's DOM.
    testingEvents.fireMouseOutEvent(element, caption);
    assertTrue(
        'No events must have been dispatched for internal mouse move',
        noEventsDispatched());
    assertTrue(
        'Control must not be un-highlighted for internal mouse move',
        control.isHighlighted());
    assertTrue(
        'Control must not be deactivated for internal mouse move',
        control.isActive());
    resetEventCount();

    // Scenario 2:  preventDefault() is called on the LEAVE event.
    const key = googEvents.listen(control, Component.EventType.LEAVE, (e) => {
      e.preventDefault();
    });
    testingEvents.fireMouseOutEvent(element, sandbox);
    assertEquals(
        'Control must have dispatched 1 LEAVE event', 1,
        getEventCount(control, Component.EventType.LEAVE));
    assertTrue(
        'Control must not be un-highlighted if LEAVE is canceled',
        control.isHighlighted());
    assertTrue(
        'Control must not be deactivated if LEAVE is canceled',
        control.isActive());
    googEvents.unlistenByKey(key);
    resetEventCount();

    // Scenario 3:  ACTIVE is not an auto-state.
    control.setAutoStates(Component.State.ACTIVE, false);
    testingEvents.fireMouseOutEvent(element, sandbox);
    assertEquals(
        'Control must dispatch LEAVE event on mouseout even if ' +
            'ACTIVE is not an auto-state',
        1, getEventCount(control, Component.EventType.LEAVE));
    assertTrue(
        'Control must not be deactivated if ACTIVE isn\'t an auto-' +
            'state',
        control.isActive());
    assertFalse(
        'Control must be un-highlighted even if ACTIVE isn\'t an ' +
            'auto-state',
        control.isHighlighted());
    control.setAutoStates(Component.State.ACTIVE, true);
    control.setHighlighted(true);
    resetEventCount();

    // Scenario 4:  HOVER is not an auto-state.
    control.setAutoStates(Component.State.HOVER, false);
    testingEvents.fireMouseOutEvent(element, sandbox);
    assertEquals(
        'Control must dispatch LEAVE event on mouseout even if ' +
            'HOVER is not an auto-state',
        1, getEventCount(control, Component.EventType.LEAVE));
    assertFalse(
        'Control must be deactivated even if HOVER isn\'t an auto-' +
            'state',
        control.isActive());
    assertTrue(
        'Control must not be un-highlighted if HOVER isn\'t an auto-' +
            'state',
        control.isHighlighted());
    control.setAutoStates(Component.State.HOVER, true);
    control.setActive(true);
    resetEventCount();

    // Scenario 5:  All is well.
    testingEvents.fireMouseOutEvent(element, sandbox);
    assertEquals(
        'Control must dispatch LEAVE event on mouseout', 1,
        getEventCount(control, Component.EventType.LEAVE));
    assertEquals(
        'Control must dispatch DEACTIVATE event on mouseout', 1,
        getEventCount(control, Component.EventType.DEACTIVATE));
    assertEquals(
        'Control must dispatch UNHIGHLIGHT event on mouseout', 1,
        getEventCount(control, Component.EventType.UNHIGHLIGHT));
    assertFalse('Control must be deactivated', control.isActive());
    assertFalse('Control must be unhighlighted', control.isHighlighted());
    resetEventCount();

    // Scenario 6: relatedTarget is null
    control.setActive(true);
    control.setHighlighted(true);
    testingEvents.fireMouseOutEvent(element, null);
    assertEquals(
        'Control must dispatch LEAVE event on mouseout', 1,
        getEventCount(control, Component.EventType.LEAVE));
    assertEquals(
        'Control must dispatch DEACTIVATE event on mouseout', 1,
        getEventCount(control, Component.EventType.DEACTIVATE));
    assertEquals(
        'Control must dispatch UNHIGHLIGHT event on mouseout', 1,
        getEventCount(control, Component.EventType.UNHIGHLIGHT));
    assertFalse('Control must be deactivated', control.isActive());
    assertFalse('Control must be unhighlighted', control.isHighlighted());
    resetEventCount();
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testIsMouseEventWithinElement() {
    const child = dom.createElement(TagName.DIV);
    const parent = dom.createDom(TagName.DIV, null, child);
    const notChild = dom.createElement(TagName.DIV);

    let event = new GoogTestingEvent('mouseout');
    event.relatedTarget = child;
    assertTrue(
        'Event is within element',
        Control.isMouseEventWithinElement_(event, parent));

    event = new GoogTestingEvent('mouseout');
    event.relatedTarget = notChild;
    assertFalse(
        'Event is not within element',
        Control.isMouseEventWithinElement_(event, parent));
  },

  testHandleMouseDown() {
    control.render(sandbox);
    assertFalse(
        'preventDefault() must have been called for control that ' +
            'doesn\'t support text selection',
        fireMouseDownAndFocus(control.getElement()));
    assertTrue('Control must be highlighted', control.isHighlighted());
    assertTrue('Control must be active', control.isActive());

    if (testFocus) {
      // Expected to fail on IE and Mac Safari 3.  IE calls focus handlers
      // asynchronously, and Mac Safari 3 doesn't support keyboard focus.
      expectedFailures.expectFailureFor(userAgent.IE);
      try {
        assertTrue('Control must be focused', control.isFocused());
      } catch (e) {
        expectedFailures.handleException(e);
      }
    }
  },

  testHandleMouseDownForDisabledControl() {
    control.setEnabled(false);
    control.render(sandbox);
    assertFalse(
        'preventDefault() must have been called for control that ' +
            'doesn\'t support text selection',
        fireMouseDownAndFocus(control.getElement()));
    assertFalse('Control must not be highlighted', control.isHighlighted());
    assertFalse('Control must not be active', control.isActive());
    if (testFocus) {
      assertFalse('Control must not be focused', control.isFocused());
    }
  },

  testHandleMouseDownForNoHoverAutoState() {
    control.setAutoStates(Component.State.HOVER, false);
    control.render(sandbox);
    assertFalse(
        'preventDefault() must have been called for control that ' +
            'doesn\'t support text selection',
        fireMouseDownAndFocus(control.getElement()));
    assertFalse('Control must not be highlighted', control.isHighlighted());
    assertTrue('Control must be active', control.isActive());

    if (testFocus) {
      // Expected to fail on IE and Mac Safari 3.  IE calls focus handlers
      // asynchronously, and Mac Safari 3 doesn't support keyboard focus.
      expectedFailures.expectFailureFor(userAgent.IE);
      try {
        assertTrue('Control must be focused', control.isFocused());
      } catch (e) {
        expectedFailures.handleException(e);
      }
    }
  },

  testHandleMouseDownForRightMouseButton() {
    control.render(sandbox);
    assertTrue(
        'preventDefault() must not have been called for right ' +
            'mouse button',
        fireMouseDownAndFocus(
            control.getElement(), BrowserEvent.MouseButton.RIGHT));
    assertTrue('Control must be highlighted', control.isHighlighted());
    assertFalse('Control must not be active', control.isActive());

    if (testFocus) {
      // Expected to fail on IE and Mac Safari 3.  IE calls focus handlers
      // asynchronously, and Mac Safari 3 doesn't support keyboard focus.
      expectedFailures.expectFailureFor(userAgent.IE);
      try {
        assertTrue('Control must be focused', control.isFocused());
      } catch (e) {
        expectedFailures.handleException(e);
      }
    }
  },

  testHandleMouseDownForNoActiveAutoState() {
    control.setAutoStates(Component.State.ACTIVE, false);
    control.render(sandbox);
    assertFalse(
        'preventDefault() must have been called for control that ' +
            'doesn\'t support text selection',
        fireMouseDownAndFocus(control.getElement()));
    assertTrue('Control must be highlighted', control.isHighlighted());
    assertFalse('Control must not be active', control.isActive());

    if (testFocus) {
      // Expected to fail on IE and Mac Safari 3.  IE calls focus handlers
      // asynchronously, and Mac Safari 3 doesn't support keyboard focus.
      expectedFailures.expectFailureFor(userAgent.IE);
      try {
        assertTrue('Control must be focused', control.isFocused());
      } catch (e) {
        expectedFailures.handleException(e);
      }
    }
  },

  testHandleMouseDownForNonFocusableControl() {
    control.setSupportedState(Component.State.FOCUSED, false);
    control.render(sandbox);
    assertFalse(
        'preventDefault() must have been called for control that ' +
            'doesn\'t support text selection',
        fireMouseDownAndFocus(control.getElement()));
    assertTrue('Control must be highlighted', control.isHighlighted());
    assertTrue('Control must be active', control.isActive());
    assertFalse('Control must not be focused', control.isFocused());
  },

  // TODO(attila): Find out why this is flaky on FF2/Linux and FF1.5/Win.
  // function testHandleMouseDownForSelectableControl() {
  //  control.setAllowTextSelection(true);
  //  control.render(sandbox);
  //  assertTrue('preventDefault() must not have been called for control ' +
  //      'that supports text selection',
  //      fireMouseDownAndFocus(control.getElement()));
  //  assertTrue('Control must be highlighted', control.isHighlighted());
  //  assertTrue('Control must be active', control.isActive());
  //  // Expected to fail on IE and Mac Safari 3.  IE calls focus handlers
  //  // asynchronously, and Mac Safari 3 doesn't support keyboard focus.
  //  expectedFailures.expectFailureFor(goog.userAgent.IE);
  //  try {
  //    assertTrue('Control must be focused', control.isFocused());
  //  } catch (e) {
  //    expectedFailures.handleException(e);
  //  }
  //}
  /** Tests {@link Control#handleMouseUp}. */
  testHandleMouseUp() {
    control.setActive(true);

    // Override performActionInternal() for testing purposes.
    let actionPerformed = false;
    /** @suppress {visibility} suppression added to enable type checking */
    control.performActionInternal = () => {
      actionPerformed = true;
      return true;
    };

    resetEventCount();

    control.render(sandbox);
    const element = control.getElement();

    // Verify baseline assumptions.
    assertTrue('Control must be enabled', control.isEnabled());
    assertTrue(
        'HOVER must be an auto-state',
        control.isAutoState(Component.State.HOVER));
    assertTrue(
        'ACTIVE must be an auto-state',
        control.isAutoState(Component.State.ACTIVE));
    assertFalse(
        'Control must not start out highlighted', control.isHighlighted());
    assertTrue('Control must start out active', control.isActive());

    // Scenario 1:  Control is disabled.
    control.setEnabled(false);
    testingEvents.fireMouseUpEvent(element);
    assertFalse(
        'Disabled control must not highlight on mouseup',
        control.isHighlighted());
    assertFalse('No action must have been performed', actionPerformed);
    control.setActive(true);
    control.setEnabled(true);

    // Scenario 2:  HOVER is not an auto-state.
    control.setAutoStates(Component.State.HOVER, false);
    testingEvents.fireMouseUpEvent(element);
    assertFalse(
        'Control must not highlight on mouseup if HOVER isn\'t an ' +
            'auto-state',
        control.isHighlighted());
    assertTrue(
        'Action must have been performed even if HOVER isn\'t an ' +
            'auto-state',
        actionPerformed);
    assertFalse(
        'Control must have been deactivated on mouseup even if ' +
            'HOVER isn\'t an auto-state',
        control.isActive());
    actionPerformed = false;
    control.setActive(true);
    control.setAutoStates(Component.State.HOVER, true);

    // Scenario 3:  Control is not active.
    control.setActive(false);
    testingEvents.fireMouseUpEvent(element);
    assertTrue(
        'Control must highlight on mouseup, even if inactive',
        control.isHighlighted());
    assertFalse(
        'No action must have been performed if control is inactive',
        actionPerformed);
    assertFalse(
        'Inactive control must remain inactive after mouseup',
        control.isActive());
    control.setHighlighted(false);
    control.setActive(true);

    // Scenario 4:  performActionInternal() returns false.
    /**
     * @suppress {visibility,duplicate} suppression added to enable type
     * checking
     */
    control.performActionInternal = () => {
      actionPerformed = true;
      return false;
    };
    testingEvents.fireMouseUpEvent(element);
    assertTrue(
        'Control must highlight on mouseup, even if no action is ' +
            'performed',
        control.isHighlighted());
    assertTrue('performActionInternal must have been called', actionPerformed);
    assertTrue(
        'Control must not deactivate if performActionInternal ' +
            'returns false',
        control.isActive());
    control.setHighlighted(false);
    actionPerformed = false;
    /**
     * @suppress {visibility,duplicate} suppression added to enable type
     * checking
     */
    control.performActionInternal = () => {
      actionPerformed = true;
      return true;
    };

    // Scenario 5:  ACTIVE is not an auto-state.
    control.setAutoStates(Component.State.ACTIVE, false);
    testingEvents.fireMouseUpEvent(element);
    assertTrue(
        'Control must highlight on mouseup even if ACTIVE isn\'t an ' +
            'auto-state',
        control.isHighlighted());
    assertTrue(
        'Action must have been performed even if ACTIVE isn\'t an ' +
            'auto-state',
        actionPerformed);
    assertTrue(
        'Control must not have been deactivated on mouseup if ' +
            'ACTIVE isn\'t an auto-state',
        control.isActive());
    actionPerformed = false;
    control.setHighlighted(false);
    control.setAutoStates(Component.State.ACTIVE, true);

    // Scenario 6:  All is well.
    testingEvents.fireMouseUpEvent(element);
    assertTrue('Control must highlight on mouseup', control.isHighlighted());
    assertTrue('Action must have been performed', actionPerformed);
    assertFalse('Control must have been deactivated', control.isActive());
  },

  testDefaultConstructor() {
    const control = new Control();
    assertNull(control.getContent());
  },

  testIeMouseEventSequenceSimulator() {
    control.render(sandbox);

    // Click sequences and isolated clicks must be handled correctly in any
    // order.
    assertClickSequenceFires('ACTION event expected after a click sequence');
    assertClickSequenceFires(
        'ACTION event expected after a second consecutive click sequence');
    if (userAgent.IE) {
      // For some reason in IE8 and perhaps earlier, isolated clicks do not
      // result a detectable dispatch of an ACTION event, so we'll only assert
      // the desired handling of isolated clicks in IE9 and higher.
      if (userAgent.isVersionOrHigher(9)) {
        assertIsolatedClickFires(
            'ACTION event expected after an isolated click immediately ' +
            'following a click sequence');
        assertIsolatedClickFires(
            'ACTION event expected after second consecutive isolated click');
      } else {
        // For IE8-and-lower, fire an isolated click event in preparation for
        // our final assertion.
        testingEvents.fireClickEvent(control.getKeyEventTarget());
      }
    } else {
      assertIsolatedClickDoesNotFire(
          'No ACTION event expected after an isolated click immediately ' +
          'following a click sequence');
      assertIsolatedClickDoesNotFire(
          'No ACTION event expected after second consecutive isolated click');
    }
    assertClickSequenceFires(
        'ACTION event expected after click sequence immediately following ' +
        'an isolated click ');
  },

  testIeMouseEventSequenceSimulator_withPointerEvents() {
    control.setPointerEventsEnabled(true);
    control.render(sandbox);

    if (userAgent.IE) {
      // For some reason in IE8 and perhaps earlier, isolated clicks do not
      // result a detectable dispatch of an ACTION event, so we'll only assert
      // the desired handling of isolated clicks in IE9 and higher.
      if (userAgent.isVersionOrHigher(9)) {
        assertIsolatedClickFires(
            'ACTION event expected after an isolated click immediately ' +
            'following a click sequence');
        assertIsolatedClickFires(
            'ACTION event expected after second consecutive isolated click');
      } else {
        // For IE8-and-lower, fire an isolated click event in preparation for
        // our final assertion.
        testingEvents.fireClickEvent(control.getKeyEventTarget());
      }
    } else {
      assertIsolatedClickDoesNotFire(
          'No ACTION event expected after an isolated click immediately ' +
          'following a click sequence');
      assertIsolatedClickDoesNotFire(
          'No ACTION event expected after second consecutive isolated click');
    }
  },

  testIeMouseEventSequenceSimulatorStrictMode() {
    if (!document.createEvent) {
      return;
    }

    control.render(sandbox);

    const actionCount = getEventCount(control, Component.EventType.ACTION);
    const e = document.createEvent('MouseEvents');
    e.initMouseEvent(
        'click', true, true, window, 0, 0, 0, 0, 0, false, false, false, false,
        0, null);
    control.getElementStrict().dispatchEvent(e);
    if (userAgent.IE) {
      assertEquals(
          'ACTION event expected after an isolated click', actionCount + 1,
          getEventCount(control, Component.EventType.ACTION));
    } else {
      assertEquals(
          'No ACTION event expected after an isolated click', actionCount,
          getEventCount(control, Component.EventType.ACTION));
    }
  },

  testSetPointerEventsEnabled() {
    control.setPointerEventsEnabled(true);
    control.render(sandbox);

    assertFalse(
        'Control should not be active before pointerdown event.',
        control.isActive());

    const pointerdown = new GoogTestingEvent(
        PointerFallbackEventType.POINTERDOWN, control.getElement());
    pointerdown.button = BrowserEvent.MouseButton.LEFT;
    testingEvents.fireBrowserEvent(pointerdown);

    assertTrue(
        'Control should be active after pointerdown event.',
        control.isActive());

    const pointerup = new GoogTestingEvent(
        PointerFallbackEventType.POINTERUP, control.getElement());
    pointerup.button = BrowserEvent.MouseButton.LEFT;
    testingEvents.fireBrowserEvent(pointerup);

    assertFalse(
        'Control should not be active after pointerup event.',
        control.isActive());
  },
});
