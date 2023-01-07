/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.MenuButtonTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const Corner = goog.require('goog.positioning.Corner');
const EventType = goog.require('goog.events.EventType');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const GoogEvent = goog.require('goog.events.Event');
const KeyCodes = goog.require('goog.events.KeyCodes');
const KeyHandler = goog.require('goog.events.KeyHandler');
const Menu = goog.require('goog.ui.Menu');
const MenuAnchoredPosition = goog.require('goog.positioning.MenuAnchoredPosition');
const MenuButton = goog.require('goog.ui.MenuButton');
const MenuItem = goog.require('goog.ui.MenuItem');
const Overflow = goog.require('goog.positioning.Overflow');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const State = goog.require('goog.a11y.aria.State');
const SubMenu = goog.require('goog.ui.SubMenu');
const TagName = goog.require('goog.dom.TagName');
const Timer = goog.require('goog.Timer');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const isVersion = goog.require('goog.userAgent.product.isVersion');
const positioning = goog.require('goog.positioning');
const product = goog.require('goog.userAgent.product');
const recordFunction = goog.require('goog.testing.recordFunction');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

let menuButton;
let clonedMenuButtonDom;
let expectedFailures;

// Mock out goog.positioning.positionAtCoordinate to always ignore failure when
// the window is too small, since we don't care about the viewport size on
// the selenium farm.
// TODO(nicksantos): Move this into a common location if we ever have enough
// code for a general goog.testing.ui library.
const originalPositionAtCoordinate = positioning.positionAtCoordinate;
positioning.positionAtCoordinate = function(
    absolutePos, movableElement, movableElementCorner, margin = undefined,
    viewport = undefined, overflow = undefined, preferredSize = undefined) {
  return originalPositionAtCoordinate.call(
      this, absolutePos, movableElement, movableElementCorner, margin, viewport,
      Overflow.IGNORE, preferredSize);
};

/**
 * Creates an event for use in multiple tests.
 * @param {!KeyCodes} keyCode Key event to handle.
 * @param {!KeyHandler.EventType=} eventType An optional EventType that defaults
 *     to `KeyHandler.EventType.KEY`, but can be set to a different EventType.
 */
function MyFakeEvent(keyCode, eventType = KeyHandler.EventType.KEY) {
  /** @suppress {globalThis} suppression added to enable type checking */
  this.type = eventType;
  /** @suppress {globalThis} suppression added to enable type checking */
  this.keyCode = keyCode;
  /** @suppress {globalThis} suppression added to enable type checking */
  this.propagationStopped = false;
  /** @suppress {globalThis} suppression added to enable type checking */
  this.preventDefault = goog.nullFunction;
  /** @suppress {globalThis} suppression added to enable type checking */
  this.stopPropagation = function() {
    /** @suppress {globalThis} suppression added to enable type checking */
    this.propagationStopped = true;
  };
}

/** Check if the aria-haspopup property is set correctly. */
function checkHasPopUp() {
  menuButton.enterDocument();
  assertFalse(
      'Menu button must have aria-haspopup attribute set to false',
      aria.getState(menuButton.getElement(), State.HASPOPUP));
  const menu = new Menu();
  menu.createDom();
  menuButton.setMenu(menu);
  assertTrue(
      'Menu button must have aria-haspopup attribute set to true',
      aria.getState(menuButton.getElement(), State.HASPOPUP));
  menuButton.setMenu(null);
  assertFalse(
      'Menu button must have aria-haspopup attribute set to false',
      aria.getState(menuButton.getElement(), State.HASPOPUP));
}

function isWinSafariBefore5() {
  return userAgent.WINDOWS && product.SAFARI && isVersion(4) && !isVersion(5);
}

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
  },

  setUp() {
    window.scrollTo(0, 0);

    const viewportSize = dom.getViewportSize();
    // Some tests need enough size viewport.
    if (viewportSize.width < 600 || viewportSize.height < 600) {
      window.moveTo(0, 0);
      window.resizeTo(640, 640);
    }

    clonedMenuButtonDom = dom.getElement('demoMenuButton').cloneNode(true);

    menuButton = new MenuButton();
  },

  tearDown() {
    expectedFailures.handleTearDown();
    menuButton.dispose();

    const element = dom.getElement('demoMenuButton');
    element.parentNode.replaceChild(clonedMenuButtonDom, element);
  },

  /**
   * Open the menu and click on the menu item inside.
   * Check if the aria-haspopup property is set correctly.
   */
  testBasicButtonBehavior() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);

    assertEquals(
        'Menu button must have aria-haspopup attribute set to true', 'true',
        aria.getState(menuButton.getElement(), State.HASPOPUP));

    testingEvents.fireClickSequence(node);

    assertTrue('Menu must open after click', menuButton.isOpen());

    let menuItemClicked = 0;
    let lastMenuItemClicked = null;
    events.listen(menuButton.getMenu(), Component.EventType.ACTION, (e) => {
      menuItemClicked++;
      lastMenuItemClicked = e.target;
    });

    const menuItem2 = dom.getElement('menuItem2');
    testingEvents.fireClickSequence(menuItem2);
    assertFalse('Menu must close on clicking when open', menuButton.isOpen());
    assertEquals(
        'Number of menu items clicked should be 1', 1, menuItemClicked);
    assertEquals(
        'menuItem2 should be the last menuitem clicked', menuItem2,
        lastMenuItemClicked.getElement());
  },

  /**
   * Open the menu, highlight first menuitem and then the second.
   * Check if the aria-activedescendant property is set correctly.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testHighlightItemBehavior() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    testingEvents.fireClickSequence(node);

    assertTrue('Menu must open after click', menuButton.isOpen());

    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.DOWN));
    assertNotNull(menuButton.getElement());
    assertEquals(
        'First menuitem must be the aria-activedescendant', 'menuItem1',
        aria.getState(menuButton.getElement(), State.ACTIVEDESCENDANT));

    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.DOWN));
    assertEquals(
        'Second menuitem must be the aria-activedescendant', 'menuItem2',
        aria.getState(menuButton.getElement(), State.ACTIVEDESCENDANT));
  },

  /**
   * Check that the appropriate items are selected when menus are opened with
   * the keyboard and setSelectFirstOnEnterOrSpace is not set.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testHighlightFirstOnOpen() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.ENTER));
    assertEquals(
        'By default no items should be highlighted when opened with enter.',
        null, menuButton.getMenu().getHighlighted());

    menuButton.setOpen(false);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.DOWN));
    assertTrue('Menu must open after down key', menuButton.isOpen());
    assertEquals(
        'First menuitem must be highlighted', 'menuItem1',
        menuButton.getMenu().getHighlighted().getElement().id);
  },

  /**
   * Check that the appropriate items are selected when menus are opened with
   * the keyboard, setSelectFirstOnEnterOrSpace is not set, and the first menu
   * item is disabled.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testHighlightFirstOnOpen_withFirstDisabled() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    const menu = menuButton.getMenu();
    menu.getItemAt(0).setEnabled(false);

    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.ENTER));
    assertEquals(
        'By default no items should be highlighted when opened with enter.',
        null, menuButton.getMenu().getHighlighted());

    menuButton.setOpen(false);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.DOWN));
    assertTrue('Menu must open after down key', menuButton.isOpen());
    assertEquals(
        'First enabled menuitem must be highlighted', 'menuItem2',
        menuButton.getMenu().getHighlighted().getElement().id);
  },

  /**
   * Check that the appropriate items are selected when menus are opened with
   * the keyboard and setSelectFirstOnEnterOrSpace is set.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testHighlightFirstOnOpen_withEnterOrSpaceSet() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    menuButton.setSelectFirstOnEnterOrSpace(true);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.ENTER));
    assertEquals(
        'The first item should be highlighted when opened with enter ' +
            'after setting selectFirstOnEnterOrSpace',
        'menuItem1', menuButton.getMenu().getHighlighted().getElement().id);
  },

  /**
   * Check that the appropriate item is selected when a menu is opened with the
   * keyboard, setSelectFirstOnEnterOrSpace is true, and the first menu item is
   * disabled.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testHighlightFirstOnOpen_withEnterOrSpaceSetAndFirstDisabled() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    menuButton.setSelectFirstOnEnterOrSpace(true);
    const menu = menuButton.getMenu();
    menu.getItemAt(0).setEnabled(false);

    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.ENTER));
    assertEquals(
        'The first enabled item should be highlighted when opened ' +
            'with enter after setting selectFirstOnEnterOrSpace',
        'menuItem2', menuButton.getMenu().getHighlighted().getElement().id);
  },

  /**
   * Open the menu, enter a submenu and then back out of it.
   * Check if the aria-activedescendant property is set correctly.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testCloseSubMenuBehavior() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    const menu = menuButton.getMenu();
    const subMenu = new SubMenu('Submenu');
    menu.addItem(subMenu);
    subMenu.getElement().id = 'subMenu';
    const subMenuMenu = new Menu();
    subMenu.setMenu(subMenuMenu);
    const subMenuItem = new MenuItem('Submenu item 1');
    subMenuMenu.addItem(subMenuItem);
    subMenuItem.getElement().id = 'subMenuItem1';
    menuButton.setOpen(true);

    for (let i = 0; i < 4; i++) {
      menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.DOWN));
    }
    assertEquals(
        'Submenu must be the aria-activedescendant', 'subMenu',
        aria.getState(menuButton.getElement(), State.ACTIVEDESCENDANT));

    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.RIGHT));
    assertEquals(
        'Submenu item 1 must be the aria-activedescendant', 'subMenuItem1',
        aria.getState(menuButton.getElement(), State.ACTIVEDESCENDANT));

    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.LEFT));
    assertEquals(
        'Submenu must be the aria-activedescendant', 'subMenu',
        aria.getState(menuButton.getElement(), State.ACTIVEDESCENDANT));
  },

  /**
   * Make sure the menu opens when enter is pressed.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testEnterOpensMenu() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.ENTER));
    assertTrue('Menu must open after enter', menuButton.isOpen());
  },

  /**
     Tests the behavior of the enter and space keys when the menu is open.
     @suppress {checkTypes} suppression added to enable type checking
   */
  testSpaceOrEnterClosesMenu() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);

    menuButton.setOpen(true);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.ENTER));
    assertFalse('Menu should close after pressing Enter', menuButton.isOpen());

    menuButton.setOpen(true);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.SPACE, EventType.KEYUP));
    assertFalse('Menu should close after pressing Space', menuButton.isOpen());
  },

  /**
   * Tests the behavior of the enter and space keys when the menu is open and
   * setCloseOnEnterOrSpace was called with false as its argument.
   * @suppress {checkTypes} suppression added to enable type checking
   */
  testSpaceOrEnterLeavesMenuOpen_withCloseOnEnterOrSpaceDisabled() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    menuButton.setCloseOnEnterOrSpace(false);

    menuButton.setOpen(true);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.ENTER));
    assertTrue(
        'Menu should remain open after pressing Enter', menuButton.isOpen());
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.SPACE, EventType.KEYUP));
    assertTrue(
        'Menu should remain open after pressing Space', menuButton.isOpen());
  },

  // Tests the behavior of the enter key on a submenu.
  /** @suppress {checkTypes} suppression added to enable type checking */
  testEnterClosesSubMenu() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    const menu = menuButton.getMenu();
    const subMenu = new SubMenu('Submenu');
    menu.addItem(subMenu);
    menuButton.setOpen(true);
    // Set the last child of the menu (the SubMenu) as highlighted so that the
    // SubMenu will handle the key event via the highlighted control in the
    // Container's handleKeyEventInternal function.
    menu.setHighlightedIndex(menu.getItemCount() - 1);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.ENTER));
    assertTrue(
        'Menu should remain open after pressing Enter', menuButton.isOpen());
  },

  // Tests the behavior of the esc key on a submenu.
  /** @suppress {checkTypes} suppression added to enable type checking */
  testEscClosesSubMenu() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    const menu = menuButton.getMenu();
    const subMenu = new SubMenu('Submenu');
    menu.addItem(subMenu);
    menuButton.setOpen(true);
    // Set the last child of the menu (the SubMenu) as highlighted so that the
    // SubMenu will handle the key event via the highlighted control in the
    // Container's handleKeyEventInternal function.
    menu.setHighlightedIndex(menu.getItemCount() - 1);
    menuButton.handleKeyEvent(new MyFakeEvent(KeyCodes.ESC));
    assertFalse('Menu should close after pressing Esc', menuButton.isOpen());
  },

  /**
   * Tests that a keydown event of the escape key propagates normally when the
   * menu is closed.
   * @suppress {visibility} suppression added to enable type checking
   */
  testStopEscapePropagationMenuClosed() {
    const node = dom.getElement('demoMenuButton');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fakeEvent = new MyFakeEvent(KeyCodes.ESCAPE, EventType.KEYDOWN);
    menuButton.decorate(node);
    menuButton.setOpen(false);

    menuButton.handleKeyDownEvent_(fakeEvent);
    assertFalse(
        'Event propagation was erroneously stopped.',
        fakeEvent.propagationStopped);
  },

  /**
   * Tests that a keydown event of the escape key is prevented from propagating
   * when the menu is open.
   * @suppress {visibility} suppression added to enable type checking
   */
  testStopEscapePropagationMenuOpen() {
    const node = dom.getElement('demoMenuButton');
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fakeEvent = new MyFakeEvent(KeyCodes.ESCAPE, EventType.KEYDOWN);
    menuButton.decorate(node);
    menuButton.setOpen(true);

    menuButton.handleKeyDownEvent_(fakeEvent);
    assertTrue(
        'Event propagation was not stopped.', fakeEvent.propagationStopped);
  },

  /**
   * Open the menu and click on the menu item inside after exiting and entering
   * the document once, to test proper setup/teardown behavior of MenuButton.
   */
  testButtonAfterEnterDocument() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);

    menuButton.exitDocument();
    menuButton.enterDocument();

    testingEvents.fireClickSequence(node);
    assertTrue('Menu must open after click', menuButton.isOpen());

    const menuItem2 = dom.getElement('menuItem2');
    testingEvents.fireClickSequence(menuItem2);
    assertFalse('Menu must close on clicking when open', menuButton.isOpen());
  },

  /**
   * Renders the menu button, moves its menu and then repositions to make sure
   * the position is more or less ok.
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  testPositionMenu() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    const menu = menuButton.getMenu();
    menu.setVisible(true, true);

    // Move to 500, 500
    menu.setPosition(500, 500);

    // Now reposition and make sure position is more or less ok.
    menuButton.positionMenu();
    const menuNode = dom.getElement('demoMenu');
    assertRoughlyEquals(
        menuNode.offsetTop, node.offsetTop + node.offsetHeight, 20);
    assertRoughlyEquals(menuNode.offsetLeft, node.offsetLeft, 20);
  },

  /**
   * Tests that calling positionMenu when the menu is not in the document does
   * not throw an exception.
   */
  testPositionMenuNotInDocument() {
    const menu = new Menu();
    menu.createDom();
    menuButton.setMenu(menu);
    menuButton.positionMenu();
  },

  /**
   * Shows the menu and moves the menu button, a timer correct the menu
   *      position.
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  testOpenedMenuPositionCorrection() {
    const iframe = dom.getElement('iframe1');
    const iframeDoc = dom.getFrameContentDocument(iframe);
    const iframeDom = dom.getDomHelper(iframeDoc);
    const iframeWindow = dom.getWindow(iframeDoc);

    const button = new MenuButton();
    iframeWindow.scrollTo(0, 0);
    const node = iframeDom.getElement('demoMenuButton');
    button.decorate(node);
    const mockTimer = new Timer();
    // Don't start the timer.  We manually dispatch the Tick event.
    mockTimer.start = goog.nullFunction;
    /** @suppress {visibility} suppression added to enable type checking */
    button.timer_ = mockTimer;

    const replacer = new PropertyReplacer();
    let positionMenuCalled;
    const origPositionMenu = goog.bind(button.positionMenu, button);
    replacer.set(button, 'positionMenu', () => {
      positionMenuCalled = true;
      origPositionMenu();
    });

    // Show the menu.
    button.setOpen(true);

    // Confirm the menu position
    const menuNode = iframeDom.getElement('demoMenu');
    assertRoughlyEquals(
        menuNode.offsetTop, node.offsetTop + node.offsetHeight, 20);
    assertRoughlyEquals(menuNode.offsetLeft, node.offsetLeft, 20);

    positionMenuCalled = false;
    // A Tick event is dispatched.
    mockTimer.dispatchEvent(Timer.TICK);
    assertFalse('positionMenu() shouldn\'t be called.', positionMenuCalled);

    // Move the menu button by DOM structure change
    const p1 =
        iframeDom.createDom(TagName.P, null, iframeDom.createTextNode('foo'));
    const p2 =
        iframeDom.createDom(TagName.P, null, iframeDom.createTextNode('foo'));
    const p3 =
        iframeDom.createDom(TagName.P, null, iframeDom.createTextNode('foo'));
    iframeDom.insertSiblingBefore(p1, node);
    iframeDom.insertSiblingBefore(p2, node);
    iframeDom.insertSiblingBefore(p3, node);

    // Confirm the menu is detached from the button.
    assertTrue(
        Math.abs(node.offsetTop + node.offsetHeight - menuNode.offsetTop) > 20);

    positionMenuCalled = false;
    // A Tick event is dispatched.
    mockTimer.dispatchEvent(Timer.TICK);
    assertTrue('positionMenu() should be called.', positionMenuCalled);

    // The menu is moved to appropriate position again.
    assertRoughlyEquals(
        menuNode.offsetTop, node.offsetTop + node.offsetHeight, 20);

    // Make the frame page scrollable.
    const viewportHeight = iframeDom.getViewportSize().height;
    const footer = iframeDom.getElement('footer');
    style.setSize(footer, 1, viewportHeight * 2);
    // Change the viewport offset.
    iframeWindow.scrollTo(0, viewportHeight);
    // A Tick event is dispatched and positionMenu() should be called.
    positionMenuCalled = false;
    mockTimer.dispatchEvent(Timer.TICK);
    assertTrue('positionMenu() should be called.', positionMenuCalled);
    style.setSize(footer, 1, 1);

    // Tear down.
    iframeDom.removeNode(p1);
    iframeDom.removeNode(p2);
    iframeDom.removeNode(p3);
    replacer.reset();
    button.dispose();
  },

  /**
   * Shows the menu and resizes the viewport, a timer corrects the menu
   * position. Before a bug was fixed, the menu position could be mispositioned
   * under some circumstances. Say that a menu button is placed close to the
   * right edge of the containing viewport, with a menu which is anchored to the
   * top right of the button. When the menu is open, the user decreases the
   * width of the viewport, say by zooming the browser window magnification. The
   * right edge effectively moves to the left. The button moves along with it,
   * and now close to the new right edge of the viewport, while the menu stays
   * where it is. But the browser now looks at the menu and finds that it no
   * longer fits in the allowable horizontal space, so it reflows the menu by
   * breaking a bunch of lines, making it narrower, and therefore taller. It's
   * now scrunched up against the right side of the viewport. At this point, the
   * MenuButton.onTick method runs, and repositions the menu. This is how it
   * works in all cases, but in this particular case, repositioning is
   * complicated by the fact that the menu is no longer the right shape; it is
   * narrower and taller than its natural size. When the correct position is
   * calculated, the size is used to determine the position, because the menu is
   * right- and bottom-aligned. When the code subtracts the width and height, it
   * winds up with a position which is not far enough to the left, and too high
   * up. The menu is moved to this new place, and under most circumstances the
   * browser now has space for the menu to return to its natural shape,
   * resulting in the menu appearing to detach from the menu button, moving up
   * and to the right. The bug fix was to detect when the viewport width is
   * decreasing, and insert an additional repositioning of the menu to
   * coordinates (0,0), giving enough room to lay out the menu properly, so the
   * correct size is available to determine the proper menu position.
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  testOpenedMenuPositionCorrection_viewportChange() {
    const iframe = dom.getElement('iframe1');
    const iframeDoc = dom.getFrameContentDocument(iframe);
    const iframeDom = dom.getDomHelper(iframeDoc);
    const iframeWindow = dom.getWindow(iframeDoc);

    const button = new MenuButton();
    button.setMenuPosition(new MenuAnchoredPosition(null, Corner.TOP_LEFT));
    iframeWindow.scrollTo(0, 0);
    const node = iframeDom.getElement('demoMenuButton2');
    button.decorate(node);
    const mockTimer = new Timer();
    // Don't start the timer.  We manually dispatch the Tick event.
    mockTimer.start = goog.nullFunction;
    /** @suppress {visibility} suppression added to enable type checking */
    button.timer_ = mockTimer;

    const replacer = new PropertyReplacer();
    let positionMenuCalled;
    const origPositionMenu = goog.bind(button.positionMenu, button);
    replacer.set(button, 'positionMenu', () => {
      positionMenuCalled = true;
      origPositionMenu();
    });

    // Show the menu.
    button.setOpen(true);

    // Confirm the menu position
    const menuNode = iframeDom.getElement('demoMenu2');
    assertRoughlyEquals(
        menuNode.offsetTop + menuNode.offsetHeight, node.offsetTop, 20);
    assertRoughlyEquals(menuNode.offsetLeft, node.offsetLeft, 20);

    positionMenuCalled = false;
    // A Tick event is dispatched.
    mockTimer.dispatchEvent(Timer.TICK);
    assertFalse('positionMenu() shouldn\'t be called.', positionMenuCalled);

    // Reduce the size of the enclosing element.
    iframe.style.width = '300px';

    // Confirm the menu is detached from the button.
    assertTrue(
        (Math.abs(node.offsetTop + node.offsetHeight - menuNode.offsetTop) >
         20) ||
        (Math.abs(node.offsetLeft - menuNode.offsetLeft) > 20));

    positionMenuCalled = false;
    // A Tick event is dispatched.
    mockTimer.dispatchEvent(Timer.TICK);
    assertTrue('positionMenu() should be called.', positionMenuCalled);

    // The menu is moved to appropriate position again.
    assertRoughlyEquals(
        menuNode.offsetTop + menuNode.offsetHeight, node.offsetTop, 20);
    assertRoughlyEquals(menuNode.offsetLeft, node.offsetLeft, 20);

    // Tear down.
    replacer.reset();
    button.dispose();
  },

  /**
   * Use a different button to position the menu and make sure it does so
   * correctly.
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  testAlternatePositioningElement() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);

    const posElement = dom.getElement('positionElement');
    menuButton.setPositionElement(posElement);

    // Show the menu.
    menuButton.setOpen(true);

    // Confirm the menu position
    const menuNode = menuButton.getMenu().getElement();
    assertRoughlyEquals(
        menuNode.offsetTop, posElement.offsetTop + posElement.offsetHeight, 20);
    assertRoughlyEquals(menuNode.offsetLeft, posElement.offsetLeft, 20);
  },

  /**
     Test forced positioning above the button.
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testPositioningAboveAnchor() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);

    // Show the menu.
    menuButton.setAlignMenuToStart(true);  // Should get overridden below
    menuButton.setScrollOnOverflow(true);  // Should get overridden below

    const position = new MenuAnchoredPosition(
        menuButton.getElement(), Corner.TOP_START,
        /* opt_adjust */ false, /* opt_resize */ false);
    menuButton.setMenuPosition(position);
    menuButton.setOpen(true);

    // Confirm the menu position
    const buttonBounds = style.getBounds(node);
    const menuNode = menuButton.getMenu().getElement();
    const menuBounds = style.getBounds(menuNode);

    assertRoughlyEquals(
        menuBounds.top + menuBounds.height, buttonBounds.top, 3);
    assertRoughlyEquals(menuBounds.left, buttonBounds.left, 3);
    // For this test to be valid, the node must have non-trival height.
    assertRoughlyEquals(node.offsetHeight, 19, 3);
  },

  /**
     Test forced positioning below the button.
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testPositioningBelowAnchor() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);

    // Show the menu.
    menuButton.setAlignMenuToStart(true);  // Should get overridden below
    menuButton.setScrollOnOverflow(true);  // Should get overridden below

    const position = new MenuAnchoredPosition(
        menuButton.getElement(), Corner.BOTTOM_START,
        /* opt_adjust */ false, /* opt_resize */ false);
    menuButton.setMenuPosition(position);
    menuButton.setOpen(true);

    // Confirm the menu position
    const buttonBounds = style.getBounds(node);
    const menuNode = menuButton.getMenu().getElement();
    const menuBounds = style.getBounds(menuNode);

    expectedFailures.expectFailureFor(isWinSafariBefore5());
    try {
      assertRoughlyEquals(
          menuBounds.top, buttonBounds.top + buttonBounds.height, 3);
      assertRoughlyEquals(menuBounds.left, buttonBounds.left, 3);
    } catch (e) {
      expectedFailures.handleException(e);
    }
    // For this test to be valid, the node must have non-trival height.
    assertRoughlyEquals(node.offsetHeight, 19, 3);
  },

  /**
     Tests that space, and only space, fire on key up.
     @suppress {missingProperties} suppression added to enable type checking
   */
  testSpaceFireOnKeyUp() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);

    let e = new GoogEvent(KeyHandler.EventType.KEY, menuButton);
    e.preventDefault = recordFunction();
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.SPACE;
    menuButton.handleKeyEvent(e);
    assertFalse(
        'Menu must not have been triggered by Space keypress',
        menuButton.isOpen());
    assertNotNull(
        'Page scrolling is prevented', e.preventDefault.getLastCall());

    e = new GoogEvent(EventType.KEYUP, menuButton);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.SPACE;
    menuButton.handleKeyEvent(e);
    assertTrue(
        'Menu must have been triggered by Space keyup', menuButton.isOpen());
    menuButton.getMenu().setHighlightedIndex(0);
    e = new GoogEvent(KeyHandler.EventType.KEY, menuButton);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.DOWN;
    menuButton.handleKeyEvent(e);
    assertEquals(
        'Highlighted menu item must have hanged by Down keypress', 1,
        menuButton.getMenu().getHighlightedIndex());

    menuButton.getMenu().setHighlightedIndex(0);
    e = new GoogEvent(EventType.KEYUP, menuButton);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    e.keyCode = KeyCodes.DOWN;
    menuButton.handleKeyEvent(e);
    assertEquals(
        'Highlighted menu item must not have changed by Down keyup', 0,
        menuButton.getMenu().getHighlightedIndex());
  },

  /**
   * Tests that preventing the button from closing also prevents the menu from
   * being hidden.
   */
  testPreventHide() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    menuButton.setDispatchTransitionEvents(Component.State.OPENED, true);

    // Show the menu.
    menuButton.setOpen(true);
    assertTrue('Menu button should be open.', menuButton.isOpen());
    assertTrue('Menu should be visible.', menuButton.getMenu().isVisible());

    const key =
        events.listen(menuButton, Component.EventType.CLOSE, (event) => {
          event.preventDefault();
        });

    // Try to hide the menu.
    menuButton.setOpen(false);
    assertTrue('Menu button should still be open.', menuButton.isOpen());
    assertTrue(
        'Menu should still be visible.', menuButton.getMenu().isVisible());

    // Remove listener and try again.
    events.unlistenByKey(key);
    menuButton.setOpen(false);
    assertFalse('Menu button should not be open.', menuButton.isOpen());
    assertFalse(
        'Menu should not be visible.', menuButton.getMenu().isVisible());
  },

  /**
   * Tests that opening and closing the menu does not affect how adding or
   * removing menu items changes the size of the menu.
   */
  testResizeOnItemAddOrRemove() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    const menu = menuButton.getMenu();

    // Show the menu.
    menuButton.setOpen(true);
    const originalSize = style.getSize(menu.getElement());

    // Check that removing an item while the menu is left open correctly changes
    // the size of the menu.
    // Remove an item using a method on Menu.
    const item = menu.removeChildAt(0, true);
    // Confirm size of menu changed.
    const afterRemoveSize = style.getSize(menu.getElement());
    assertTrue(
        'Height of menu must decrease after removing a menu item.',
        afterRemoveSize.height < originalSize.height);

    // Check that removing an item while the menu is closed, then opened
    // (so that reposition is called) correctly changes the size of the menu.
    // Hide menu.
    menuButton.setOpen(false);
    const item2 = menu.removeChildAt(0, true);
    menuButton.setOpen(true);
    // Confirm size of menu changed.
    const afterRemoveAgainSize = style.getSize(menu.getElement());
    assertTrue(
        'Height of menu must decrease after removing a second menu item.',
        afterRemoveAgainSize.height < afterRemoveSize.height);

    // Check that adding an item while the menu is opened, then closed, then
    // opened, correctly changes the size of the menu.
    // Add an item, this time using a MenuButton method.
    menuButton.setOpen(true);
    menuButton.addItem(item2);
    menuButton.setOpen(false);
    menuButton.setOpen(true);
    // Confirm size of menu changed.
    const afterAddSize = style.getSize(menu.getElement());
    assertTrue(
        'Height of menu must increase after adding a menu item.',
        afterRemoveAgainSize.height < afterAddSize.height);
    assertEquals(
        'Removing and adding back items must not change the height of a menu.',
        afterRemoveSize.height, afterAddSize.height);

    // Add back the last item to keep state consistent.
    menuButton.addItem(item);
  },

  /**
   * Tests that adding and removing items from a menu with scrollOnOverflow is
   * on correctly resizes the menu.
   */
  testResizeOnItemAddOrRemoveWithScrollOnOverflow() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    const menu = menuButton.getMenu();

    // Show the menu.
    menuButton.setScrollOnOverflow(true);
    menuButton.setOpen(true);
    const originalSize = style.getSize(menu.getElement());

    // Check that removing an item while the menu is left open correctly changes
    // the size of the menu.
    // Remove an item using a method on Menu.
    const item = menu.removeChildAt(0, true);
    menuButton.invalidateMenuSize();
    menuButton.positionMenu();

    // Confirm size of menu changed.
    const afterRemoveSize = style.getSize(menu.getElement());
    assertTrue(
        'Height of menu must decrease after removing a menu item.',
        afterRemoveSize.height < originalSize.height);

    const item2 = menu.removeChildAt(0, true);
    menuButton.invalidateMenuSize();
    menuButton.positionMenu();

    // Confirm size of menu changed.
    const afterRemoveAgainSize = style.getSize(menu.getElement());
    assertTrue(
        'Height of menu must decrease after removing a second menu item.',
        afterRemoveAgainSize.height < afterRemoveSize.height);

    // Check that adding an item while the menu is opened correctly changes the
    // size of the menu.
    menuButton.addItem(item2);
    menuButton.invalidateMenuSize();
    menuButton.positionMenu();

    // Confirm size of menu changed.
    const afterAddSize = style.getSize(menu.getElement());
    assertTrue(
        'Height of menu must increase after adding a menu item.',
        afterRemoveAgainSize.height < afterAddSize.height);
    assertEquals(
        'Removing and adding back items must not change the height of a menu.',
        afterRemoveSize.height, afterAddSize.height);
  },

  /**
   * Try rendering the menu as a sibling rather than as a child of the dom. This
   * tests the case when the button is rendered, rather than decorated.
   */
  testRenderMenuAsSibling() {
    menuButton.setRenderMenuAsSibling(true);
    menuButton.addItem(new MenuItem('Menu item 1'));
    menuButton.addItem(new MenuItem('Menu item 2'));
    // By default the menu is rendered into the top level dom and the button
    // is rendered into whatever parent we provide.  If we don't provide a
    // parent then we aren't really testing anything, since both would be, by
    // default, rendered into the top level dom, and therefore siblings.
    menuButton.render(dom.getElement('siblingTest'));
    menuButton.setOpen(true);
    assertEquals(
        menuButton.getElement().parentNode,
        menuButton.getMenu().getElement().parentNode);
  },

  /**
   * Check that we render the menu as a sibling of the menu button, immediately
   * after the menu button.
   */
  testRenderMenuAsSiblingForDecoratedButton() {
    const menu = new Menu();
    menu.addChild(new MenuItem('Menu item 1'), true /* render */);
    menu.addChild(new MenuItem('Menu item 2'), true /* render */);
    menu.addChild(new MenuItem('Menu item 3'), true /* render */);

    const menuButton = new MenuButton();
    menuButton.setMenu(menu);
    menuButton.setRenderMenuAsSibling(true);
    const node = dom.getElement('button1');
    menuButton.decorate(node);

    menuButton.setOpen(true);

    assertEquals(
        'The menu should be rendered immediately after the menu button',
        dom.getNextElementSibling(menuButton.getElement()), menu.getElement());

    assertEquals(
        'The menu should be rendered immediately before the next button',
        dom.getNextElementSibling(menu.getElement()),
        dom.getElement('button2'));
  },

  testAlignToStartSetter() {
    assertTrue(menuButton.isAlignMenuToStart());

    menuButton.setAlignMenuToStart(false);
    assertFalse(menuButton.isAlignMenuToStart());

    menuButton.setAlignMenuToStart(true);
    assertTrue(menuButton.isAlignMenuToStart());
  },

  testScrollOnOverflowSetter() {
    assertFalse(menuButton.isScrollOnOverflow());

    menuButton.setScrollOnOverflow(true);
    assertTrue(menuButton.isScrollOnOverflow());

    menuButton.setScrollOnOverflow(false);
    assertFalse(menuButton.isScrollOnOverflow());
  },

  /**
   * Tests that the attached menu has been set to aria-hidden=false explicitly
   * when the menu is opened.
   */
  testSetOpenUnsetsAriaHidden() {
    const node = dom.getElement('demoMenuButton');
    menuButton.decorate(node);
    const menuElem = menuButton.getMenu().getElementStrict();
    aria.setState(menuElem, State.HIDDEN, true);
    menuButton.setOpen(true);
    assertEquals('', aria.getState(menuElem, State.HIDDEN));
  },
});
