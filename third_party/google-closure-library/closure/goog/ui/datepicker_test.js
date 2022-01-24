/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} Overriding goog.i18n.DateTimeSymbols
 */

goog.module('goog.ui.DatePickerTest');
goog.setTestOnly();

const DateDate = goog.require('goog.date.Date');
const DatePicker = goog.require('goog.ui.DatePicker');
const DateRange = goog.require('goog.date.DateRange');
/** @suppress {extraRequire} */
const DateTimeSymbols = goog.require('goog.i18n.DateTimeSymbols');
const DateTimeSymbols_en_US = goog.require('goog.i18n.DateTimeSymbols_en_US');
const DateTimeSymbols_zh_HK = goog.require('goog.i18n.DateTimeSymbols_zh_HK');
const KeyCodes = goog.require('goog.events.KeyCodes');
const Role = goog.require('goog.a11y.aria.Role');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const recordFunction = goog.require('goog.testing.recordFunction');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let picker;
const $$ = dom.getElementsByTagNameAndClass;
let sandbox;

testSuite({
  setUpPage() {
    sandbox = dom.getElement('sandbox');

    // Set the current date to a constant.
    Date.now = () => +new Date(2017, 9, 17);
  },

  tearDown() {
    picker.dispose();
    dom.removeChildren(sandbox);
  },

  testIsMonthOnLeft() {
    goog.i18n.DateTimeSymbols = DateTimeSymbols_en_US;
    picker = new DatePicker();
    picker.create(sandbox);
    const head = $$('tr', 'goog-date-picker-head')[0];
    /** @suppress {checkTypes} suppression added to enable type checking */
    const month = $$('button', 'goog-date-picker-month', head.firstChild)[0];
    assertSameElements(
        'Button element must have expected class names',
        ['goog-date-picker-btn', 'goog-date-picker-month'],
        classlist.get(month));
  },

  testIsYearOnLeft() {
    goog.i18n.DateTimeSymbols = DateTimeSymbols_zh_HK;
    picker = new DatePicker();
    picker.create(sandbox);
    const head = $$('tr', 'goog-date-picker-head')[0];
    /** @suppress {checkTypes} suppression added to enable type checking */
    const year = $$('button', 'goog-date-picker-year', head.firstChild)[0];
    assertSameElements(
        'Button element must have expected class names',
        ['goog-date-picker-btn', 'goog-date-picker-year'], classlist.get(year));
  },

  testHidingOfTableFoot0() {
    picker = new DatePicker();
    picker.setAllowNone(false);
    picker.setShowToday(false);
    picker.create(sandbox);
    const tFoot = $$('tfoot')[0];
    assertFalse(style.isElementShown(tFoot));
  },

  testHidingOfTableFoot1() {
    picker = new DatePicker();
    picker.setAllowNone(false);
    picker.setShowToday(true);
    picker.create(sandbox);
    const tFoot = $$('tfoot')[0];
    assertTrue(style.isElementShown(tFoot));
  },

  testHidingOfTableFoot2() {
    picker = new DatePicker();
    picker.setAllowNone(true);
    picker.setShowToday(false);
    picker.create(sandbox);
    const tFoot = $$('tfoot')[0];
    assertTrue(style.isElementShown(tFoot));
  },

  testHidingOfTableFoot3() {
    picker = new DatePicker();
    picker.setAllowNone(true);
    picker.setShowToday(true);
    picker.create(sandbox);
    const tFoot = $$('tfoot')[0];
    assertTrue(style.isElementShown(tFoot));
  },

  testHidingOfTableFootAfterCreate0() {
    picker = new DatePicker();
    picker.create(sandbox);
    picker.setAllowNone(false);
    picker.setShowToday(false);
    const tFoot = $$('tfoot')[0];
    assertFalse(style.isElementShown(tFoot));
  },

  testHidingOfTableFootAfterCreate1() {
    picker = new DatePicker();
    picker.create(sandbox);
    picker.setAllowNone(false);
    picker.setShowToday(true);
    const tFoot = $$('tfoot')[0];
    assertTrue(style.isElementShown(tFoot));
  },

  testHidingOfTableFootAfterCreate2() {
    picker = new DatePicker();
    picker.create(sandbox);
    picker.setAllowNone(true);
    picker.setShowToday(false);
    const tFoot = $$('tfoot')[0];
    assertTrue(style.isElementShown(tFoot));
  },

  testHidingOfTableFootAfterCreate3() {
    picker = new DatePicker();
    picker.create(sandbox);
    picker.setAllowNone(true);
    picker.setShowToday(true);
    const tFoot = $$('tfoot')[0];
    assertTrue(style.isElementShown(tFoot));
  },

  testLongDateFormat() {
    picker = new DatePicker();
    picker.create(sandbox);
    picker.setLongDateFormat(true);
    const dates = $$('td', 'goog-date-picker-date');
    for (let i = 0; i < dates.length; i++) {
      assertEquals(2, dom.getTextContent(dates[i]).length);
    }
  },

  testGetActiveMonth() {
    picker = new DatePicker(new Date(2000, 5, 5));
    const month = picker.getActiveMonth();
    assertObjectEquals(new DateDate(2000, 5, 1), month);

    month.setMonth(10);
    assertObjectEquals(
        'modifying the returned object is safe', new DateDate(2000, 5, 1),
        picker.getActiveMonth());
  },

  testGetActiveMonthBeforeYear100() {
    const date = new Date(23, 5, 5);
    // Above statement will create date with year 1923, need to set full year
    // explicitly.
    date.setFullYear(23);

    const expectedMonth = new DateDate(23, 5, 1);
    expectedMonth.setFullYear(23);

    picker = new DatePicker(date);
    const month = picker.getActiveMonth();
    assertObjectEquals(expectedMonth, month);

    month.setMonth(10);
    assertObjectEquals(
        'modifying the returned object is safe', expectedMonth,
        picker.getActiveMonth());
  },

  testGetDate() {
    picker = new DatePicker(new Date(2000, 0, 1));
    const date = picker.getDate();
    assertObjectEquals(new DateDate(2000, 0, 1), date);

    date.setMonth(1);
    assertObjectEquals(
        'modifying the returned date is safe', new DateDate(2000, 0, 1),
        picker.getDate());

    picker.setDate(null);
    assertNull('no date is selected', picker.getDate());
  },

  testGetDateBeforeYear100() {
    const inputDate = new Date(23, 5, 5);
    // Above statement will create date with year 1923, need to set full year
    // explicitly.
    inputDate.setFullYear(23);
    picker = new DatePicker(inputDate);
    const date = picker.getDate();

    const expectedDate = new DateDate(23, 5, 5);
    expectedDate.setFullYear(23);
    assertObjectEquals(expectedDate, date);

    picker.setDate(inputDate);
    assertObjectEquals(expectedDate, picker.getDate());
    const expectedMonth = new DateDate(23, 5, 1);
    expectedMonth.setFullYear(23);
    assertObjectEquals(expectedMonth, picker.getActiveMonth());
  },

  testGridForDecember23() {
    // Initialize picker to December 23.
    const inputDate = new Date(23, 11, 5);
    // Above statement will create date with year 1923, need to set full year
    // explicitly.
    inputDate.setFullYear(23);
    picker = new DatePicker(inputDate);
    picker.create(sandbox);

    // Grid start with last days of November 23, shows December 23 and first
    // days of January 24.
    for (let i = 0; i < 6; i++) {
      for (let j = 0; j < 7; j++) {
        const date = picker.getDateAt(i, j);
        if (date.getMonth() == 0) {
          assertEquals(24, date.getFullYear());
        } else {
          assertEquals(23, date.getFullYear());
        }
      }
    }
  },

  testGridForJanuary22() {
    // Initialize picker to January 22.
    const inputDate = new Date(22, 0, 5);
    // Above statement will create date with year 1922, need to set full year
    // explicitly.
    inputDate.setFullYear(22);
    picker = new DatePicker(inputDate);
    picker.create(sandbox);

    // Grid start with last days of December 21, shows January 22 and first days
    // of February 22.
    for (let i = 0; i < 6; i++) {
      for (let j = 0; j < 7; j++) {
        const date = picker.getDateAt(i, j);
        if (date.getMonth() == 11) {
          assertEquals(21, date.getFullYear());
        } else {
          assertEquals(22, date.getFullYear());
        }
      }
    }
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetDateAt() {
    picker = new DatePicker();
    picker.create(sandbox);
    picker.setDate(new Date(2000, 5, 5));
    const date = picker.getDateAt(0, 0);
    assertTrue(date.equals(picker.grid_[0][0]));

    date.setMonth(1);
    assertFalse(date.equals(picker.grid_[0][0]));
  },

  testGetDateAt_NotInGrid() {
    picker = new DatePicker();
    picker.create(sandbox);
    picker.setDate(new Date(2000, 5, 5));
    let date = picker.getDateAt(-1, 0);
    assertNull(date);

    date = picker.getDateAt(0, -1);
    assertNull(date);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testGetDateElementAt() {
    picker = new DatePicker();
    picker.create(sandbox);
    picker.setDate(new Date(2000, 5, 5));
    /** @suppress {visibility} suppression added to enable type checking */
    const element = picker.getDateElementAt(0, 0);
    assertEquals('td', element.tagName.toLowerCase());
    assertObjectEquals(element, picker.elTable_[1][1]);
  },

  testGetDateElementAt_NotInTable() {
    picker = new DatePicker();
    picker.create(sandbox);
    picker.setDate(new Date(2000, 5, 5));
    /** @suppress {visibility} suppression added to enable type checking */
    let element = picker.getDateElementAt(-1, 0);
    assertNull(element);

    /** @suppress {visibility} suppression added to enable type checking */
    element = picker.getDateElementAt(0, -1);
    assertNull(element);

    /** @suppress {visibility} suppression added to enable type checking */
    element = picker.getDateElementAt(picker.elTable_.length - 1, 0);
    assertNull(element);

    /** @suppress {visibility} suppression added to enable type checking */
    element = picker.getDateElementAt(0, picker.elTable_[0].length - 1);
    assertNull(element);
  },

  testSetDate() {
    picker = new DatePicker();
    picker.createDom();
    picker.enterDocument();
    let selectEvents = 0;
    let changeEvents = 0;
    let changeActiveMonthEvents = 0;
    events.listen(picker, DatePicker.Events.SELECT, () => {
      selectEvents++;
    });
    events.listen(picker, DatePicker.Events.CHANGE, () => {
      changeEvents++;
    });
    events.listen(picker, DatePicker.Events.CHANGE_ACTIVE_MONTH, () => {
      changeActiveMonthEvents++;
    });

    // Set date.
    picker.setDate(new Date(2010, 1, 26));
    assertEquals('no select event dispatched', 1, selectEvents);
    assertEquals('no change event dispatched', 1, changeEvents);
    assertEquals(
        'no change active month event dispatched', 1, changeActiveMonthEvents);
    assertTrue(
        'date is set', new DateDate(2010, 1, 26).equals(picker.getDate()));

    // Set date to same date.
    picker.setDate(new Date(2010, 1, 26));
    assertEquals('1 select event dispatched', 2, selectEvents);
    assertEquals('no change event dispatched', 1, changeEvents);
    assertEquals(
        'no change active month event dispatched', 1, changeActiveMonthEvents);

    // Set date to different date.
    picker.setDate(new Date(2010, 1, 27));
    assertEquals('another select event dispatched', 3, selectEvents);
    assertEquals('1 change event dispatched', 2, changeEvents);
    assertEquals(
        '2 change active month events dispatched', 1, changeActiveMonthEvents);

    // Set date to a date in a different month.
    picker.setDate(new Date(2010, 2, 27));
    assertEquals('another select event dispatched', 4, selectEvents);
    assertEquals('another change event dispatched', 3, changeEvents);
    assertEquals(
        '3 change active month event dispatched', 2, changeActiveMonthEvents);

    // Set date to none.
    picker.setDate(null);
    assertEquals('another select event dispatched', 5, selectEvents);
    assertEquals('another change event dispatched', 4, changeEvents);
    assertNull('date cleared', picker.getDate());
  },

  testChangeActiveMonth() {
    picker = new DatePicker();
    let changeActiveMonthEvents = 0;
    events.listen(picker, DatePicker.Events.CHANGE_ACTIVE_MONTH, () => {
      changeActiveMonthEvents++;
    });

    // Set date.
    picker.setDate(new Date(2010, 1, 26));
    assertEquals(
        'change active month events dispatched', 1, changeActiveMonthEvents);
    assertTrue(
        'date is set', new DateDate(2010, 1, 26).equals(picker.getDate()));

    // Change to next month.
    picker.nextMonth();
    assertEquals(
        '1 change active month event dispatched', 2, changeActiveMonthEvents);
    assertTrue(
        'date should still be the same',
        new DateDate(2010, 1, 26).equals(picker.getDate()));

    // Change to next year.
    picker.nextYear();
    assertEquals(
        '2 change active month events dispatched', 3, changeActiveMonthEvents);

    // Change to previous month.
    picker.previousMonth();
    assertEquals(
        '3 change active month events dispatched', 4, changeActiveMonthEvents);

    // Change to previous year.
    picker.previousYear();
    assertEquals(
        '4 change active month events dispatched', 5, changeActiveMonthEvents);
  },

  testChangeActiveMonth_whenGridGrows_dispatchesGridIncreaseEvent() {
    picker = new DatePicker();
    picker.setShowFixedNumWeeks(false);
    picker.render();
    let gridSizeIncreaseEvents = 0;
    events.listen(
        picker, DatePicker.Events.GRID_SIZE_INCREASE,
        () => void gridSizeIncreaseEvents++);

    // Feb 2015 has only four rows of dates.
    picker.setDate(new Date(2015, 1, 1));
    assertEquals('No grid size changes yet', 0, gridSizeIncreaseEvents);

    // Mar 2015 has 5 rows.
    picker.nextMonth();
    assertEquals(
        '1 grid size increase events dispatched', 1, gridSizeIncreaseEvents);

    // Going back to Feb is a size decrease, so no dispatch.
    picker.previousMonth();
    assertEquals(
        '1 grid size increase events dispatched', 1, gridSizeIncreaseEvents);

    // Going forward to Mar again should dispatch again.
    picker.nextMonth();
    assertEquals(
        '2 grid size increase events dispatched', 2, gridSizeIncreaseEvents);

    // Apr 2015 has 5 rows also.
    picker.nextMonth();
    assertEquals(
        '2 grid size increase events dispatched', 2, gridSizeIncreaseEvents);

    // May 2015 has 6 rows.
    picker.nextMonth();
    assertEquals(
        '3 grid size increase events dispatched', 3, gridSizeIncreaseEvents);
  },

  testChangeActiveMonth_withFixedNumWeeks_dispatchesNoGridIncreaseEvent() {
    picker = new DatePicker();
    picker.setShowFixedNumWeeks(true);
    picker.render();
    let gridSizeIncreaseEvents = 0;
    events.listen(
        picker, DatePicker.Events.GRID_SIZE_INCREASE,
        () => void gridSizeIncreaseEvents++);

    // Feb 2015 has only four rows of dates.
    picker.setDate(new Date(2015, 1, 1));
    assertEquals('No grid size changes yet', 0, gridSizeIncreaseEvents);

    for (let i = 0; i < 100; i++) {
      picker.nextMonth();
    }
    assertEquals('No grid size changes', 0, gridSizeIncreaseEvents);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUserSelectableDates() {
    const dateRange =
        new DateRange(new DateDate(2010, 1, 25), new DateDate(2010, 1, 27));
    picker = new DatePicker();
    picker.setUserSelectableDateRange(dateRange);
    assertFalse(
        'should not be selectable date',
        picker.isUserSelectableDate_(new DateDate(2010, 1, 24)));
    assertTrue(
        'should be a selectable date',
        picker.isUserSelectableDate_(new DateDate(2010, 1, 25)));
    assertTrue(
        'should be a selectable date',
        picker.isUserSelectableDate_(new DateDate(2010, 1, 26)));
    assertTrue(
        'should be a selectable date',
        picker.isUserSelectableDate_(new DateDate(2010, 1, 27)));
    assertFalse(
        'should not be selectable date',
        picker.isUserSelectableDate_(new DateDate(2010, 1, 28)));
  },

  testGetUserSelectableDateRange() {
    picker = new DatePicker();
    let dateRange = picker.getUserSelectableDateRange();
    assertTrue(
        'default date range is all time',
        DateRange.equals(dateRange, DateRange.allTime()));
    const newDateRange =
        new DateRange(new DateDate(2010, 1, 25), new DateDate(2010, 1, 27));
    picker.setUserSelectableDateRange(newDateRange);
    dateRange = picker.getUserSelectableDateRange();
    assertTrue(
        'should be equal to updated date range',
        DateRange.equals(dateRange, newDateRange));
  },

  testUniqueCellIds() {
    picker = new DatePicker();
    picker.render();
    const cells = dom.getElementsByTagNameAndClass(
        TagName.TD, undefined, picker.getElement());
    const existingIds = {};
    const numCells = cells.length;
    for (let i = 0; i < numCells; i++) {
      assertNotNull(cells[i]);
      if (aria.getRole(cells[i]) == Role.GRIDCELL) {
        assertNonEmptyString('cell id is non empty', cells[i].id);
        assertUndefined('cell id is not unique', existingIds[cells[i].id]);
        existingIds[cells[i].id] = 1;
      }
    }
  },

  testDecoratePreservesClasses() {
    picker = new DatePicker();
    const div = dom.createDom(TagName.DIV, 'existing-class');
    picker.decorate(div);
    assertTrue(classlist.contains(div, picker.getBaseCssClass()));
    assertTrue(classlist.contains(div, 'existing-class'));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testKeyboardNavigation_arrowKey() {
    // This is a Sunday, so it's the first cell in the grid.
    picker = new DatePicker(new Date(2017, 9, 1));
    // Make the first column be Sunday, not week numbers
    picker.setShowWeekNum(false);
    picker.render(dom.getElement('sandbox'));
    const selectEvents = recordFunction();
    const changeEvents = recordFunction();
    events.listen(picker, DatePicker.Events.SELECT, selectEvents);
    events.listen(picker, DatePicker.Events.CHANGE, changeEvents);

    testingEvents.fireNonAsciiKeySequence(
        picker.getElement(), KeyCodes.DOWN, KeyCodes.DOWN);
    changeEvents.assertCallCount(1);
    selectEvents.assertCallCount(0);

    // Make sure the new selection is focused, for a11y.  elTable_[0] has the
    // week day headers, so +1 starts at the first row of days.
    assertEquals(picker.elTable_[1 + 1][1], document.activeElement);

    testingEvents.fireNonAsciiKeySequence(
        picker.getElement(), KeyCodes.ENTER, KeyCodes.ENTER);
    changeEvents.assertCallCount(1);
    selectEvents.assertCallCount(1);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testKeyboardNavigation_homeKey() {
    // This is a Sunday, so it's the first cell in the grid.
    picker = new DatePicker(new Date(2017, 9, 1));
    // Make the first column be Sunday, not week numbers
    picker.setShowWeekNum(false);
    picker.render(dom.getElement('sandbox'));
    const selectEvents = recordFunction();
    const changeEvents = recordFunction();
    events.listen(picker, DatePicker.Events.SELECT, selectEvents);
    events.listen(picker, DatePicker.Events.CHANGE, changeEvents);

    testingEvents.fireNonAsciiKeySequence(
        picker.getElement(), KeyCodes.HOME, KeyCodes.HOME);
    changeEvents.assertCallCount(1);
    selectEvents.assertCallCount(1);

    // Make sure the new selection is focused, for a11y.  elTable_[0] has the
    // week day headers, so +1 starts at the first row of days.
    assertEquals(picker.elTable_[2 + 1][3], document.activeElement);
  },

  testDayGridHasNonEmptyAriaLabels() {
    picker = new DatePicker(new Date(2017, 8, 9));
    picker.render(dom.getElement('sandbox'));

    const cells = dom.getElementsByTagNameAndClass(
        TagName.TD, undefined, picker.getElement());
    const numCells = cells.length;
    for (let i = 0; i < numCells; i++) {
      assertNotNull(cells[i]);
      if (aria.getRole(cells[i]) == Role.GRIDCELL) {
        assertNonEmptyString(
            'Aria label in date cell should not be empty',
            aria.getLabel(cells[i]));
      }
    }
  },
});
