/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.SubMenuTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const GoogEvent = goog.require('goog.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const KeyHandler = goog.require('goog.events.KeyHandler');
const Menu = goog.require('goog.ui.Menu');
const MenuItem = goog.require('goog.ui.MenuItem');
const MockClock = goog.require('goog.testing.MockClock');
const Overflow = goog.require('goog.positioning.Overflow');
const State = goog.require('goog.a11y.aria.State');
const SubMenu = goog.require('goog.ui.SubMenu');
const SubMenuRenderer = goog.require('goog.ui.SubMenuRenderer');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const positioning = goog.require('goog.positioning');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let menu;
let clonedMenuDom;

let mockClock;

// mock out goog.positioning.positionAtCoordinate so that
// the menu always fits. (we don't care about testing the
// dynamic menu positioning if the menu doesn't fit in the window.)
const oldPositionFn = positioning.positionAtCoordinate;
/** @suppress {checkTypes} suppression added to enable type checking */
positioning.positionAtCoordinate =
    (absolutePos, movableElement, movableElementCorner, margin = undefined,
     overflow = undefined) =>
        oldPositionFn.call(
            null, absolutePos, movableElement, movableElementCorner, margin,
            Overflow.IGNORE);

function assertKeyHandlingIsCorrect(keyToOpenSubMenu, keyToCloseSubMenu) {
  menu.setFocusable(true);
  menu.decorate(dom.getElement('demoMenu'));

  const plainItem = menu.getChildAt(0);
  plainItem.setMnemonic(KeyCodes.F);

  const subMenuItem1 = menu.getChildAt(1);
  subMenuItem1.setMnemonic(KeyCodes.S);
  const subMenuItem1Menu = subMenuItem1.getMenu();
  menu.setHighlighted(plainItem);

  const fireKeySequence = testingEvents.fireKeySequence;

  const keySequenceReturn =
      fireKeySequence(plainItem.getElement(), keyToOpenSubMenu);

  // Expect keyToOpenSubMenu to only be handled when ENTER because menuItem
  // handles ENTER, while subMenu handles LEFT, RIGHT and ENTER.
  if (keyToOpenSubMenu == KeyCodes.ENTER) {
    assertFalse(
        'Expected OpenSubMenu key to be handled when KeyCode is ENTER',
        keySequenceReturn);
  } else {
    assertTrue('Expected OpenSubMenu key to not be handled', keySequenceReturn);
  }
  assertFalse(subMenuItem1Menu.isVisible());

  // Expect ENTER to be handled when subMenu is visible.
  subMenuItem1Menu.setVisible(true);
  assertFalse(fireKeySequence(subMenuItem1.getElement(), KeyCodes.ENTER));

  assertFalse(
      'Expected F key to be handled',
      fireKeySequence(plainItem.getElement(), KeyCodes.F));

  assertFalse(
      'Expected DOWN key to be handled',
      fireKeySequence(plainItem.getElement(), KeyCodes.DOWN));
  assertEquals(subMenuItem1, menu.getChildAt(1));

  // Expect keyToOpenSubMenu to be handled when subMenu is not visible.
  subMenuItem1Menu.setVisible(false);
  assertFalse(
      'Expected OpenSubMenu key to be handled',
      fireKeySequence(subMenuItem1.getElement(), keyToOpenSubMenu));
  assertTrue(subMenuItem1Menu.isVisible());

  assertFalse(
      'Expected CloseSubMenu key to be handled',
      fireKeySequence(subMenuItem1.getElement(), keyToCloseSubMenu));
  assertFalse(subMenuItem1Menu.isVisible());

  assertFalse(
      'Expected UP key to be handled',
      fireKeySequence(subMenuItem1.getElement(), KeyCodes.UP));

  assertFalse(
      'Expected S key to be handled',
      fireKeySequence(plainItem.getElement(), KeyCodes.S));
  assertTrue(subMenuItem1Menu.isVisible());
}

/**
 * Asserts that this sub menu renders in the right direction relative to
 * the parent menu.
 * @param {SubMenu} subMenu The sub menu.
 * @param {boolean} left True for left-pointing, false for right-pointing.
 */
function assertRenderDirection(subMenu, left) {
  subMenu.getParent().setHighlighted(subMenu);
  subMenu.showSubMenu();
  const menuItemPosition = style.getPageOffset(subMenu.getElement());
  const menuPosition = style.getPageOffset(subMenu.getMenu().getElement());
  assert(Math.abs(menuItemPosition.y - menuPosition.y) < 5);
  assertEquals(
      'Menu at: ' + menuPosition.x + ', submenu item at: ' + menuItemPosition.x,
      left, menuPosition.x < menuItemPosition.x);
}

/**
 * Asserts that this sub menu has a properly-oriented arrow.
 * @param {SubMenu} subMenu The sub menu.
 * @param {boolean} left True for left-pointing, false for right-pointing.
 * @suppress {visibility} suppression added to enable type checking
 */
function assertArrowDirection(subMenu, left) {
  assertEquals(
      left ? SubMenuRenderer.LEFT_ARROW_ : SubMenuRenderer.RIGHT_ARROW_,
      getArrowElement(subMenu).innerHTML);
}

/**
 * Asserts that the arrow position is correct.
 * @param {SubMenu} subMenu The sub menu.
 * @param {boolean} leftAlign True for left-aligned, false for right-aligned.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function assertArrowPosition(subMenu, left) {
  const arrow = getArrowElement(subMenu);
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  const expectedLeft =
      left ? 0 : arrow.offsetParent.offsetWidth - arrow.offsetWidth;
  /**
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  const actualLeft = arrow.offsetLeft;
  assertTrue(
      `Expected left offset: ${expectedLeft}
` +
          'Actual left offset: ' + actualLeft + '\n',
      Math.abs(expectedLeft - actualLeft) < 5);
}

/**
 * Gets the arrow element of a sub menu.
 * @param {SubMenu} subMenu The sub menu.
 * @return {Element} The arrow.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function getArrowElement(subMenu) {
  return subMenu.getContentElement().lastChild;
}
testSuite({
  setUp() {
    clonedMenuDom = dom.getElement('demoMenu').cloneNode(true);

    menu = new Menu();
  },

  tearDown() {
    document.body.style.direction = 'ltr';
    menu.dispose();

    const element = dom.getElement('demoMenu');
    element.parentNode.replaceChild(clonedMenuDom, element);

    dom.removeChildren(dom.getElement('sandbox'));

    if (mockClock) {
      mockClock.uninstall();
      mockClock = null;
    }
  },

  testEnterOpensSubmenu() {
    assertKeyHandlingIsCorrect(KeyCodes.ENTER, KeyCodes.LEFT);
  },

  testKeyHandling_ltr() {
    assertKeyHandlingIsCorrect(KeyCodes.RIGHT, KeyCodes.LEFT);
  },

  testKeyHandling_rtl() {
    document.body.style.direction = 'rtl';
    assertKeyHandlingIsCorrect(KeyCodes.LEFT, KeyCodes.RIGHT);
  },

  testNormalLtrSubMenu() {
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    assertArrowDirection(subMenu, false);
    assertRenderDirection(subMenu, false);
    assertArrowPosition(subMenu, false);
  },

  testNormalRtlSubMenu() {
    document.body.style.direction = 'rtl';
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    assertArrowDirection(subMenu, true);
    assertRenderDirection(subMenu, true);
    assertArrowPosition(subMenu, true);
  },

  testLtrSubMenuAlignedToStart() {
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    subMenu.setAlignToEnd(false);
    assertArrowDirection(subMenu, true);
    assertRenderDirection(subMenu, true);
    assertArrowPosition(subMenu, false);
  },

  testNullContentElement() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const subMenu = new SubMenu();
    subMenu.setContent('demo');
  },

  testRtlSubMenuAlignedToStart() {
    document.body.style.direction = 'rtl';
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    subMenu.setAlignToEnd(false);
    assertArrowDirection(subMenu, false);
    assertRenderDirection(subMenu, false);
    assertArrowPosition(subMenu, true);
  },

  testSetContentKeepsArrow_ltr() {
    document.body.style.direction = 'ltr';
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    subMenu.setAlignToEnd(false);
    subMenu.setContent('test');
    assertArrowDirection(subMenu, true);
    assertRenderDirection(subMenu, true);
    assertArrowPosition(subMenu, false);
  },

  testSetContentKeepsArrow_rtl() {
    document.body.style.direction = 'rtl';
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    subMenu.setAlignToEnd(false);
    subMenu.setContent('test');
    assertArrowDirection(subMenu, false);
    assertRenderDirection(subMenu, false);
    assertArrowPosition(subMenu, true);
  },

  testExitDocument() {
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    const innerMenu = subMenu.getMenu();

    assertTrue('Top-level menu was not in document', menu.isInDocument());
    assertTrue('Submenu was not in document', subMenu.isInDocument());
    assertTrue('Inner menu was not in document', innerMenu.isInDocument());

    menu.exitDocument();

    assertFalse('Top-level menu was in document', menu.isInDocument());
    assertFalse('Submenu was in document', subMenu.isInDocument());
    assertFalse('Inner menu was in document', innerMenu.isInDocument());
  },

  testDisposal() {
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    const innerMenu = subMenu.getMenu();
    menu.dispose();

    assert('Top-level menu was not disposed', menu.getDisposed());
    assert('Submenu was not disposed', subMenu.getDisposed());
    assert('Inner menu was not disposed', innerMenu.getDisposed());
  },

  testShowAndDismissSubMenu() {
    let openEventDispatched = false;
    let closeEventDispatched = false;

    function handleEvent(e) {
      switch (e.type) {
        case Component.EventType.OPEN:
          openEventDispatched = true;
          break;
        case Component.EventType.CLOSE:
          closeEventDispatched = true;
          break;
        default:
          fail('Invalid event type: ' + e.type);
      }
    }

    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    subMenu.setHighlighted(true);

    events.listen(
        subMenu, [Component.EventType.OPEN, Component.EventType.CLOSE],
        handleEvent);

    assertFalse(
        'Submenu must not have "-open" CSS class',
        classlist.contains(subMenu.getElement(), 'goog-submenu-open'));
    assertFalse(
        'Popup menu must not be visible', subMenu.getMenu().isVisible());
    assertFalse('No OPEN event must have been dispatched', openEventDispatched);
    assertFalse(
        'No CLOSE event must have been dispatched', closeEventDispatched);

    subMenu.showSubMenu();
    assertTrue(
        'Submenu must have "-open" CSS class',
        classlist.contains(subMenu.getElement(), 'goog-submenu-open'));
    assertTrue('Popup menu must be visible', subMenu.getMenu().isVisible());
    assertTrue('OPEN event must have been dispatched', openEventDispatched);
    assertFalse(
        'No CLOSE event must have been dispatched', closeEventDispatched);

    subMenu.dismissSubMenu();
    assertFalse(
        'Submenu must not have "-open" CSS class',
        classlist.contains(subMenu.getElement(), 'goog-submenu-open'));
    assertFalse(
        'Popup menu must not be visible', subMenu.getMenu().isVisible());
    assertTrue('CLOSE event must have been dispatched', closeEventDispatched);

    events.unlisten(
        subMenu, [Component.EventType.OPEN, Component.EventType.CLOSE],
        handleEvent);
  },

  testDismissWhenSubMenuNotVisible() {
    let openEventDispatched = false;
    let closeEventDispatched = false;

    function handleEvent(e) {
      switch (e.type) {
        case Component.EventType.OPEN:
          openEventDispatched = true;
          break;
        case Component.EventType.CLOSE:
          closeEventDispatched = true;
          break;
        default:
          fail('Invalid event type: ' + e.type);
      }
    }

    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    subMenu.setHighlighted(true);

    events.listen(
        subMenu, [Component.EventType.OPEN, Component.EventType.CLOSE],
        handleEvent);

    assertFalse(
        'Submenu must not have "-open" CSS class',
        classlist.contains(subMenu.getElement(), 'goog-submenu-open'));
    assertFalse(
        'Popup menu must not be visible', subMenu.getMenu().isVisible());
    assertFalse('No OPEN event must have been dispatched', openEventDispatched);
    assertFalse(
        'No CLOSE event must have been dispatched', closeEventDispatched);

    subMenu.showSubMenu();
    subMenu.getMenu().setVisible(false);

    subMenu.dismissSubMenu();
    assertFalse(
        'Submenu must not have "-open" CSS class',
        classlist.contains(subMenu.getElement(), 'goog-submenu-open'));
    assertFalse(subMenu.menuIsVisible_);
    assertFalse(
        'Popup menu must not be visible', subMenu.getMenu().isVisible());
    assertTrue('CLOSE event must have been dispatched', closeEventDispatched);

    events.unlisten(
        subMenu, [Component.EventType.OPEN, Component.EventType.CLOSE],
        handleEvent);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCloseSubMenuBehavior() {
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    subMenu.getElement().id = 'subMenu';

    const innerMenu = subMenu.getMenu();
    innerMenu.getChildAt(0).getElement().id = 'child1';

    subMenu.setHighlighted(true);
    subMenu.showSubMenu();

    function MyFakeEvent(keyCode, opt_eventType) {
      this.type = opt_eventType || KeyHandler.EventType.KEY;
      this.keyCode = keyCode;
      this.propagationStopped = false;
      this.preventDefault = goog.nullFunction;
      this.stopPropagation = function() {
        this.propagationStopped = true;
      };
    }

    // Focus on the first item in the submenu and verify the activedescendant is
    // set correctly.
    subMenu.handleKeyEvent(new MyFakeEvent(KeyCodes.DOWN));
    assertEquals(
        'First item in submenu must be the aria-activedescendant', 'child1',
        aria.getState(menu.getElement(), State.ACTIVEDESCENDANT));

    // Dismiss the submenu and verify the activedescendant is updated correctly.
    subMenu.handleKeyEvent(new MyFakeEvent(KeyCodes.LEFT));
    assertEquals(
        'Submenu must be the aria-activedescendant', 'subMenu',
        aria.getState(menu.getElement(), State.ACTIVEDESCENDANT));
  },

  testLazyInstantiateSubMenu() {
    menu.decorate(dom.getElement('demoMenu'));
    const subMenu = menu.getChildAt(1);
    subMenu.setHighlighted(true);

    let lazyMenu;

    const key = events.listen(subMenu, Component.EventType.OPEN, (e) => {
      lazyMenu = new Menu();
      lazyMenu.addItem(new MenuItem('foo'));
      lazyMenu.addItem(new MenuItem('bar'));
      subMenu.setMenu(lazyMenu, /* opt_internal */ false);
    });

    subMenu.showSubMenu();
    assertNotNull('Popup menu must have been created', lazyMenu);
    assertEquals(
        'Popup menu must be a child of the submenu', subMenu,
        lazyMenu.getParent());
    assertTrue('Popup menu must have been rendered', lazyMenu.isInDocument());
    assertTrue('Popup menu must be visible', lazyMenu.isVisible());

    menu.dispose();
    assertTrue('Submenu must have been disposed of', subMenu.isDisposed());
    assertFalse(
        'Popup menu must not have been disposed of', lazyMenu.isDisposed());

    lazyMenu.dispose();

    events.unlistenByKey(key);
  },

  testReusableMenu() {
    const subMenuOne = new SubMenu('SubMenu One');
    const subMenuTwo = new SubMenu('SubMenu Two');
    menu.addItem(subMenuOne);
    menu.addItem(subMenuTwo);
    menu.render(dom.getElement('sandbox'));

    // It is possible for the same popup menu to be shared between different
    // submenus.
    const sharedMenu = new Menu();
    sharedMenu.addItem(new MenuItem('Hello'));
    sharedMenu.addItem(new MenuItem('World'));

    assertNull('Shared menu must not have a parent', sharedMenu.getParent());

    subMenuOne.setMenu(sharedMenu);
    assertEquals(
        'SubMenuOne must point to the shared menu', sharedMenu,
        subMenuOne.getMenu());
    assertEquals(
        'SubMenuOne must be the shared menu\'s parent', subMenuOne,
        sharedMenu.getParent());

    subMenuTwo.setMenu(sharedMenu);
    assertEquals(
        'SubMenuTwo must point to the shared menu', sharedMenu,
        subMenuTwo.getMenu());
    assertEquals(
        'SubMenuTwo must be the shared menu\'s parent', subMenuTwo,
        sharedMenu.getParent());
    assertEquals(
        'SubMenuOne must still point to the shared menu', sharedMenu,
        subMenuOne.getMenu());

    menu.setHighlighted(subMenuOne);
    subMenuOne.showSubMenu();
    assertEquals(
        'SubMenuOne must point to the shared menu', sharedMenu,
        subMenuOne.getMenu());
    assertEquals(
        'SubMenuOne must be the shared menu\'s parent', subMenuOne,
        sharedMenu.getParent());
    assertEquals(
        'SubMenuTwo must still point to the shared menu', sharedMenu,
        subMenuTwo.getMenu());
    assertTrue('Shared menu must be visible', sharedMenu.isVisible());

    menu.setHighlighted(subMenuTwo);
    subMenuTwo.showSubMenu();
    assertEquals(
        'SubMenuTwo must point to the shared menu', sharedMenu,
        subMenuTwo.getMenu());
    assertEquals(
        'SubMenuTwo must be the shared menu\'s parent', subMenuTwo,
        sharedMenu.getParent());
    assertEquals(
        'SubMenuOne must still point to the shared menu', sharedMenu,
        subMenuOne.getMenu());
    assertTrue('Shared menu must be visible', sharedMenu.isVisible());
  },

  /**
   * If you remove a submenu in the interval between when a mouseover event
   * is fired on it, and showSubMenu() is called, showSubMenu causes a null
   * value to be dereferenced. This test validates that the fix for this works.
   * (See bug 1823144).
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testDeleteItemDuringSubmenuDisplayInterval() {
    mockClock = new MockClock(true);

    const submenu = new SubMenu('submenu');
    submenu.addItem(new MenuItem('submenu item 1'));
    menu.addItem(submenu);

    // Trigger mouseover, and remove item before showSubMenu can be called.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const e = new GoogEvent();
    submenu.handleMouseOver(e);
    menu.removeItem(submenu);
    mockClock.tick(SubMenu.MENU_DELAY_MS);
    // (No JS error should occur.)
  },

  testShowSubMenuAfterRemoval() {
    const submenu = new SubMenu('submenu');
    menu.addItem(submenu);
    menu.removeItem(submenu);
    submenu.showSubMenu();
    // (No JS error should occur.)
  },

  /**
     Tests that if a sub menu is selectable, then it can handle actions.
     @suppress {visibility} suppression added to enable type checking
   */
  testSubmenuSelectable() {
    const submenu = new SubMenu('submenu');
    submenu.addItem(new MenuItem('submenu item 1'));
    menu.addItem(submenu);
    submenu.setSelectable(true);

    let numClicks = 0;
    const menuClickedFn = (e) => {
      numClicks++;
    };

    events.listen(submenu, Component.EventType.ACTION, menuClickedFn);
    submenu.performActionInternal(null);
    submenu.performActionInternal(null);

    assertEquals('The submenu should have fired an event', 2, numClicks);

    submenu.setSelectable(false);
    submenu.performActionInternal(null);

    assertEquals(
        'The submenu should not have fired any further events', 2, numClicks);
  },

  /**
     Tests that if a sub menu is checkable, then it can handle actions.
     @suppress {visibility} suppression added to enable type checking
   */
  testSubmenuCheckable() {
    const submenu = new SubMenu('submenu');
    submenu.addItem(new MenuItem('submenu item 1'));
    menu.addItem(submenu);
    submenu.setCheckable(true);

    let numClicks = 0;
    const menuClickedFn = (e) => {
      numClicks++;
    };

    events.listen(submenu, Component.EventType.ACTION, menuClickedFn);
    submenu.performActionInternal(null);
    submenu.performActionInternal(null);

    assertEquals('The submenu should have fired an event', 2, numClicks);

    submenu.setCheckable(false);
    submenu.performActionInternal(null);

    assertEquals(
        'The submenu should not have fired any further events', 2, numClicks);
  },

  /**
     Tests that entering a child menu cancels the dismiss timer for the
     submenu.
   */
  testEnteringChildCancelsDismiss() {
    const submenu = new SubMenu('submenu');
    submenu.isInDocument = functions.TRUE;
    submenu.addItem(new MenuItem('submenu item 1'));
    menu.addItem(submenu);

    mockClock = new MockClock(true);
    submenu.getMenu().setVisible(true);

    // This starts the dismiss timer.
    submenu.setHighlighted(false);

    // This should cancel the dismiss timer.
    submenu.getMenu().dispatchEvent(Component.EventType.ENTER);

    // Tick the length of the dismiss timer.
    mockClock.tick(SubMenu.MENU_DELAY_MS);

    // Check that the menu is now highlighted and still visible.
    assertTrue(submenu.getMenu().isVisible());
    assertTrue(submenu.isHighlighted());
  },
});
