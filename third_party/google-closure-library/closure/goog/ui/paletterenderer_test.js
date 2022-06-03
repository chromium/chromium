/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.PaletteRendererTest');
goog.setTestOnly();

const Palette = goog.require('goog.ui.Palette');
const PaletteRenderer = goog.require('goog.ui.PaletteRenderer');
const Role = goog.require('goog.a11y.aria.Role');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.html.testing');

let sandbox;
const items = [
  '<div aria-label="label-0"></div>',
  '<div title="title-1"></div>',
  '<div aria-label="label-2" title="title-2"></div>',
  '<div><span title="child-title-3"></span></div>',
];
let itemEls;
let renderer;
let palette;

/** @param {!Array<string>} items */
function createPalette(items) {
  sandbox = dom.getElement('sandbox');
  itemEls = items.map(
      (item, index, a) => dom.safeHtmlToNode(testing.newSafeHtmlForTest(item)));
  renderer = new PaletteRenderer();
  palette = new Palette(itemEls, renderer);
  palette.setSize(4, 1);
}

testSuite({
  setUp() {},

  tearDown() {
    palette.dispose();
  },

  testGridA11yRoles() {
    createPalette(items);
    const grid = renderer.createDom(palette);
    const table = dom.getElementsByTagName(TagName.TABLE, grid)[0];
    assertEquals(Role.GRID, aria.getRole(table));
    const row = dom.getElementsByTagName(TagName.TR, table)[0];
    assertEquals(Role.ROW, aria.getRole(row));
    const cell = dom.getElementsByTagName(TagName.TD, row)[0];
    assertEquals(Role.GRIDCELL, aria.getRole(cell));
  },

  testCellA11yLabels() {
    createPalette(items);
    const grid = renderer.createDom(palette);
    const cells = dom.getElementsByTagName(TagName.TD, grid);

    assertEquals(
        'An aria-label is used as a label', 'label-0', aria.getLabel(cells[0]));
    assertEquals(
        'A title is used as a label', 'title-1', aria.getLabel(cells[1]));
    assertEquals(
        'An aria-label takes precedence over a title', 'label-2',
        aria.getLabel(cells[2]));
    assertEquals(
        'Children are traversed to find labels', 'child-title-3',
        aria.getLabel(cells[3]));
  },

  testA11yActiveDescendant() {
    createPalette(items);
    palette.render();
    const cells =
        dom.getElementsByTagName(TagName.TD, palette.getElementStrict());

    renderer.highlightCell(palette, cells[1].firstChild, true);
    assertEquals(
        cells[1].id,
        aria.getState(palette.getElementStrict(), State.ACTIVEDESCENDANT));

    renderer.highlightCell(palette, cells[0].firstChild, false);
    assertEquals(
        cells[1].id,
        aria.getState(palette.getElementStrict(), State.ACTIVEDESCENDANT));

    renderer.highlightCell(palette, cells[1].firstChild, false);
    assertNotEquals(
        cells[1].id,
        aria.getState(palette.getElementStrict(), State.ACTIVEDESCENDANT));
  },

  testSetContentIncremental() {
    const items = (new Array(6)).fill('<div class="item">item</div>');
    const itemEls =
        items.map(item => dom.safeHtmlToNode(testing.newSafeHtmlForTest(item)));

    createPalette([]);
    palette.render();
    const paletteEl = palette.getElementStrict();

    let rows = dom.getElementsByTagName(TagName.TR, paletteEl);
    assertEquals(1, rows.length);
    assertEquals(0, dom.getElementsByClass('item', rows[0]).length);

    palette.setContent(itemEls.slice(0, 1));
    rows = dom.getElementsByTagName(TagName.TR, paletteEl);
    assertEquals(1, rows.length);
    assertEquals(1, dom.getElementsByClass('item', rows[0]).length);

    palette.setContent(itemEls.slice(0, 3));
    rows = dom.getElementsByTagName(TagName.TR, paletteEl);
    assertEquals(1, rows.length);
    assertEquals(3, dom.getElementsByClass('item', rows[0]).length);

    palette.setContent(itemEls);
    rows = dom.getElementsByTagName(TagName.TR, paletteEl);
    assertEquals(2, rows.length);
    assertEquals(4, dom.getElementsByClass('item', rows[0]).length);
    assertEquals(2, dom.getElementsByClass('item', rows[1]).length);
  },

  testA11yLabelsSetContentIncremental() {
    const itemEls = items.map(
        (item, index, a) =>
            dom.safeHtmlToNode(testing.newSafeHtmlForTest(item)));

    createPalette([]);
    palette.render();
    const paletteEl = palette.getElementStrict();

    palette.setContent(itemEls.slice(0, 1));
    let cells = dom.getElementsByTagName(TagName.TD, paletteEl);
    assertEquals(4, cells.length);
    assertEquals('label-0', aria.getLabel(cells[0]));
    assertEquals('', aria.getLabel(cells[1]));
    assertEquals('', aria.getLabel(cells[2]));
    assertEquals('', aria.getLabel(cells[3]));

    palette.setContent(itemEls);
    cells = dom.getElementsByTagName(TagName.TD, paletteEl);
    assertEquals('label-0', aria.getLabel(cells[0]));
    assertEquals('title-1', aria.getLabel(cells[1]));
    assertEquals('label-2', aria.getLabel(cells[2]));
    assertEquals('child-title-3', aria.getLabel(cells[3]));
  },

  testA11yLabelsSetContentIncremental_ariaLabelUpdated() {
    createPalette(items);
    palette.render();
    const paletteEl = palette.getElementStrict();
    let cells = dom.getElementsByTagName(TagName.TD, paletteEl);
    assertEquals('label-0', aria.getLabel(cells[0]));
    assertEquals('title-1', aria.getLabel(cells[1]));
    assertEquals('label-2', aria.getLabel(cells[2]));
    assertEquals('child-title-3', aria.getLabel(cells[3]));

    const newItems = [
      '<div aria-label="newlabel-0"></div>',
      '<div title="newtitle-1"></div>',
      '<div aria-label="newlabel-2" title="title-2"></div>',
      '<div><span></span></div>',
    ];
    const newItemEls = newItems.map(
        (item, index, a) =>
            dom.safeHtmlToNode(testing.newSafeHtmlForTest(item)));

    palette.setContent(newItemEls);

    assertEquals('newlabel-0', aria.getLabel(cells[0]));
    assertEquals('newtitle-1', aria.getLabel(cells[1]));
    assertEquals('newlabel-2', aria.getLabel(cells[2]));
    assertEquals('', aria.getLabel(cells[3]));
  },
});
