/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.InputTypeTest');
goog.setTestOnly();

const InputType = goog.require('goog.dom.InputType');
const googObject = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

testSuite({
  testCorrectNumberOfInputTypes() {
    assertEquals(27, googObject.getCount(InputType));
  },

  testPropertyNamesEqualValues() {
    for (let propertyName in InputType) {
      assertEquals(
          propertyName.toLowerCase().replace('_', '-'),
          InputType[propertyName]);
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testTypes() {
    assertEquals(InputType.TEXT, document.getElementById('textinput').type);
    // Not all browsers support the time input type.
    if (userAgent.CHROME || userAgent.EDGE) {
      assertEquals(InputType.TIME, document.getElementById('timeinput').type);
    }
    assertEquals(InputType.TEXTAREA, document.getElementById('textarea').type);
    assertEquals(
        InputType.SELECT_ONE, document.getElementById('selectone').type);
    assertEquals(
        InputType.SELECT_MULTIPLE,
        document.getElementById('selectmultiple').type);
  },
});
