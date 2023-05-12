// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * UI types, constants, and simple classes.
 */

/**
 * @typedef {HTMLAnchorElement | HTMLSpanElement} TreeNodeElement
 */

/**
 * @enum {number} Abberivated keys used by FileEntry fields in the JSON data
 *     file.
 */
const _FLAGS = {
  ANONYMOUS:        1 << 0,
  STARTUP:          1 << 1,
  UNLIKELY:         1 << 2,
  REL:              1 << 3,
  REL_LOCAL:        1 << 4,
  GENERATED_SOURCE: 1 << 5,
  CLONE:            1 << 6,
  HOT:              1 << 7,
  COVERAGE:         1 << 8,
  UNCOMPRESSED:     1 << 9,
};

/** @type {Object<string, _FLAGS>} */
const _NAMES_TO_FLAGS = Object.freeze({
  hot: _FLAGS.HOT,
  generated: _FLAGS.GENERATED_SOURCE,
  coverage: _FLAGS.COVERAGE,
  uncompressed: _FLAGS.UNCOMPRESSED,
});

/**
 * @enum {number} Various byte units and the corresponding amount of bytes that
 *     one unit represents.
 */
const _BYTE_UNITS = {
  GiB: 1024 ** 3,
  MiB: 1024 ** 2,
  KiB: 1024 ** 1,
  B:   1024 ** 0,
};

/** @enum {number} All possible states for a delta symbol. */
const _DIFF_STATUSES = {
  UNCHANGED: 0,
  CHANGED:   1,
  ADDED:     2,
  REMOVED:   3,
};

/**
 * @enum {string} Special types used by artifacts, such as folders and files.
 */
const _ARTIFACT_TYPES = {
  DIRECTORY:  'D',
  GROUP:      'G',
  FILE:       'F',
  JAVA_CLASS: 'J',
};
const _ARTIFACT_TYPE_SET = new Set(Object.values(_ARTIFACT_TYPES));

/** @type {string} Type for a dex method symbol. */
const _DEX_METHOD_SYMBOL_TYPE = 'm';

/** @type {string} Type for an 'other' symbol. */
const _OTHER_SYMBOL_TYPE = 'o';

/** @type {Array<string> | string} */
const _LOCALE =
    /** @type {Array<string>} */ (navigator.languages) || navigator.language;

/** @type {Object} */
window.supersize = window.supersize || {};

/** @type {?Worker} */
window.supersize.worker = null;

/** @type {?Object} **/
window.supersize.metadata = null;

/**
 * Helper to render cached metadata.
 * @type {!function(): void}
 **/
window.supersize.prettyMetadata = () => {
  console.log(JSON.stringify(window.supersize.metadata, null, 2));
};

/**
 * Returns shortName for a tree node.
 * @param {TreeNode} treeNode
 * @return {string}
 */
function shortName(treeNode) {
  return treeNode.idPath.slice(treeNode.shortNameIndex);
}

/**
 * Returns whether a symbol has a certain bit flag.
 * @param {_FLAGS} flag Bit flag from `_FLAGS`.
 * @param {TreeNode} symbolNode
 * @return {boolean}
 */
function hasFlag(flag, symbolNode) {
  return (symbolNode.flags & flag) === flag;
}

/**
 * Returns a formatted number with grouping, taking an optional range for number
 * of digits after the decimal point (default 0, i.e., assume integer).
 * @param {number} num
 * @param {number=} lo
 * @param {number=} hi
 * @return {string}
 */
function formatNumber(num, lo = 0, hi = 0) {
  return num.toLocaleString(_LOCALE, {
    useGrouping: true,
    minimumFractionDigits: lo,
    maximumFractionDigits: hi
  });
}

/**
 * Same as formatNumber(), but returns percentage instead.
 * @param {number} num
 * @param {number=} lo
 * @param {number=} hi
 * @return {string}
 */
function formatPercent(num, lo = 0, hi = 0) {
  return num.toLocaleString(_LOCALE, {
    style: 'percent',
    minimumFractionDigits: lo,
    maximumFractionDigits: hi,
  });
}

/**
 * Combines multiple iterators (if non-null) into a single iterator.
 * @param {...?Iterable<*>} itList
 * @generator
 */
function* joinIter(...itList) {
  for (const it of itList) {
    if (it) {
      for (const v of it) {
        yield v;
      }
    }
  }
}

/**
 * Returns a sorted list of disstinct strings taken from an iterator.
 * @param {!Iterable<string>} it
 * @return {!Array<string>}
 */
function uniquifyIterToString(it) {
  return Array.from(new Set(it)).sort();
}

/**
 * Returns the extension of a file, given '/'-delimited path.
 * @param {string} path
 * @return {?string} The file extension, or null if none.
 */
function getFileExtension(path) {
  const dirPos = path.lastIndexOf('/');
  const dotPos = path.lastIndexOf('.');
  if (dotPos <= dirPos)  // 'dir.with.dot/file_without_dot', 'no_path_no_ext'.
    return null;
  return (dotPos >= 0) ? path.slice(dotPos + 1) : null;
}

/**
 * Map wrapper with forcedGet() to assigns a value when key not found.
 * @template KEY_DATA_TYPE The data type of a key in the Map.
 * @template VALUE_DATA_TYPE The data type of a value in the Map.
 * @extends {Map<KEY_DATA_TYPE, VALUE_DATA_TYPE>}
 */
class DefaultMap extends Map {
  /**
   * @param {!function(KEY_DATA_TYPE): ?VALUE_DATA_TYPE} makeValue A function
   *     to make a new value (as function of key) if forceGet() is passed a key
   *     that's not in the Map.
   * @param {*} entries
   */
  constructor(makeValue, entries) {
    super(entries);

    /** @private @const {!function(KEY_DATA_TYPE): ?VALUE_DATA_TYPE} */
    this.makeValue = makeValue;
  }

  /**
   * @param {KEY_DATA_TYPE} key
   * @return {VALUE_DATA_TYPE}
   * @public
   */
  forcedGet(key) {
    let value = this.get(key);
    if (value === undefined) {
      value = this.makeValue(key);
      this.set(key, value);
    }
    return value;
  }
}

/**
 * Manager for progress bar animation.
 */
class ProgressBar {
  /** @param {!HTMLProgressElement} elt */
  constructor(elt) {
    /** @private {HTMLProgressElement} */
    this.elt = elt;

    /** @private {number} */
    this.prevVal = this.elt.value;
  }

  /** @param {number} val */
  setValue(val) {
    if (val === 0 || val >= this.prevVal) {
      this.elt.value = val;
      this.prevVal = val;
    } else {
      // Reset to 0 so the progress bar doesn't animate backwards.
      this.setValue(0);
      requestAnimationFrame(() => this.setValue(val));
    }
  }
}

/**
 * Returns a metadata |*size_file|'s |containers| list if it exists, or a list
 * with a synthesized container for old format without explicit containers.
 * @param {?Object} sizeFile
 * @return {!Array<!Object>}
 */
function getOrMakeContainers(sizeFile) {
  if (sizeFile) {
    if (sizeFile.containers)
      return sizeFile.containers;
    // Synthesize for old format without explicit containers.
    const container = {name: '(Default container)'};
    if (sizeFile.metrics_by_file)
      container.metrics_by_file = sizeFile.metrics_by_file;
    if (sizeFile.metadata)
      container.metadata = sizeFile.metadata;
    return [container];
  }
  return [];
}
