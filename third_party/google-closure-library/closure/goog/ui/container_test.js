/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ContainerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const Component = goog.require('goog.ui.Component');
const Container = goog.require('goog.ui.Container');
const Control = goog.require('goog.ui.Control');
const GoogEvent = goog.require('goog.events.Event');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const KeyEvent = goog.require('goog.events.KeyEvent');
const PointerFallbackEventType = goog.require('goog.events.PointerFallbackEventType');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dispose = goog.require('goog.dispose');
const dom = goog.require('goog.dom');
const googEvents = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let sandbox;
let containerElement;
let container;
let keyContainer;
let listContainer;

/**
 * Test container to which the elements have to be added with
 * {@code container.addChild(element, false)}
 */
class ListContainer extends Container {
  constructor() {
    super();
    Container.call(this);
  }

  /**
   * @override
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  createDom() {
    ListContainer.superClass_.createDom.call(this);
    const ul = this.getDomHelper().createDom(TagName.UL);
    this.forEachChild(function(child) {
      child.createDom();
      const childEl = child.getElement();
      ul.appendChild(this.getDomHelper().createDom(TagName.LI, {}, childEl));
    }, this);
    this.getContentElement().appendChild(ul);
  }
}

/** Test container for tracking key events being handled. */
class KeyHandlingContainer extends Container {
  constructor() {
    super();
    Container.call(this);
    this.keyEventsHandled = 0;
  }

  /** @override */
  handleKeyEventInternal() {
    this.keyEventsHandled++;
    return false;
  }
}

/**
 * Checks that getHighlighted() returns the expected value and checks
 * that the child at this index is highlighted and other children are not.
 * @param {string} explanation Message indicating what is expected.
 * @param {number} index Expected return value of getHighlightedIndex().
 */
function assertHighlightedIndex(explanation, index) {
  assertEquals(explanation, index, container.getHighlightedIndex());
  for (let i = 0; i < container.getChildCount(); i++) {
    if (i == index) {
      assertTrue(
          'Child at highlighted index should be highlighted',
          container.getChildAt(i).isHighlighted());
    } else {
      assertFalse(
          'Only child at highlighted index should be highlighted',
          container.getChildAt(i).isHighlighted());
    }
  }
}

testSuite({
  setUpPage() {
    sandbox = dom.getElement('sandbox');
  },

  setUp() {
    container = new Container();
    keyContainer = null;
    listContainer = null;

    sandbox.innerHTML = '<div id="containerElement" class="goog-container">\n' +
        '  <div class="goog-control" id="hello">Hello</div>\n' +
        '  <div class="goog-control" id="world">World</div>\n' +
        '</div>';
    containerElement = dom.getElement('containerElement');
  },

  tearDown() {
    dom.removeChildren(sandbox);
    container.dispose();
    dispose(keyContainer);
    dispose(listContainer);
  },

  testDecorateHidden() {
    containerElement.style.display = 'none';

    assertTrue('Container must be visible', container.isVisible());
    container.decorate(containerElement);
    assertFalse('Container must be hidden', container.isVisible());
    container.forEachChild((control) => {
      assertTrue(
          'Child control ' + control.getId() + ' must report being ' +
              'visible, even if in a hidden container',
          control.isVisible());
    });
  },

  testDecorateDisabled() {
    classlist.add(containerElement, 'goog-container-disabled');

    assertTrue('Container must be enabled', container.isEnabled());
    container.decorate(containerElement);
    assertFalse('Container must be disabled', container.isEnabled());
    container.forEachChild((control) => {
      assertFalse(
          'Child control ' + control.getId() + ' must be disabled, ' +
              'because the host container is disabled',
          control.isEnabled());
    });
  },

  testDecorateFocusableContainer() {
    container.decorate(containerElement);
    assertTrue('Container must be focusable', container.isFocusable());
    container.forEachChild((control) => {
      assertFalse(
          'Child control ' + control.getId() + ' must not be ' +
              'focusable',
          control.isSupportedState(Component.State.FOCUSED));
    });
  },

  testDecorateFocusableChildrenContainer() {
    container.setFocusable(false);
    container.setFocusableChildrenAllowed(true);
    container.decorate(containerElement);
    assertFalse('Container must not be focusable', container.isFocusable());
    container.forEachChild((control) => {
      assertTrue(
          'Child control ' + control.getId() + ' must be ' +
              'focusable',
          control.isSupportedState(Component.State.FOCUSED));
    });
  },

  testHighlightOnEnter() {
    // This interaction test ensures that containers enforce that children
    // get highlighted on mouseover, and that one and only one child may
    // be highlighted at a time.  Although integration tests aren't the
    // best, it's difficult to test these event-based interactions due to
    // their disposition toward the "misunderstood contract" problem.

    container.decorate(containerElement);
    assertFalse(
        'Child 0 should initially not be highlighted',
        container.getChildAt(0).isHighlighted());

    testingEvents.fireMouseOverEvent(
        container.getChildAt(0).getElement(), sandbox);
    assertTrue(
        'Child 0 should become highlighted after a mouse over',
        container.getChildAt(0).isHighlighted());
    assertEquals(
        'Child 0 should be the active descendant',
        container.getChildAt(0).getElement(),
        aria.getActiveDescendant(container.getElement()));

    testingEvents.fireMouseOverEvent(
        container.getChildAt(1).getElement(),
        container.getChildAt(0).getElement());
    assertFalse(
        'Child 0 should lose highlight when child 1 is moused ' +
            'over, even if no mouseout occurs.',
        container.getChildAt(0).isHighlighted());
    assertTrue(
        'Child 1 should now be highlighted.',
        container.getChildAt(1).isHighlighted());
    assertEquals(
        'Child 1 should be the active descendant',
        container.getChildAt(1).getElement(),
        aria.getActiveDescendant(container.getElement()));
  },

  testHighlightOnEnterPreventable() {
    container.decorate(containerElement);
    googEvents.listen(container, Component.EventType.ENTER, (event) => {
      event.preventDefault();
    });
    testingEvents.fireMouseOverEvent(
        container.getChildAt(0).getElement(), sandbox);
    assertFalse(
        'Child 0 should not be highlighted if preventDefault called',
        container.getChildAt(0).isHighlighted());
  },

  testHighlightDisabled() {
    // Another interaction test.  Already tested in control_test.
    container.decorate(containerElement);
    container.getChildAt(0).setEnabled(false);
    testingEvents.fireMouseOverEvent(
        container.getChildAt(0).getElement(), sandbox);
    assertFalse(
        'Disabled children should not be highlighted',
        container.getChildAt(0).isHighlighted());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetOwnerControl() {
    container.decorate(containerElement);

    assertEquals(
        'Must return appropriate control given an element in the ' +
            'control.',
        container.getChildAt(1),
        container.getOwnerControl(container.getChildAt(1).getElement()));

    assertNull(
        'Must return null for element not associated with control.',
        container.getOwnerControl(document.body));
    assertNull(
        'Must return null if given null node', container.getOwnerControl(null));
  },

  testShowEvent() {
    container.decorate(containerElement);
    container.setVisible(false);
    let eventFired = false;
    googEvents.listen(container, Component.EventType.SHOW, () => {
      assertFalse(
          'Container must not be visible when SHOW event is ' +
              'fired',
          container.isVisible());
      eventFired = true;
    });
    container.setVisible(true);
    assertTrue('SHOW event expected', eventFired);
  },

  testAfterShowEvent() {
    container.decorate(containerElement);
    container.setVisible(false);
    let eventFired = false;
    googEvents.listen(container, Container.EventType.AFTER_SHOW, () => {
      assertTrue(
          'Container must be visible when AFTER_SHOW event is ' +
              'fired',
          container.isVisible());
      eventFired = true;
    });
    container.setVisible(true);
    assertTrue('AFTER_SHOW event expected', eventFired);
  },

  testHideEvents() {
    const events = [];
    container.decorate(containerElement);
    container.setVisible(true);
    const eventFired = false;
    googEvents.listen(container, Component.EventType.HIDE, (e) => {
      assertTrue(
          'Container must be visible when HIDE event is fired',
          container.isVisible());
      events.push(e.type);
    });
    googEvents.listen(container, Container.EventType.AFTER_HIDE, (e) => {
      assertFalse(
          'Container must not be visible when AFTER_HIDE event is fired',
          container.isVisible());
      events.push(e.type);
    });
    container.setVisible(false);
    assertArrayEquals(
        'HIDE event followed by AFTER_HIDE expected',
        [
          Component.EventType.HIDE,
          Container.EventType.AFTER_HIDE,
        ],
        events);
  },

  testGetOwnerControlWithNoRenderingInAddChild() {
    listContainer = new ListContainer();
    const control = new Control('item');
    listContainer.addChild(control);
    listContainer.render();
    /** @suppress {visibility} suppression added to enable type checking */
    const ownerControl = listContainer.getOwnerControl(control.getElement());

    assertEquals(
        'Control was added with addChild(control, false)', control,
        ownerControl);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHandleKeyEvent_onlyHandlesWhenVisible() {
    keyContainer = new KeyHandlingContainer();
    keyContainer.decorate(containerElement);

    keyContainer.setVisible(false);
    keyContainer.handleKeyEvent(new GoogEvent());
    assertEquals(
        'No key events should be handled', 0, keyContainer.keyEventsHandled);

    keyContainer.setVisible(true);
    keyContainer.handleKeyEvent(new GoogEvent());
    assertEquals(
        'One key event should be handled', 1, keyContainer.keyEventsHandled);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHandleKeyEvent_onlyHandlesWhenEnabled() {
    keyContainer = new KeyHandlingContainer();
    keyContainer.decorate(containerElement);
    keyContainer.setVisible(true);

    keyContainer.setEnabled(false);
    keyContainer.handleKeyEvent(new GoogEvent());
    assertEquals(
        'No key events should be handled', 0, keyContainer.keyEventsHandled);

    keyContainer.setEnabled(true);
    keyContainer.handleKeyEvent(new GoogEvent());
    assertEquals(
        'One key event should be handled', 1, keyContainer.keyEventsHandled);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHandleKeyEvent_childlessContainersIgnoreKeyEvents() {
    keyContainer = new KeyHandlingContainer();
    keyContainer.render();
    keyContainer.setVisible(true);

    keyContainer.handleKeyEvent(new GoogEvent());
    assertEquals(
        'No key events should be handled', 0, keyContainer.keyEventsHandled);

    keyContainer.addChild(new Control());
    keyContainer.handleKeyEvent(new GoogEvent());
    assertEquals(
        'One key event should be handled', 1, keyContainer.keyEventsHandled);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHandleKeyEvent_alwaysHandlesWithKeyEventTarget() {
    keyContainer = new KeyHandlingContainer();
    keyContainer.render();
    keyContainer.setKeyEventTarget(dom.createDom(TagName.DIV));
    keyContainer.setVisible(true);

    keyContainer.handleKeyEvent(new GoogEvent());
    assertEquals(
        'One key events should be handled', 1, keyContainer.keyEventsHandled);
  },

  testHandleKeyEventInternal_onlyHandlesUnmodified() {
    container.setKeyEventTarget(sandbox);
    const event = new KeyEvent(KeyCodes.ESC, 0, false, null);

    const propertyNames = ['shiftKey', 'altKey', 'ctrlKey', 'metaKey'];

    // Verify that the event is not handled whenever a modifier key is true.
    let propertyName;
    for (let i = 0; propertyName = propertyNames[i]; i++) {
      assertTrue(
          'Event should be handled when modifer key is not pressed.',
          container.handleKeyEventInternal(event));
      event[propertyName] = true;
      assertFalse(
          'Event should not be handled when modifer key is pressed.',
          container.handleKeyEventInternal(event));
      event[propertyName] = false;
    }
  },

  testOpenFollowsHighlight() {
    container.decorate(containerElement);
    container.setOpenFollowsHighlight(true);
    assertTrue(
        'isOpenFollowsHighlight should return true',
        container.isOpenFollowsHighlight());

    // Make the children openable.
    container.forEachChild((child) => {
      child.setSupportedState(Component.State.OPENED, true);
    });
    // Open child 1 initially.
    container.getChildAt(1).setOpen(true);

    assertFalse(
        'Child 0 should initially not be highlighted',
        container.getChildAt(0).isHighlighted());
    testingEvents.fireMouseOverEvent(
        container.getChildAt(0).getElement(), sandbox);
    assertTrue(
        'Child 0 should become highlighted after a mouse over',
        container.getChildAt(0).isHighlighted());
    assertTrue(
        'Child 0 should become open after higlighted',
        container.getChildAt(0).isOpen());
    assertFalse(
        'Child 1 should become closed once 0 is open',
        container.getChildAt(1).isOpen());
    assertEquals(
        'OpenItem should be child 0', container.getChildAt(0),
        container.getOpenItem());
  },

  testOpenNotFollowsHighlight() {
    container.decorate(containerElement);
    container.setOpenFollowsHighlight(false);
    assertFalse(
        'isOpenFollowsHighlight should return false',
        container.isOpenFollowsHighlight());

    // Make the children openable.
    container.forEachChild((child) => {
      child.setSupportedState(Component.State.OPENED, true);
    });
    // Open child 1 initially.
    container.getChildAt(1).setOpen(true);

    assertFalse(
        'Child 0 should initially not be highlighted',
        container.getChildAt(0).isHighlighted());
    testingEvents.fireMouseOverEvent(
        container.getChildAt(0).getElement(), sandbox);
    assertTrue(
        'Child 0 should become highlighted after a mouse over',
        container.getChildAt(0).isHighlighted());
    assertFalse(
        'Child 0 should remain closed after higlighted',
        container.getChildAt(0).isOpen());
    assertTrue('Child 1 should remain open', container.getChildAt(1).isOpen());
    assertEquals(
        'OpenItem should be child 1', container.getChildAt(1),
        container.getOpenItem());
  },

  testRemoveChild() {
    dom.removeChildren(containerElement);
    container.decorate(containerElement);

    const a = new Control('A');
    const b = new Control('B');
    const c = new Control('C');

    a.setId('a');
    b.setId('b');
    c.setId('c');

    container.addChild(a, true);
    container.addChild(b, true);
    container.addChild(c, true);

    container.setHighlightedIndex(2);

    assertEquals(
        'Parent must remove and return child by ID', b,
        container.removeChild('b'));
    assertNull(
        'Parent must no longer contain this child', container.getChild('b'));
    assertEquals(
        'Highlighted index must be decreased', 1,
        container.getHighlightedIndex());
    assertTrue(
        'The removed control must handle its own mouse events',
        b.isHandleMouseEvents());

    assertEquals(
        'Parent must remove and return child', c, container.removeChild(c));
    assertNull(
        'Parent must no longer contain this child', container.getChild('c'));
    assertFalse('This child must no longer be highlighted', c.isHighlighted());
    assertTrue(
        'The removed control must handle its own mouse events',
        c.isHandleMouseEvents());

    assertEquals(
        'Parent must remove and return child by index', a,
        container.removeChildAt(0));
    assertNull(
        'Parent must no longer contain this child', container.getChild('a'));
    assertTrue(
        'The removed control must handle its own mouse events',
        a.isHandleMouseEvents());
  },

  testRemoveHighlightedDisposedChild() {
    dom.removeChildren(containerElement);
    container.decorate(containerElement);

    const a = new Control('A');
    container.addChild(a, true);

    container.setHighlightedIndex(0);
    a.dispose();
    container.removeChild(a);
    container.dispose();
  },

  testUpdateHighlightedIndex_updatesWhenChildrenAreAdded() {
    dom.removeChildren(containerElement);
    container.decorate(containerElement);

    const a = new Control('A');
    const b = new Control('B');
    const c = new Control('C');

    container.addChild(a);
    container.setHighlightedIndex(0);
    assertHighlightedIndex('Highlighted index should match set value', 0);

    // Add child before the highlighted one.
    container.addChildAt(b, 0);
    assertHighlightedIndex('Highlighted index should be increased', 1);

    // Add child after the highlighted one.
    container.addChildAt(c, 2);
    assertHighlightedIndex('Highlighted index should not change', 1);

    container.dispose();
  },

  testUpdateHighlightedIndex_updatesWhenChildrenAreMoved() {
    dom.removeChildren(containerElement);
    container.decorate(containerElement);

    const a = new Control('A');
    const b = new Control('B');
    const c = new Control('C');

    container.addChild(a);
    container.addChild(b);
    container.addChild(c);

    // Highlight 'c' and swap 'a' and 'b'
    // [a, b, c] -> [a, b, *c] -> [b, a, *c] (* indicates the highlighted child)
    container.setHighlightedIndex(2);
    container.addChildAt(a, 1, false);
    assertHighlightedIndex('Highlighted index should not change', 2);

    // Move the highlighted child 'c' from index 2 to index 1.
    // [b, a, *c] -> [b, *c, a]
    container.addChildAt(c, 1, false);
    assertHighlightedIndex('Highlighted index must follow the moved child', 1);

    // Take the element in front of the highlighted index and move it behind it.
    // [b, *c, a] -> [*c, a, b]
    container.addChildAt(b, 2, false);
    assertHighlightedIndex('Highlighted index must be decreased', 0);

    // And move the element back to the front.
    // [*c, a, b] -> [b, *c, a]
    container.addChildAt(b, 0, false);
    assertHighlightedIndex('Highlighted index must be increased', 1);

    container.dispose();
  },

  testUpdateHighlightedIndex_notChangedOnNoOp() {
    dom.removeChildren(containerElement);
    container.decorate(containerElement);

    container.addChild(new Control('A'));
    container.addChild(new Control('B'));
    container.setHighlightedIndex(1);

    // Re-add a child to its current position.
    container.addChildAt(container.getChildAt(0), 0, false);
    assertHighlightedIndex('Highlighted index must not change', 1);

    container.dispose();
  },

  testUpdateHighlightedIndex_notChangedWhenNoChildSelected() {
    dom.removeChildren(containerElement);
    container.decorate(containerElement);

    const a = new Control('A');
    const b = new Control('B');
    const c = new Control('C');
    container.addChild(a);
    container.addChild(b);
    container.addChild(c);

    // Move children around.
    container.addChildAt(a, 2, false);
    container.addChildAt(b, 1, false);
    container.addChildAt(c, 2, false);

    assertHighlightedIndex('Highlighted index must not change', -1);

    container.dispose();
  },

  testUpdateHighlightedIndex_indexStaysInBoundsWhenMovedToMaxIndex() {
    dom.removeChildren(containerElement);
    container.decorate(containerElement);

    const a = new Control('A');
    const b = new Control('B');
    container.addChild(a);
    container.addChild(b);

    // Move higlighted child to an index one behind last child.
    container.setHighlightedIndex(0);
    container.addChildAt(a, 2);

    assertEquals(
        'Child should be moved to index 1', a, container.getChildAt(1));
    assertEquals('Child count should not change', 2, container.getChildCount());
    assertHighlightedIndex('Highlighted index must point to new index', 1);

    container.dispose();
  },

  testSetPointerEventsEnabled() {
    container.setPointerEventsEnabled(true);
    container.decorate(containerElement);

    const child = container.getChildAt(0);

    assertFalse(
        'Child should not be active before pointerdown event.',
        child.isActive());

    const pointerdown = new GoogTestingEvent(
        PointerFallbackEventType.POINTERDOWN, child.getElement());
    pointerdown.button = BrowserEvent.MouseButton.LEFT;
    testingEvents.fireBrowserEvent(pointerdown);

    assertTrue(
        'Child should be active after pointerdown event.', child.isActive());

    const pointerup = new GoogTestingEvent(
        PointerFallbackEventType.POINTERUP, child.getElement());
    pointerup.button = BrowserEvent.MouseButton.LEFT;
    testingEvents.fireBrowserEvent(pointerup);

    assertFalse(
        'Child should not be active after pointerup event.', child.isActive());

    container.dispose();
  },
});
