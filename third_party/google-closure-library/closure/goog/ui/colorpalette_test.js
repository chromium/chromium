/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ColorPaletteTest');
goog.setTestOnly();

const ColorPalette = goog.require('goog.ui.ColorPalette');
const TagName = goog.require('goog.dom.TagName');
const color = goog.require('goog.color');
const testSuite = goog.require('goog.testing.testSuite');

let emptyPalette;
let samplePalette;

testSuite({
  setUp() {
    emptyPalette = new ColorPalette();
    samplePalette = new ColorPalette(['red', '#00FF00', 'rgb(0, 0, 255)']);
    samplePalette.setSelectedColor('blue');
  },

  tearDown() {
    emptyPalette.dispose();
    samplePalette.dispose();
    document.getElementById('sandbox').innerHTML = '';
  },

  testEmptyColorPalette() {
    const colors = emptyPalette.getColors();
    assertNotNull(colors);
    assertEquals(0, colors.length);

    const nodes = emptyPalette.getContent();
    assertNotNull(nodes);
    assertEquals(0, nodes.length);
  },

  testSampleColorPalette() {
    const colors = samplePalette.getColors();
    assertNotNull(colors);
    assertEquals(3, colors.length);
    assertEquals('red', colors[0]);
    assertEquals('#00FF00', colors[1]);
    assertEquals('rgb(0, 0, 255)', colors[2]);

    const nodes = samplePalette.getContent();
    assertNotNull(nodes);
    assertEquals(3, nodes.length);
    assertEquals('goog-palette-colorswatch', nodes[0].className);
    assertEquals('goog-palette-colorswatch', nodes[1].className);
    assertEquals('goog-palette-colorswatch', nodes[2].className);
    assertEquals('#ff0000', color.parse(nodes[0].style.backgroundColor).hex);
    assertEquals('#00ff00', color.parse(nodes[1].style.backgroundColor).hex);
    assertEquals('#0000ff', color.parse(nodes[2].style.backgroundColor).hex);
  },

  testGetColors() {
    const emptyColors = emptyPalette.getColors();
    assertNotNull(emptyColors);
    assertEquals(0, emptyColors.length);

    const sampleColors = samplePalette.getColors();
    assertNotNull(sampleColors);
    assertEquals(3, sampleColors.length);
    assertEquals('red', sampleColors[0]);
    assertEquals('#00FF00', sampleColors[1]);
    assertEquals('rgb(0, 0, 255)', sampleColors[2]);
  },

  testSetColors() {
    emptyPalette.setColors(['black', '#FFFFFF']);

    const colors = emptyPalette.getColors();
    assertNotNull(colors);
    assertEquals(2, colors.length);
    assertEquals('black', colors[0]);
    assertEquals('#FFFFFF', colors[1]);

    const nodes = emptyPalette.getContent();
    assertNotNull(nodes);
    assertEquals(2, nodes.length);
    assertEquals('goog-palette-colorswatch', nodes[0].className);
    assertEquals('goog-palette-colorswatch', nodes[1].className);
    assertEquals('#000000', color.parse(nodes[0].style.backgroundColor).hex);
    assertEquals('#ffffff', color.parse(nodes[1].style.backgroundColor).hex);
    assertEquals('black', nodes[0].title);
    assertEquals('RGB (255, 255, 255)', nodes[1].title);

    samplePalette.setColors(['#336699', 'cyan']);

    const newColors = samplePalette.getColors();
    assertNotNull(newColors);
    assertEquals(2, newColors.length);
    assertEquals('#336699', newColors[0]);
    assertEquals('cyan', newColors[1]);

    const newNodes = samplePalette.getContent();
    assertNotNull(newNodes);
    assertEquals(2, newNodes.length);
    assertEquals('goog-palette-colorswatch', newNodes[0].className);
    assertEquals('goog-palette-colorswatch', newNodes[1].className);
    assertEquals('#336699', color.parse(newNodes[0].style.backgroundColor).hex);
    assertEquals('#00ffff', color.parse(newNodes[1].style.backgroundColor).hex);
  },

  testSetColorsWithLabels() {
    emptyPalette.setColors(['#00f', '#FFFFFF', 'black'], ['blue', 'white']);
    const nodes = emptyPalette.getContent();
    assertEquals('blue', nodes[0].title);
    assertEquals('white', nodes[1].title);
    assertEquals('black', nodes[2].title);
  },

  testRender() {
    samplePalette.render(document.getElementById('sandbox'));

    assertTrue(samplePalette.isInDocument());

    const elem = samplePalette.getElement();
    assertNotNull(elem);
    assertEquals(String(TagName.DIV), elem.tagName);
    assertEquals('goog-palette', elem.className);

    const table = elem.firstChild;
    assertEquals('TABLE', table.tagName);
    assertEquals('goog-palette-table', table.className);
  },

  testGetSelectedColor() {
    assertNull(emptyPalette.getSelectedColor());
    assertEquals('#0000ff', samplePalette.getSelectedColor());
  },

  testSetSelectedColor() {
    emptyPalette.setSelectedColor('red');
    assertNull(emptyPalette.getSelectedColor());

    samplePalette.setSelectedColor('red');
    assertEquals('#ff0000', samplePalette.getSelectedColor());
    samplePalette.setSelectedColor(17);  // Invalid color spec.
    assertNull(samplePalette.getSelectedColor());

    samplePalette.setSelectedColor('rgb(0, 255, 0)');
    assertEquals('#00ff00', samplePalette.getSelectedColor());
    samplePalette.setSelectedColor(false);  // Invalid color spec.
    assertNull(samplePalette.getSelectedColor());

    samplePalette.setSelectedColor('#0000FF');
    assertEquals('#0000ff', samplePalette.getSelectedColor());
    samplePalette.setSelectedColor(null);  // Invalid color spec.
    assertNull(samplePalette.getSelectedColor());
  },
});
