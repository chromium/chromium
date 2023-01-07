/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview JSON performance tests.
 */

goog.provide('goog.jsonPerf');

goog.require('goog.dom');
goog.require('goog.json');
goog.require('goog.math');
goog.require('goog.string');
goog.require('goog.testing.PerformanceTable');
goog.require('goog.testing.PropertyReplacer');
goog.require('goog.testing.jsunit');

goog.setTestOnly('goog.jsonPerf');

var table = new goog.testing.PerformanceTable(goog.dom.getElement('perfTable'));

var stubs = new goog.testing.PropertyReplacer();

function tearDown() {
  stubs.reset();
}

function testSerialize() {
  var obj = populateObject({}, 50, 4);

  table.run(function() {
    'use strict';
    var s = JSON.stringify(obj);
  }, 'Stringify using JSON.stringify');

  table.run(function() {
    'use strict';
    var s = goog.json.serialize(obj);
  }, 'Stringify using goog.json.serialize');
}

function testParse() {
  var obj = populateObject({}, 50, 4);
  var s = JSON.stringify(obj);

  table.run(function() {
    'use strict';
    var o = JSON.parse(s);
  }, 'Parse using JSON.parse');

  table.run(function() {
    'use strict';
    var o = goog.json.parse(s);
  }, 'Parse using goog.json.parse');
}


/**
 * @param {!Object} obj The object to add properties to.
 * @param {number} numProperties The number of properties to add.
 * @param {number} depth The depth at which to recursively add properties.
 * @return {!Object} The object given in obj (for convenience).
 */
function populateObject(obj, numProperties, depth) {
  if (depth == 0) {
    return randomLiteral();
  }

  // Make an object with a mix of strings, numbers, arrays, objects, booleans
  // nulls as children.
  for (var i = 0; i < numProperties; i++) {
    var bucket = goog.math.randomInt(3);
    switch (bucket) {
      case 0:
        obj[i] = randomLiteral();
        break;
      case 1:
        obj[i] = populateObject({}, numProperties, depth - 1);
        break;
      case 2:
        obj[i] = populateObject([], numProperties, depth - 1);
        break;
    }
  }
  return obj;
}


function randomLiteral() {
  var bucket = goog.math.randomInt(3);
  switch (bucket) {
    case 0:
      return goog.string.getRandomString();
    case 1:
      return Math.random();
    case 2:
      return Math.random() >= .5;
  }
  return null;
}
