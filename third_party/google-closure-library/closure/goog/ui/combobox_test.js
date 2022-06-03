/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ComboBoxTest');
goog.setTestOnly();

const ComboBox = goog.require('goog.ui.ComboBox');
const ComboBoxItem = goog.require('goog.ui.ComboBoxItem');
const Component = goog.require('goog.ui.Component');
const ControlRenderer = goog.require('goog.ui.ControlRenderer');
const KeyCodes = goog.require('goog.events.KeyCodes');
const LabelInput = goog.require('goog.ui.LabelInput');
const Menu = goog.require('goog.ui.Menu');
const MenuItem = goog.require('goog.ui.MenuItem');
const MockClock = goog.require('goog.testing.MockClock');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');

let comboBox;
let input;

testSuite({
  setUp() {
    dom.removeChildren(dom.getElement('combo'));

    comboBox = new ComboBox();
    comboBox.setDefaultText('Select a color...');
    comboBox.addItem(new ComboBoxItem('Red'));
    comboBox.addItem(new ComboBoxItem('Maroon'));
    comboBox.addItem(new ComboBoxItem('Gre<en'));
    comboBox.addItem(new ComboBoxItem('Blue'));
    comboBox.addItem(new ComboBoxItem('Royal Blue'));
    comboBox.addItem(new ComboBoxItem('Yellow'));
    comboBox.addItem(new ComboBoxItem('Magenta'));
    comboBox.addItem(new ComboBoxItem('Mouve'));
    comboBox.addItem(new ComboBoxItem('Grey'));
    comboBox.render(dom.getElement('combo'));

    input = comboBox.getInputElement();
  },

  tearDown() {
    comboBox.dispose();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testInputElementAttributes() {
    const comboBox = new ComboBox();
    comboBox.setFieldName('a_form_field');
    comboBox.createDom();
    const inputElement = comboBox.getInputElement();
    assertEquals('text', inputElement.type);
    assertEquals('a_form_field', inputElement.name);
    assertEquals('off', inputElement.autocomplete);
    comboBox.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetDefaultText() {
    assertEquals('Select a color...', comboBox.getDefaultText());
    comboBox.setDefaultText('new default text...');
    assertEquals('new default text...', comboBox.getDefaultText());
    assertEquals('new default text...', comboBox.labelInput_.getLabel());
  },

  testGetMenu() {
    assertTrue(
        'Menu should be instance of goog.ui.Menu',
        comboBox.getMenu() instanceof Menu);
    assertEquals(
        'Menu should have correct number of children', 9,
        comboBox.getMenu().getChildCount());
  },

  testMenuBeginsInvisible() {
    assertFalse('Menu should begin invisible', comboBox.getMenu().isVisible());
  },

  testClickCausesPopup() {
    events.fireClickSequence(input);
    assertTrue(
        'Menu becomes visible after click', comboBox.getMenu().isVisible());
  },

  testUpKeyCausesPopup() {
    events.fireKeySequence(input, KeyCodes.UP);
    assertTrue(
        'Menu becomes visible after UP key', comboBox.getMenu().isVisible());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testActionSelectsItem() {
    comboBox.getMenu().getItemAt(2).dispatchEvent(Component.EventType.ACTION);
    assertEquals('Gre<en', input.value);
  },

  testActionSelectsItemWithModel() {
    const itemWithModel = new MenuItem('one', 1);
    comboBox.addItem(itemWithModel);
    itemWithModel.dispatchEvent(Component.EventType.ACTION);
    assertEquals('one', comboBox.getValue());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRedisplayMenuAfterBackspace() {
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    input.value = 'mx';
    comboBox.onInputEvent_();
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    input.value = 'm';
    comboBox.onInputEvent_();
    assertEquals(
        'Three items should be displayed', 3,
        comboBox.getNumberOfVisibleItems_());
  },

  testExternallyCreatedMenu() {
    const menu = new Menu();
    menu.decorate(dom.getElement('menu'));
    assertTrue(
        'Menu items should be instances of goog.ui.ComboBoxItem',
        menu.getChildAt(0) instanceof ComboBoxItem);

    comboBox = new ComboBox(null, menu);
    comboBox.render(dom.getElement('combo'));

    /** @suppress {checkTypes} suppression added to enable type checking */
    input = dom.getElementsByTagName(TagName.INPUT, comboBox.getElement())[0];
    menu.getItemAt(2).dispatchEvent(Component.EventType.ACTION);
    assertEquals('Blue', input.value);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRecomputeVisibleCountAfterChangingItems() {
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    input.value = 'Black';
    comboBox.onInputEvent_();
    assertEquals(
        'No items should be displayed', 0, comboBox.getNumberOfVisibleItems_());
    comboBox.addItem(new ComboBoxItem('Black'));
    assertEquals(
        'One item should be displayed', 1, comboBox.getNumberOfVisibleItems_());

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    input.value = 'Red';
    comboBox.onInputEvent_();
    assertEquals(
        'One item should be displayed', 1, comboBox.getNumberOfVisibleItems_());
    comboBox.removeItemAt(0);  // Red
    assertEquals(
        'No items should be displayed', 0, comboBox.getNumberOfVisibleItems_());
  },

  /**
     @suppress {strictMissingProperties,checkTypes} suppression added to enable
     type checking
   */
  testSetEnabled() {
    // By default, everything should be enabled.
    assertFalse('Text input should initially not be disabled', input.disabled);
    assertFalse(
        'Text input should initially not look disabled',
        classlist.contains(
            input,
            goog.getCssName(
                LabelInput.prototype.labelCssClassName, 'disabled')));
    assertFalse(
        'Combo box should initially not look disabled',
        classlist.contains(
            comboBox.getElement(), goog.getCssName('goog-combobox-disabled')));
    events.fireClickSequence(comboBox.getElement());
    assertTrue(
        'Menu initially becomes visible after click',
        comboBox.getMenu().isVisible());
    events.fireClickSequence(document);
    assertFalse(
        'Menu initially becomes invisible after document click',
        comboBox.getMenu().isVisible());

    assertTrue(comboBox.isEnabled());
    comboBox.setEnabled(false);
    assertFalse(comboBox.isEnabled());
    assertTrue(
        'Text input should be disabled after being disabled', input.disabled);
    assertTrue(
        'Text input should appear disabled after being disabled',
        classlist.contains(
            input,
            goog.getCssName(
                LabelInput.prototype.labelCssClassName, 'disabled')));
    assertTrue(
        'Combo box should appear disabled after being disabled',
        classlist.contains(
            comboBox.getElement(), goog.getCssName('goog-combobox-disabled')));
    events.fireClickSequence(comboBox.getElement());
    assertFalse(
        'Menu should not become visible after click if disabled',
        comboBox.getMenu().isVisible());

    comboBox.setEnabled(true);
    assertTrue(comboBox.isEnabled());
    assertFalse(
        'Text input should not be disabled after being re-enabled',
        input.disabled);
    assertFalse(
        'Text input should not appear disabled after being re-enabled',
        classlist.contains(
            input,
            goog.getCssName(
                LabelInput.prototype.labelCssClassName, 'disabled')));
    assertFalse(
        'Combo box should not appear disabled after being re-enabled',
        classlist.contains(
            comboBox.getElement(), goog.getCssName('goog-combobox-disabled')));
    events.fireClickSequence(comboBox.getElement());
    assertTrue(
        'Menu becomes visible after click when re-enabled',
        comboBox.getMenu().isVisible());
    events.fireClickSequence(document);
    assertFalse(
        'Menu becomes invisible after document click when re-enabled',
        comboBox.getMenu().isVisible());
  },

  testSetFormatFromToken() {
    const item = new ComboBoxItem('ABc');
    item.setFormatFromToken('b');
    const div = dom.createDom(TagName.DIV);
    new ControlRenderer().setContent(div, item.getContent());
    assertTrue(div.innerHTML == 'A<b>B</b>c' || div.innerHTML == 'A<B>B</B>c');
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetValue() {
    const clock = new MockClock(/* autoInstall */ true);

    // Get the input focus. Note that both calls are needed to correctly
    // simulate the focus (and setting document.activeElement) across all
    // browsers.
    input.focus();
    events.fireClickSequence(input);

    // Simulate text input.
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    input.value = 'Black';
    comboBox.onInputEvent_();
    clock.tick();
    assertEquals(
        'No items should be displayed', 0, comboBox.getNumberOfVisibleItems_());
    assertFalse('Menu should be invisible', comboBox.getMenu().isVisible());

    // Programmatic change with the input focus causes the menu visibility to
    // change if needed.
    comboBox.setValue('Blue');
    clock.tick();
    assertTrue('Menu should be visible1', comboBox.getMenu().isVisible());
    assertEquals(
        'One item should be displayed', 1, comboBox.getNumberOfVisibleItems_());

    // Simulate user input to ensure all the items are invisible again, then
    // blur away.
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    input.value = 'Black';
    comboBox.onInputEvent_();
    clock.tick();
    input.blur();
    document.body.focus();
    clock.tick(ComboBox.BLUR_DISMISS_TIMER_MS);
    assertEquals(
        'No items should be displayed', 0, comboBox.getNumberOfVisibleItems_());
    assertFalse('Menu should be invisible', comboBox.getMenu().isVisible());

    // Programmatic change without the input focus does not pop up the menu,
    // but still updates the list of visible items within it.
    comboBox.setValue('Blue');
    clock.tick();
    assertFalse('Menu should be invisible', comboBox.getMenu().isVisible());
    assertEquals(
        'Menu should contain one item', 1, comboBox.getNumberOfVisibleItems_());

    // Click on the combobox. The entire menu becomes visible, the last item
    // (programmatically) set is highlighted.
    events.fireClickSequence(comboBox.getElement());
    assertTrue('Menu should be visible2', comboBox.getMenu().isVisible());
    assertEquals(
        'All items should be displayed', comboBox.getMenu().getItemCount(),
        comboBox.getNumberOfVisibleItems_());
    assertEquals(
        'The last item set should be highlighted',
        /* Blue= */ 3, comboBox.getMenu().getHighlightedIndex());

    clock.uninstall();
  },
});
