/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.MenuItemTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const Coordinate = goog.require('goog.math.Coordinate');
const KeyCodes = goog.require('goog.events.KeyCodes');
const MenuItem = goog.require('goog.ui.MenuItem');
const MenuItemRenderer = goog.require('goog.ui.MenuItemRenderer');
const NodeType = goog.require('goog.dom.NodeType');
const Role = goog.require('goog.a11y.aria.Role');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const googArray = goog.require('goog.array');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');

let sandbox;
let item;

testSuite({
  setUp() {
    sandbox = dom.getElement('sandbox');
    item = new MenuItem('Item');
  },

  tearDown() {
    item.dispose();
    dom.removeChildren(sandbox);
  },

  testMenuItem() {
    assertNotNull('Instance must not be null', item);
    assertEquals(
        'Renderer must default to MenuItemRenderer singleton',
        MenuItemRenderer.getInstance(), item.getRenderer());
    assertEquals('Content must have expected value', 'Item', item.getContent());
    assertEquals(
        'Caption must default to the content', item.getContent(),
        item.getCaption());
    assertEquals(
        'Value must default to the caption', item.getCaption(),
        item.getValue());
  },

  testMenuItemConstructor() {
    const model = 'Hello';
    const fakeDom = {};
    const fakeRenderer = {};

    /** @suppress {checkTypes} suppression added to enable type checking */
    const menuItem = new MenuItem('Item', model, fakeDom, fakeRenderer);
    assertEquals(
        'Content must have expected value', 'Item', menuItem.getContent());
    assertEquals(
        'Caption must default to the content', menuItem.getContent(),
        menuItem.getCaption());
    assertEquals('Model must be set', model, menuItem.getModel());
    assertNotEquals(
        'Value must not equal the caption', menuItem.getCaption(),
        menuItem.getValue());
    assertEquals('Value must equal the model', model, menuItem.getValue());
    assertEquals('DomHelper must be set', fakeDom, menuItem.getDomHelper());
    assertEquals('Renderer must be set', fakeRenderer, menuItem.getRenderer());
  },

  testGetValue() {
    assertUndefined('Model must be undefined by default', item.getModel());
    assertEquals(
        'Without a model, value must default to the caption', item.getCaption(),
        item.getValue());
    item.setModel('Foo');
    assertEquals(
        'With a model, value must default to the model', item.getModel(),
        item.getValue());
  },

  testSetValue() {
    assertUndefined('Model must be undefined by default', item.getModel());
    assertEquals(
        'Without a model, value must default to the caption', item.getCaption(),
        item.getValue());
    item.setValue(17);
    assertEquals('Value must be set', 17, item.getValue());
    assertEquals(
        'Value and model must be the same', item.getValue(), item.getModel());
  },

  testGetSetContent() {
    assertEquals('Content must have expected value', 'Item', item.getContent());
    item.setContent(dom.createDom(TagName.DIV, 'foo', 'Foo'));
    assertEquals(
        'Content must be an element', NodeType.ELEMENT,
        item.getContent().nodeType);
    assertHTMLEquals(
        'Content must be the expected element', '<div class="foo">Foo</div>',
        dom.getOuterHtml(item.getContent()));
  },

  testGetSetCaption() {
    assertEquals('Caption must have expected value', 'Item', item.getCaption());
    item.setCaption('Hello, world!');
    assertTrue(
        'Caption must be a string', typeof item.getCaption() === 'string');
    assertEquals(
        'Caption must have expected value', 'Hello, world!', item.getCaption());
    item.setContent(dom.createDom(TagName.DIV, 'foo', 'Foo'));
    assertTrue(
        'Caption must be a string', typeof item.getCaption() === 'string');
    assertEquals('Caption must have expected value', 'Foo', item.getCaption());
  },

  testGetSetContentAfterCreateDom() {
    item.createDom();
    assertEquals('Content must have expected value', 'Item', item.getContent());
    item.setContent(dom.createDom(TagName.DIV, 'foo', 'Foo'));
    assertEquals(
        'Content must be an element', NodeType.ELEMENT,
        item.getContent().nodeType);
    assertHTMLEquals(
        'Content must be the expected element', '<div class="foo">Foo</div>',
        dom.getOuterHtml(item.getContent()));
  },

  testGetSetCaptionAfterCreateDom() {
    item.createDom();
    assertEquals('Caption must have expected value', 'Item', item.getCaption());
    item.setCaption('Hello, world!');
    assertTrue(
        'Caption must be a string', typeof item.getCaption() === 'string');
    assertEquals(
        'Caption must have expected value', 'Hello, world!', item.getCaption());
    item.setContent(dom.createDom(TagName.DIV, 'foo', 'Foo'));
    assertTrue(
        'Caption must be a string', typeof item.getCaption() === 'string');
    assertEquals('Caption must have expected value', 'Foo', item.getCaption());

    const arrayContent =
        googArray.clone(dom.safeHtmlToNode(testing.newSafeHtmlForTest(
                                               ' <b> \xa0foo</b><i>  bar</i> '))
                            .childNodes);
    item.setContent(arrayContent);
    assertEquals(
        'whitespaces must be normalized in the caption', '\xa0foo bar',
        item.getCaption());
  },

  testSetSelectable() {
    assertFalse(
        'Item must not be selectable by default',
        item.isSupportedState(Component.State.SELECTED));
    item.setSelectable(true);
    assertTrue(
        'Item must be selectable',
        item.isSupportedState(Component.State.SELECTED));
    item.setSelected(true);
    assertTrue('Item must be selected', item.isSelected());
    assertFalse('Item must not be checked', item.isChecked());
    item.setSelectable(false);
    assertFalse(
        'Item must not no longer be selectable',
        item.isSupportedState(Component.State.SELECTED));
    assertFalse('Item must no longer be selected', item.isSelected());
    assertFalse('Item must not be checked', item.isChecked());
  },

  testSetCheckable() {
    assertFalse(
        'Item must not be checkable by default',
        item.isSupportedState(Component.State.CHECKED));
    item.setCheckable(true);
    assertTrue(
        'Item must be checkable',
        item.isSupportedState(Component.State.CHECKED));
    item.setChecked(true);
    assertTrue('Item must be checked', item.isChecked());
    assertFalse('Item must not be selected', item.isSelected());
    item.setCheckable(false);
    assertFalse(
        'Item must not no longer be checkable',
        item.isSupportedState(Component.State.CHECKED));
    assertFalse('Item must no longer be checked', item.isChecked());
    assertFalse('Item must not be selected', item.isSelected());
  },

  testSetSelectableBeforeCreateDom() {
    item.setSelectable(true);
    item.createDom();
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    item.setSelectable(false);
    assertFalse(
        'Item must no longer have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
  },

  testSetCheckableBeforeCreateDom() {
    item.setCheckable(true);
    item.createDom();
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertEquals(
        'Element must have ARIA role menuitemcheckbox', Role.MENU_ITEM_CHECKBOX,
        aria.getRole(item.getElement()));
    item.setCheckable(false);
    assertFalse(
        'Item must no longer have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
  },

  testSetSelectableAfterCreateDom() {
    item.createDom();
    item.setSelectable(true);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertEquals(
        'Element must have ARIA role menuitemradio', Role.MENU_ITEM_RADIO,
        aria.getRole(item.getElement()));
    item.setSelectable(false);
    assertFalse(
        'Item must no longer have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
  },

  testSetCheckableAfterCreateDom() {
    item.createDom();
    item.setCheckable(true);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    item.setCheckable(false);
    assertFalse(
        'Item must no longer have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSelectableBehavior() {
    item.setSelectable(true);
    item.render(sandbox);
    assertFalse('Item must not be selected by default', item.isSelected());
    item.performActionInternal();
    assertTrue('Item must be selected', item.isSelected());
    item.performActionInternal();
    assertTrue('Item must still be selected', item.isSelected());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testCheckableBehavior() {
    item.setCheckable(true);
    item.render(sandbox);
    assertFalse('Item must not be checked by default', item.isChecked());
    item.performActionInternal();
    assertTrue('Item must be checked', item.isChecked());
    item.performActionInternal();
    assertFalse('Item must no longer be checked', item.isChecked());
  },

  testGetSetContentForItemWithCheckBox() {
    item.setSelectable(true);
    item.createDom();

    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertEquals(
        'getContent() must not return the checkbox structure', 'Item',
        item.getContent());

    item.setContent('Hello');
    assertEquals(
        'getContent() must not return the checkbox structure', 'Hello',
        item.getContent());
    assertTrue(
        'Item must still have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));

    item.setContent(dom.createDom(TagName.SPAN, 'foo', 'Foo'));
    assertEquals(
        'getContent() must return element', NodeType.ELEMENT,
        item.getContent().nodeType);
    assertTrue(
        'Item must still have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));

    item.setContent(null);
    assertNull('getContent() must return null', item.getContent());
    assertTrue(
        'Item must still have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
  },

  testGetSetCaptionForItemWithCheckBox() {
    item.setCheckable(true);
    item.createDom();

    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertEquals(
        'getCaption() must not return the checkbox structure', 'Item',
        item.getCaption());

    item.setCaption('Hello');
    assertEquals(
        'getCaption() must not return the checkbox structure', 'Hello',
        item.getCaption());
    assertTrue(
        'Item must still have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));

    item.setContent(dom.createDom(TagName.SPAN, 'foo', 'Foo'));
    assertEquals(
        'getCaption() must return text content', 'Foo', item.getCaption());
    assertTrue(
        'Item must still have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));

    item.setCaption('');
    assertEquals(
        'getCaption() must return empty string', '', item.getCaption());
    assertTrue(
        'Item must still have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
  },

  testGetSetCaptionForItemWithAccelerators() {
    const contentArr = [];
    contentArr.push(dom.createDom(
        TagName.SPAN, goog.getCssName('goog-menuitem-accel'), 'Ctrl+1'));
    contentArr.push(dom.createTextNode('Hello'));
    item.setCaption(contentArr);
    assertEquals(
        'getCaption() must not return the accelerator', 'Hello',
        item.getCaption());

    item.setCaption([dom.createDom(
        TagName.SPAN, goog.getCssName('goog-menuitem-accel'), 'Ctrl+1')]);
    assertEquals(
        'getCaption() must return empty string', '', item.getCaption());

    assertEquals(
        'getAccelerator() should return the accelerator', 'Ctrl+1',
        item.getAccelerator());
  },

  testGetSetCaptionForItemWithMnemonics() {
    let contentArr = [];
    contentArr.push(dom.createDom(
        TagName.SPAN, goog.getCssName('goog-menuitem-mnemonic-hint'), 'H'));
    contentArr.push(dom.createTextNode('ello'));
    item.setCaption(contentArr);
    assertEquals(
        'getCaption() must not return hint markup', 'Hello', item.getCaption());

    contentArr = [];
    contentArr.push(dom.createTextNode('Hello'));
    contentArr.push(dom.createDom(
        TagName.SPAN, goog.getCssName('goog-menuitem-mnemonic-separator'), '(',
        dom.createDom(
            TagName.SPAN, goog.getCssName('goog-menuitem-mnemonic-hint'), 'J'),
        ')'));
    item.setCaption(contentArr);
    assertEquals(
        'getCaption() must not return the paranethetical mnemonic', 'Hello',
        item.getCaption());

    item.setCaption('');
    assertEquals(
        'getCaption() must return the empty string', '', item.getCaption());
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testHandleKeyEventInternalWithMnemonic() {
    /** @suppress {visibility} suppression added to enable type checking */
    item.performActionInternal = recordFunction(item.performActionInternal);
    item.setMnemonic(KeyCodes.F);
    item.handleKeyEventInternal({'keyCode': KeyCodes.F});
    assertEquals(
        'performActionInternal must be called', 1,
        item.performActionInternal.getCallCount());
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testHandleKeyEventInternalWithoutMnemonic() {
    /** @suppress {visibility} suppression added to enable type checking */
    item.performActionInternal = recordFunction(item.performActionInternal);
    item.handleKeyEventInternal({'keyCode': KeyCodes.F});
    assertEquals(
        'performActionInternal must not be called without a' +
            ' mnemonic',
        0, item.performActionInternal.getCallCount());
  },

  testRender() {
    item.render(sandbox);
    const contentElement = item.getContentElement();
    assertNotNull('Content element must exist', contentElement);
    assertTrue(
        'Content element must have expected class name',
        classlist.contains(
            contentElement,
            item.getRenderer().getStructuralCssClass() + '-content'));
    assertHTMLEquals(
        'Content element must have expected structure', 'Item',
        contentElement.innerHTML);
  },

  testRenderSelectableItem() {
    item.setSelectable(true);
    item.render(sandbox);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertEquals(
        'getCaption() return expected value', 'Item', item.getCaption());
  },

  testRenderSelectedItem() {
    item.setSelectable(true);
    item.setSelected(true);
    item.render(sandbox);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertTrue(
        'Item must have selected style',
        classlist.contains(
            item.getElement(),
            item.getRenderer().getClassForState(Component.State.SELECTED)));
  },

  testRenderCheckableItem() {
    item.setCheckable(true);
    item.render(sandbox);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertEquals(
        'getCaption() return expected value', 'Item', item.getCaption());
  },

  testRenderCheckedItem() {
    item.setCheckable(true);
    item.setChecked(true);
    item.render(sandbox);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertTrue(
        'Item must have checked style',
        classlist.contains(
            item.getElement(),
            item.getRenderer().getClassForState(Component.State.CHECKED)));
  },

  testDecorate() {
    sandbox.innerHTML = '<div id="foo">Foo</div>';
    const foo = dom.getElement('foo');
    item.decorate(foo);
    assertEquals(
        'Decorated element must be as expected', foo, item.getElement());
    assertTrue(
        'Decorated element must have expected class name',
        classlist.contains(
            item.getElement(), item.getRenderer().getCssClass()));
    assertEquals(
        'Content element must be the decorated element\'s child',
        item.getContentElement(), item.getElement().firstChild);
    assertHTMLEquals(
        'Content must have expected structure', 'Foo',
        item.getContentElement().innerHTML);
  },

  testDecorateCheckableItem() {
    sandbox.innerHTML = '<div id="foo" class="goog-option">Foo</div>';
    const foo = dom.getElement('foo');
    item.decorate(foo);
    assertEquals(
        'Decorated element must be as expected', foo, item.getElement());
    assertTrue(
        'Decorated element must have expected class name',
        classlist.contains(
            item.getElement(), item.getRenderer().getCssClass()));
    assertEquals(
        'Content element must be the decorated element\'s child',
        item.getContentElement(), item.getElement().firstChild);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertFalse('Item must not be checked', item.isChecked());
  },

  testDecorateCheckedItem() {
    sandbox.innerHTML =
        '<div id="foo" class="goog-option goog-option-selected">Foo</div>';
    const foo = dom.getElement('foo');
    item.decorate(foo);
    assertEquals(
        'Decorated element must be as expected', foo, item.getElement());
    assertSameElements(
        'Decorated element must have expected class names',
        ['goog-menuitem', 'goog-option', 'goog-option-selected'],
        classlist.get(item.getElement()));
    assertEquals(
        'Content element must be the decorated element\'s child',
        item.getContentElement(), item.getElement().firstChild);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertTrue('Item must be checked', item.isChecked());
  },

  testDecorateTemplate() {
    sandbox.innerHTML = '<div id="foo" class="goog-menuitem">' +
        '<div class="goog-menuitem-content">Foo</div></div>';
    const foo = dom.getElement('foo');
    item.decorate(foo);
    assertEquals(
        'Decorated element must be as expected', foo, item.getElement());
    assertTrue(
        'Decorated element must have expected class name',
        classlist.contains(
            item.getElement(), item.getRenderer().getCssClass()));
    assertEquals(
        'Content element must be the decorated element\'s child',
        item.getContentElement(), item.getElement().firstChild);
    assertHTMLEquals(
        'Content must have expected structure', 'Foo',
        item.getContentElement().innerHTML);
  },

  testDecorateCheckableItemTemplate() {
    sandbox.innerHTML = '<div id="foo" class="goog-menuitem goog-option">' +
        '<div class="goog-menuitem-content">' +
        '<div class="goog-menuitem-checkbox"></div>' +
        'Foo</div></div>';
    const foo = dom.getElement('foo');
    item.decorate(foo);
    assertEquals(
        'Decorated element must be as expected', foo, item.getElement());
    assertTrue(
        'Decorated element must have expected class name',
        classlist.contains(
            item.getElement(), item.getRenderer().getCssClass()));
    assertEquals(
        'Content element must be the decorated element\'s child',
        item.getContentElement(), item.getElement().firstChild);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertEquals(
        'Item must have exactly one checkbox structure', 1,
        dom.getElementsByTagNameAndClass(
               TagName.DIV, 'goog-menuitem-checkbox', item.getElement())
            .length);
    assertFalse('Item must not be checked', item.isChecked());
  },

  testDecorateCheckedItemTemplate() {
    sandbox.innerHTML = '<div id="foo" ' +
        'class="goog-menuitem goog-option goog-option-selected">' +
        '<div class="goog-menuitem-content">' +
        '<div class="goog-menuitem-checkbox"></div>' +
        'Foo</div></div>';
    const foo = dom.getElement('foo');
    item.decorate(foo);
    assertEquals(
        'Decorated element must be as expected', foo, item.getElement());
    assertSameElements(
        'Decorated element must have expected class names',
        ['goog-menuitem', 'goog-option', 'goog-option-selected'],
        classlist.get(item.getElement()));
    assertEquals(
        'Content element must be the decorated element\'s child',
        item.getContentElement(), item.getElement().firstChild);
    assertTrue(
        'Item must have checkbox structure',
        item.getRenderer().hasCheckBoxStructure(item.getElement()));
    assertEquals(
        'Item must have exactly one checkbox structure', 1,
        dom.getElementsByTagNameAndClass(
               TagName.DIV, 'goog-menuitem-checkbox', item.getElement())
            .length);
    assertTrue('Item must be checked', item.isChecked());
  },

  /** @bug 1463524 */
  testHandleMouseUp() {
    const COORDS_1 = new Coordinate(1, 1);
    const COORDS_2 = new Coordinate(2, 2);
    item.setActive(true);
    // Override performActionInternal() for testing purposes.
    let actionPerformed;
    /** @suppress {visibility} suppression added to enable type checking */
    item.performActionInternal = () => {
      actionPerformed = true;
      return true;
    };
    item.render(sandbox);

    // Scenario 1: item has no parent.
    actionPerformed = false;
    item.setActive(true);
    events.fireMouseUpEvent(item.getElement());
    assertTrue('Action should be performed on mouseup', actionPerformed);

    // Scenario 2: item has a parent.
    actionPerformed = false;
    item.setActive(true);
    const parent = new Component();
    const parentElem = dom.getElement('parentComponent');
    parent.render(parentElem);
    parent.addChild(item);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    parent.openingCoords = COORDS_1;
    events.fireMouseUpEvent(item.getElement(), undefined, COORDS_2);
    assertTrue('Action should be performed on mouseup', actionPerformed);

    // Scenario 3: item has a parent which was opened during mousedown, and
    // item, and now the mouseup fires at the same coords.
    actionPerformed = false;
    item.setActive(true);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    parent.openingCoords = COORDS_2;
    events.fireMouseUpEvent(item.getElement(), undefined, COORDS_2);
    assertFalse('Action should not be performed on mouseup', actionPerformed);
  },

  testSetAriaLabel() {
    assertNull('Item must not have aria label by default', item.getAriaLabel());
    item.setAriaLabel('Item 1');
    item.render(sandbox);
    const el = item.getElementStrict();
    assertEquals(
        'Item element must have expected aria-label', 'Item 1',
        el.getAttribute('aria-label'));
    assertEquals(
        'Item element must have expected aria-role', 'menuitem',
        el.getAttribute('role'));
    item.setAriaLabel('Item 2');
    assertEquals(
        'Item element must have updated aria-label', 'Item 2',
        el.getAttribute('aria-label'));
    assertEquals(
        'Item element must have expected aria-role', 'menuitem',
        el.getAttribute('role'));
  },
});
