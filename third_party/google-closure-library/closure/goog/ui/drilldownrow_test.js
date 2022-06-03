/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.DrilldownRowTest');
goog.setTestOnly();

const DrilldownRow = goog.require('goog.ui.DrilldownRow');
const SafeHtml = goog.require('goog.html.SafeHtml');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

function createHtmlForRow(rowText) {
  return SafeHtml.create(
      TagName.TR, {},
      SafeHtml.concat(
          SafeHtml.create(TagName.TD, {}, rowText),
          SafeHtml.create(TagName.TD, {}, 'Second column')));
}
testSuite({
  testMakeRows() {
    const ff = dom.getElement('firstRow');
    const d = new DrilldownRow({});
    const d1 = new DrilldownRow({html: createHtmlForRow('Second row')});
    const d2 = new DrilldownRow({html: createHtmlForRow('Third row')});
    const d21 = new DrilldownRow({html: createHtmlForRow('Fourth row')});
    /** @suppress {checkTypes} suppression added to enable type checking */
    const d22 = new DrilldownRow(DrilldownRow.sampleProperties);
    d.decorate(ff);
    d.addChild(d1, true);
    d.addChild(d2, true);
    d2.addChild(d21, true);
    d2.addChild(d22, true);

    assertThrows(() => {
      d.findIndex();
    });

    assertEquals(0, d1.findIndex());
    assertEquals(1, d2.findIndex());
  },
});
