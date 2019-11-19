// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class for loading and storing the maps for math atoms from
 * JSON files. The class (and entries) can then be used as via the
 * background page.
 */


goog.provide('cvox.MathMap');

goog.require('cvox.MathCompoundStore');
goog.require('cvox.MathUtil');


/**
 *
 * @constructor
 */
cvox.MathMap = function() {

  /**
   * The compund store for symbol and function mappings.
   * @type {cvox.MathCompoundStore}
   */
  this.store = cvox.MathCompoundStore.getInstance();
  cvox.MathMap.parseFiles(
      cvox.MathMap.FUNCTIONS_FILES_.map(
          function(file) {
            return cvox.MathMap.FUNCTIONS_PATH_ + file;
          }))
              .forEach(goog.bind(this.store.addFunctionRules, this.store));
  cvox.MathMap.parseFiles(
      cvox.MathMap.SYMBOLS_FILES_.map(
          function(file) {
            return cvox.MathMap.SYMBOLS_PATH_ + file;
          }))
              .forEach(goog.bind(this.store.addSymbolRules, this.store));

  var cstrValues = this.store.getDynamicConstraintValues();
  /**
   * Array of domain names.
   * @type {Array<string>}
   */
  this.allDomains = cstrValues.domain;

  /**
   * Array of style names.
   * @type {Array<string>}
   */
  this.allStyles = cstrValues.style;
};


/**
 * Stringifies MathMap into JSON object.
 * @return {string} The stringified version of the mapping.
 */
cvox.MathMap.prototype.stringify = function() {
  return JSON.stringify(this);
};


/**
 * Path to dir containing ChromeVox JSON definitions for math speak.
 * @type {string}
 * @const
 * @private
 */
cvox.MathMap.MATHMAP_PATH_ = 'chromevox/background/mathmaps/';


/**
 * Subpath to dir containing ChromeVox JSON definitions for symbols.
 * @type {string}
 * @const
 * @private
 */
cvox.MathMap.SYMBOLS_PATH_ = cvox.MathMap.MATHMAP_PATH_ + 'symbols/';


/**
 * Subpath to dir containing ChromeVox JSON definitions for functions.
 * @type {string}
 * @const
 * @private
 */
cvox.MathMap.FUNCTIONS_PATH_ = cvox.MathMap.MATHMAP_PATH_ + 'functions/';


/**
 * Array of JSON filenames containing symbol definitions for math speak.
 * @type {Array<string>}
 * @const
 * @private
 */
cvox.MathMap.SYMBOLS_FILES_ = [
  // Greek
  'greek-capital.json', 'greek-small.json', 'greek-scripts.json',
  'greek-mathfonts.json', 'greek-symbols.json',

  // Hebrew
  'hebrew_letters.json',

  // Latin
  'latin-lower-double-accent.json', 'latin-lower-normal.json',
  'latin-lower-phonetic.json', 'latin-lower-single-accent.json',
  'latin-rest.json', 'latin-upper-double-accent.json',
  'latin-upper-normal.json', 'latin-upper-single-accent.json',
  'latin-mathfonts.json',

  // Math Symbols
  'math_angles.json', 'math_arrows.json', 'math_characters.json',
  'math_delimiters.json', 'math_digits.json', 'math_geometry.json',
  'math_harpoons.json', 'math_non_characters.json', 'math_symbols.json',
  'math_whitespace.json', 'other_stars.json'
];


/**
 * Array of JSON filenames containing symbol definitions for math speak.
 * @type {Array<string>}
 * @const
 * @private
 */
cvox.MathMap.FUNCTIONS_FILES_ = [
  'algebra.json', 'elementary.json', 'hyperbolic.json', 'trigonometry.json'
];


/**
 * Loads JSON for a given file name.
 * @param {string} file A valid filename.
 * @return {string} A string representing JSON array.
 */
cvox.MathMap.loadFile = function(file) {
  try {
    return cvox.MathMap.readJSON_(file);
  } catch (x) {
    console.log('Unable to load file: ' + file + ', error: ' + x);
  }
};


/**
 * Loads a list of JSON files.
 * @param {Array<string>} files An array of valid filenames.
 * @return {Array<string>} A string representing JSON array.
 */
cvox.MathMap.loadFiles = function(files) {
  return files.map(cvox.MathMap.loadFile);
};


/**
 * Creates an array of JSON objects from a list of files.
 * @param {Array<string>} files An array of filenames.
 * @return {Array<Object>} Array of JSON objects.
 */
cvox.MathMap.parseFiles = function(files) {
  var strs = cvox.MathMap.loadFiles(files);

  return [].concat.apply([], strs.map(
      // Note: As JSON.parse does not expect the value index as the second
      // parameter, we wrap it.
      function(value) { return JSON.parse(value); }));
};


/**
 * Takes path to a JSON file and returns a JSON object.
 * @param {string} path Contains the path to a JSON file.
 * @return {string} JSON.
 * @private
 */
cvox.MathMap.readJSON_ = function(path) {
  var url = chrome.extension.getURL(path);
  if (!url) {
    throw 'Invalid path: ' + path;
    }

  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, false);
  xhr.send();
  return xhr.responseText;
};
