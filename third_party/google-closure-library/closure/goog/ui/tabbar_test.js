/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.TabBarTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const Container = goog.require('goog.ui.Container');
const EventType = goog.require('goog.events.EventType');
const GoogEvent = goog.require('goog.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const TabBar = goog.require('goog.ui.TabBar');
const TabBarRenderer = goog.require('goog.ui.TabBarRenderer');
const UiTab = goog.require('goog.ui.Tab');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');

let sandbox;
let tabBar;

// Fake keyboard event object.
class FakeKeyEvent {
  constructor(keyCode) {
    this.keyCode = keyCode;
    this.defaultPrevented = false;
    this.propagationStopped = false;
  }

  preventDefault() {
    this.defaultPrevented = true;
  }

  stopPropagation() {
    this.propagationStopped = true;
  }
}

/** @suppress {visibility} suppression added to enable type checking */
function setHighlightedIndexFromKeyEvent() {
  let bar;
  let baz;
  let foo;

  // Create a tab bar with some tabs.
  tabBar.addChild(foo = new UiTab('foo'));
  tabBar.addChild(bar = new UiTab('bar'));
  tabBar.addChild(baz = new UiTab('baz'));

  // Verify baseline assumptions.
  assertNull('No tab must be highlighted', tabBar.getHighlighted());
  assertNull('No tab must be selected', tabBar.getSelectedTab());
  assertTrue(
      'Tab bar must auto-select tabs on keyboard highlight',
      tabBar.isAutoSelectTabs());

  // Highlight and selection must move together.
  tabBar.setHighlightedIndexFromKeyEvent(0);
  assertTrue('Foo must be highlighted', foo.isHighlighted());
  assertTrue('Foo must be selected', foo.isSelected());

  // Highlight and selection must move together.
  tabBar.setHighlightedIndexFromKeyEvent(1);
  assertFalse('Foo must no longer be highlighted', foo.isHighlighted());
  assertFalse('Foo must no longer be selected', foo.isSelected());
  assertTrue('Bar must be highlighted', bar.isHighlighted());
  assertTrue('Bar must be selected', bar.isSelected());

  // Turn off auto-select-on-keyboard-highlight.
  tabBar.setAutoSelectTabs(false);

  // Selection must not change; only highlight should move.
  tabBar.setHighlightedIndexFromKeyEvent(2);
  assertFalse('Bar must no longer be highlighted', bar.isHighlighted());
  assertTrue('Bar must remain selected', bar.isSelected());
  assertTrue('Baz must be highlighted', baz.isHighlighted());
  assertFalse('Baz must not be selected', baz.isSelected());
}

testSuite({
  setUp() {
    sandbox = dom.getElement('sandbox');
    tabBar = new TabBar();
  },

  tearDown() {
    tabBar.dispose();
    dom.removeChildren(sandbox);
  },

  testConstructor() {
    assertNotNull('Tab bar must not be null', tabBar);
    assertEquals(
        'Tab bar renderer must default to expected value',
        TabBarRenderer.getInstance(), tabBar.getRenderer());
    assertEquals(
        'Tab bar location must default to expected value', TabBar.Location.TOP,
        tabBar.getLocation());
    assertEquals(
        'Tab bar orientation must default to expected value',
        Container.Orientation.HORIZONTAL, tabBar.getOrientation());

    const fakeRenderer = {};
    const fakeDomHelper = {};
    /** @suppress {checkTypes} suppression added to enable type checking */
    const bar = new TabBar(TabBar.Location.START, fakeRenderer, fakeDomHelper);
    assertNotNull('Tab bar must not be null', bar);
    assertEquals(
        'Tab bar renderer must have expected value', fakeRenderer,
        bar.getRenderer());
    assertEquals(
        'Tab bar DOM helper must have expected value', fakeDomHelper,
        bar.getDomHelper());
    assertEquals(
        'Tab bar location must have expected value', TabBar.Location.START,
        bar.getLocation());
    assertEquals(
        'Tab bar orientation must have expected value',
        Container.Orientation.VERTICAL, bar.getOrientation());
    bar.dispose();
  },

  testDispose() {
    // Set tabBar.selectedTab_ to something non-null, just to test dispose().
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    tabBar.selectedTab_ = {};
    assertNotNull('Selected tab must be non-null', tabBar.getSelectedTab());
    assertFalse('Tab bar must not have been disposed of', tabBar.isDisposed());
    tabBar.dispose();
    assertNull('Selected tab must be null', tabBar.getSelectedTab());
    assertTrue('Tab bar must have been disposed of', tabBar.isDisposed());
  },

  testAddRemoveChild() {
    assertNull('No tab must be selected', tabBar.getSelectedTab());

    const first = new UiTab('First');
    tabBar.addChild(first);
    assertEquals(
        'First tab must have been added at the expected index', 0,
        tabBar.indexOfChild(first));
    first.setSelected(true);
    assertEquals('First tab must be selected', 0, tabBar.getSelectedTabIndex());

    const second = new UiTab('Second');
    tabBar.addChild(second);
    assertEquals(
        'Second tab must have been added at the expected index', 1,
        tabBar.indexOfChild(second));
    assertEquals(
        'First tab must remain selected', 0, tabBar.getSelectedTabIndex());

    const firstRemoved = tabBar.removeChild(first);
    assertEquals(
        'removeChild() must return the removed tab', first, firstRemoved);
    assertEquals(
        'First tab must no longer be in the tab bar', -1,
        tabBar.indexOfChild(first));
    assertEquals(
        'Second tab must be at the expected index', 0,
        tabBar.indexOfChild(second));
    assertFalse('First tab must no longer be selected', first.isSelected());
    assertTrue('Remaining tab must be selected', second.isSelected());

    const secondRemoved = tabBar.removeChild(second);
    assertEquals(
        'removeChild() must return the removed tab', second, secondRemoved);
    assertFalse('Tab must no longer be selected', second.isSelected());
    assertNull('No tab must be selected', tabBar.getSelectedTab());
  },

  testGetSetLocation() {
    assertEquals(
        'Location must default to TOP', TabBar.Location.TOP,
        tabBar.getLocation());
    tabBar.setLocation(TabBar.Location.START);
    assertEquals(
        'Location must have expected value', TabBar.Location.START,
        tabBar.getLocation());
    tabBar.createDom();
    assertThrows(
        'Attempting to change the location after the tab bar has ' +
            'been rendered must throw error',
        () => {
          tabBar.setLocation(TabBar.Location.BOTTOM);
        });
  },

  testIsSetAutoSelectTabs() {
    assertTrue(
        'Tab bar must auto-select tabs by default', tabBar.isAutoSelectTabs());
    tabBar.setAutoSelectTabs(false);
    assertFalse(
        'Tab bar must no longer auto-select tabs by default',
        tabBar.isAutoSelectTabs());
    tabBar.render(sandbox);
    assertFalse(
        'Rendering must not change auto-select setting',
        tabBar.isAutoSelectTabs());
    tabBar.setAutoSelectTabs(true);
    assertTrue(
        'Tab bar must once again auto-select tabs', tabBar.isAutoSelectTabs());
  },

  testGetSetSelectedTab() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChild(foo = new UiTab('foo'));
    tabBar.addChild(bar = new UiTab('bar'));
    tabBar.addChild(baz = new UiTab('baz'));

    assertNull('No tab must be selected', tabBar.getSelectedTab());

    tabBar.setSelectedTab(baz);
    assertTrue('Baz must be selected', baz.isSelected());
    assertEquals('Baz must be the selected tab', baz, tabBar.getSelectedTab());

    tabBar.setSelectedTab(foo);
    assertFalse('Baz must no longer be selected', baz.isSelected());
    assertTrue('Foo must be selected', foo.isSelected());
    assertEquals('Foo must be the selected tab', foo, tabBar.getSelectedTab());

    tabBar.setSelectedTab(foo);
    assertTrue('Foo must remain selected', foo.isSelected());
    assertEquals(
        'Foo must remain the selected tab', foo, tabBar.getSelectedTab());

    tabBar.setSelectedTab(null);
    assertFalse('Foo must no longer be selected', foo.isSelected());
    assertNull('No tab must be selected', tabBar.getSelectedTab());
  },

  testGetSetSelectedTabIndex() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChildAt(foo = new UiTab('foo'), 0);
    tabBar.addChildAt(bar = new UiTab('bar'), 1);
    tabBar.addChildAt(baz = new UiTab('baz'), 2);

    assertEquals('No tab must be selected', -1, tabBar.getSelectedTabIndex());

    tabBar.setSelectedTabIndex(2);
    assertTrue('Baz must be selected', baz.isSelected());
    assertEquals(
        'Baz must be the selected tab', 2, tabBar.getSelectedTabIndex());

    tabBar.setSelectedTabIndex(0);
    assertFalse('Baz must no longer be selected', baz.isSelected());
    assertTrue('Foo must be selected', foo.isSelected());
    assertEquals(
        'Foo must be the selected tab', 0, tabBar.getSelectedTabIndex());

    tabBar.setSelectedTabIndex(0);
    assertTrue('Foo must remain selected', foo.isSelected());
    assertEquals(
        'Foo must remain the selected tab', 0, tabBar.getSelectedTabIndex());

    tabBar.setSelectedTabIndex(-1);
    assertFalse('Foo must no longer be selected', foo.isSelected());
    assertEquals('No tab must be selected', -1, tabBar.getSelectedTabIndex());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDeselectIfSelected() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChild(foo = new UiTab('foo'));
    tabBar.addChild(bar = new UiTab('bar'));
    tabBar.addChild(baz = new UiTab('baz'));

    // Start with the middle tab selected.
    bar.setSelected(true);
    assertTrue('Bar must be selected', bar.isSelected());
    assertEquals('Bar must be the selected tab', bar, tabBar.getSelectedTab());

    // Should be a no-op.
    tabBar.deselectIfSelected(null);
    assertTrue('Bar must remain selected', bar.isSelected());
    assertEquals(
        'Bar must remain the selected tab', bar, tabBar.getSelectedTab());

    // Should be a no-op.
    tabBar.deselectIfSelected(foo);
    assertTrue('Bar must remain selected', bar.isSelected());
    assertEquals(
        'Bar must remain the selected tab', bar, tabBar.getSelectedTab());

    // Should deselect bar and select the previous tab (foo).
    tabBar.deselectIfSelected(bar);
    assertFalse('Bar must no longer be selected', bar.isSelected());
    assertTrue('Foo must be selected', foo.isSelected());
    assertEquals('Foo must be the selected tab', foo, tabBar.getSelectedTab());

    // Should deselect foo and select the next tab (bar).
    tabBar.deselectIfSelected(foo);
    assertFalse('Foo must no longer be selected', foo.isSelected());
    assertTrue('Bar must be selected', bar.isSelected());
    assertEquals('Bar must be the selected tab', bar, tabBar.getSelectedTab());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHandleTabSelect() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChild(foo = new UiTab('foo'));
    tabBar.addChild(bar = new UiTab('bar'));
    tabBar.addChild(baz = new UiTab('baz'));

    assertNull('No tab must be selected', tabBar.getSelectedTab());

    tabBar.handleTabSelect(new GoogEvent(Component.EventType.SELECT, bar));
    assertEquals('Bar must be the selected tab', bar, tabBar.getSelectedTab());

    tabBar.handleTabSelect(new GoogEvent(Component.EventType.SELECT, bar));
    assertEquals('Bar must remain selected tab', bar, tabBar.getSelectedTab());

    tabBar.handleTabSelect(new GoogEvent(Component.EventType.SELECT, foo));
    assertEquals(
        'Foo must now be the selected tab', foo, tabBar.getSelectedTab());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHandleTabUnselect() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChild(foo = new UiTab('foo'));
    tabBar.addChild(bar = new UiTab('bar'));
    tabBar.addChild(baz = new UiTab('baz'));

    bar.setSelected(true);
    assertEquals('Bar must be the selected tab', bar, tabBar.getSelectedTab());

    tabBar.handleTabUnselect(new GoogEvent(Component.EventType.UNSELECT, foo));
    assertEquals(
        'Bar must remain the selected tab', bar, tabBar.getSelectedTab());

    tabBar.handleTabUnselect(new GoogEvent(Component.EventType.SELECT, bar));
    assertNull('No tab must be selected', tabBar.getSelectedTab());
  },

  testHandleTabDisable() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChild(foo = new UiTab('foo'));
    tabBar.addChild(bar = new UiTab('bar'));
    tabBar.addChild(baz = new UiTab('baz'));

    // Start with the middle tab selected.
    bar.setSelected(true);
    assertTrue('Bar must be selected', bar.isSelected());
    assertEquals('Bar must be the selected tab', bar, tabBar.getSelectedTab());

    // Should deselect bar and select the previous enabled, visible tab (foo).
    bar.setEnabled(false);
    assertFalse('Bar must no longer be selected', bar.isSelected());
    assertTrue('Foo must be selected', foo.isSelected());
    assertEquals('Foo must be the selected tab', foo, tabBar.getSelectedTab());

    // Should deselect foo and select the next enabled, visible tab (baz).
    foo.setEnabled(false);
    assertFalse('Foo must no longer be selected', foo.isSelected());
    assertTrue('Baz must be selected', baz.isSelected());
    assertEquals('Baz must be the selected tab', baz, tabBar.getSelectedTab());

    // Should deselect baz.  Since there are no enabled, visible tabs left,
    // the tab bar should have no selected tab.
    baz.setEnabled(false);
    assertFalse('Baz must no longer be selected', baz.isSelected());
    assertNull('No tab must be selected', tabBar.getSelectedTab());
  },

  testHandleTabHide() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChild(foo = new UiTab('foo'));
    tabBar.addChild(bar = new UiTab('bar'));
    tabBar.addChild(baz = new UiTab('baz'));

    // Start with the middle tab selected.
    bar.setSelected(true);
    assertTrue('Bar must be selected', bar.isSelected());
    assertEquals('Bar must be the selected tab', bar, tabBar.getSelectedTab());

    // Should deselect bar and select the previous enabled, visible tab (foo).
    bar.setVisible(false);
    assertFalse('Bar must no longer be selected', bar.isSelected());
    assertTrue('Foo must be selected', foo.isSelected());
    assertEquals('Foo must be the selected tab', foo, tabBar.getSelectedTab());

    // Should deselect foo and select the next enabled, visible tab (baz).
    foo.setVisible(false);
    assertFalse('Foo must no longer be selected', foo.isSelected());
    assertTrue('Baz must be selected', baz.isSelected());
    assertEquals('Baz must be the selected tab', baz, tabBar.getSelectedTab());

    // Should deselect baz.  Since there are no enabled, visible tabs left,
    // the tab bar should have no selected tab.
    baz.setVisible(false);
    assertFalse('Baz must no longer be selected', baz.isSelected());
    assertNull('No tab must be selected', tabBar.getSelectedTab());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHandleFocus() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChild(foo = new UiTab('foo'), true);
    tabBar.addChild(bar = new UiTab('bar'), true);
    tabBar.addChild(baz = new UiTab('baz'), true);

    // Render the tab bar into the document, so highlight handling works as
    // expected.
    tabBar.render(sandbox);

    // Start with the middle tab selected.
    bar.setSelected(true);
    assertTrue('Bar must be selected', bar.isSelected());
    assertEquals('Bar must be the selected tab', bar, tabBar.getSelectedTab());

    assertNull('No tab must be highlighted', tabBar.getHighlighted());
    tabBar.handleFocus(new GoogEvent(EventType.FOCUS, tabBar.getElement()));
    assertTrue('Bar must be highlighted', bar.isHighlighted());
    assertEquals(
        'Bar must be the highlighted tab', bar, tabBar.getHighlighted());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testHandleFocusWithoutSelectedTab() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChild(foo = new UiTab('foo'), true);
    tabBar.addChild(bar = new UiTab('bar'), true);
    tabBar.addChild(baz = new UiTab('baz'), true);

    // Render the tab bar into the document, so highlight handling works as
    // expected.
    tabBar.render(sandbox);

    // Start with no tab selected.
    assertNull('No tab must be selected', tabBar.getSelectedTab());

    assertNull('No tab must be highlighted', tabBar.getHighlighted());
    tabBar.handleFocus(new GoogEvent(EventType.FOCUS, tabBar.getElement()));
    assertTrue('Foo must be highlighted', foo.isHighlighted());
    assertEquals(
        'Foo must be the highlighted tab', foo, tabBar.getHighlighted());
  },

  testGetOrientationFromLocation() {
    assertEquals(
        Container.Orientation.HORIZONTAL,
        TabBar.getOrientationFromLocation(TabBar.Location.TOP));
    assertEquals(
        Container.Orientation.HORIZONTAL,
        TabBar.getOrientationFromLocation(TabBar.Location.BOTTOM));
    assertEquals(
        Container.Orientation.VERTICAL,
        TabBar.getOrientationFromLocation(TabBar.Location.START));
    assertEquals(
        Container.Orientation.VERTICAL,
        TabBar.getOrientationFromLocation(TabBar.Location.END));
  },

  testKeyboardNavigation() {
    let bar;
    let baz;
    let foo;

    // Create a tab bar with some tabs.
    tabBar.addChild(foo = new UiTab('foo'), true);
    tabBar.addChild(bar = new UiTab('bar'), true);
    tabBar.addChild(baz = new UiTab('baz'), true);
    tabBar.render(sandbox);

    // Highlight the selected tab (this happens automatically when the tab
    // bar receives keyboard focus).
    tabBar.setSelectedTabIndex(0);
    tabBar.getSelectedTab().setHighlighted(true);

    // Count events dispatched by each tab.
    const eventCount = {
      'foo': {'select': 0, 'unselect': 0},
      'bar': {'select': 0, 'unselect': 0},
      'baz': {'select': 0, 'unselect': 0},
    };

    function countEvent(e) {
      const tabId = e.target.getContent();
      const type = e.type;
      eventCount[tabId][type]++;
    }

    function getEventCount(tabId, type) {
      return eventCount[tabId][type];
    }

    // Listen for SELECT and UNSELECT events on the tab bar.
    events.listen(
        tabBar,
        [
          Component.EventType.SELECT,
          Component.EventType.UNSELECT,
        ],
        countEvent);

    // Verify baseline assumptions.
    assertTrue('Tab bar must auto-select tabs', tabBar.isAutoSelectTabs());
    assertEquals('First tab must be selected', 0, tabBar.getSelectedTabIndex());

    // Simulate a right arrow key event.
    const rightEvent = new FakeKeyEvent(KeyCodes.RIGHT);
    assertTrue(
        'Key event must have beeen handled', tabBar.handleKeyEvent(rightEvent));
    assertTrue(
        'Key event propagation must have been stopped',
        rightEvent.propagationStopped);
    assertTrue(
        'Default key event must have been prevented',
        rightEvent.defaultPrevented);
    assertEquals(
        'Foo must have dispatched UNSELECT', 1,
        getEventCount('foo', Component.EventType.UNSELECT));
    assertEquals(
        'Bar must have dispatched SELECT', 1,
        getEventCount('bar', Component.EventType.SELECT));
    assertEquals('Bar must have been selected', bar, tabBar.getSelectedTab());

    // Simulate a left arrow key event.
    const leftEvent = new FakeKeyEvent(KeyCodes.LEFT);
    assertTrue(
        'Key event must have beeen handled', tabBar.handleKeyEvent(leftEvent));
    assertTrue(
        'Key event propagation must have been stopped',
        leftEvent.propagationStopped);
    assertTrue(
        'Default key event must have been prevented',
        leftEvent.defaultPrevented);
    assertEquals(
        'Bar must have dispatched UNSELECT', 1,
        getEventCount('bar', Component.EventType.UNSELECT));
    assertEquals(
        'Foo must have dispatched SELECT', 1,
        getEventCount('foo', Component.EventType.SELECT));
    assertEquals('Foo must have been selected', foo, tabBar.getSelectedTab());

    // Disable tab auto-selection.
    tabBar.setAutoSelectTabs(false);

    // Simulate another left arrow key event.
    const anotherLeftEvent = new FakeKeyEvent(KeyCodes.LEFT);
    assertTrue(
        'Key event must have beeen handled',
        tabBar.handleKeyEvent(anotherLeftEvent));
    assertTrue(
        'Key event propagation must have been stopped',
        anotherLeftEvent.propagationStopped);
    assertTrue(
        'Default key event must have been prevented',
        anotherLeftEvent.defaultPrevented);
    assertEquals('Foo must remain selected', foo, tabBar.getSelectedTab());
    assertEquals(
        'Foo must not have dispatched another UNSELECT event', 1,
        getEventCount('foo', Component.EventType.UNSELECT));
    assertEquals(
        'Baz must not have dispatched a SELECT event', 0,
        getEventCount('baz', Component.EventType.SELECT));
    assertFalse('Baz must not be selected', baz.isSelected());
    assertTrue('Baz must be highlighted', baz.isHighlighted());

    // Simulate 'g' key event.
    const gEvent = new FakeKeyEvent(KeyCodes.G);
    assertFalse(
        'Key event must not have beeen handled', tabBar.handleKeyEvent(gEvent));
    assertFalse(
        'Key event propagation must not have been stopped',
        gEvent.propagationStopped);
    assertFalse(
        'Default key event must not have been prevented',
        gEvent.defaultPrevented);
    assertEquals('Foo must remain selected', foo, tabBar.getSelectedTab());

    // Clean up.
    events.unlisten(
        tabBar,
        [
          Component.EventType.SELECT,
          Component.EventType.UNSELECT,
        ],
        countEvent);
  },

  testExitAndEnterDocument() {
    const component = new Component();
    component.render(sandbox);

    const tab1 = new UiTab('tab1');
    const tab2 = new UiTab('tab2');
    const tab3 = new UiTab('tab3');
    tabBar.addChild(tab1, true);
    tabBar.addChild(tab2, true);
    tabBar.addChild(tab3, true);

    component.addChild(tabBar, true);
    tab2.setSelected(true);
    assertEquals(tabBar.getSelectedTab(), tab2);

    component.removeChild(tabBar, true);
    tab1.setSelected(true);
    assertEquals(tabBar.getSelectedTab(), tab2);

    component.addChild(tabBar, true);
    tab3.setSelected(true);
    assertEquals(tabBar.getSelectedTab(), tab3);
  },
});
