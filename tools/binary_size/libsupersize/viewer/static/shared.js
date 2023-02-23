// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * Constants used by both the UI and Web Worker scripts.
 */

/**
 * Node object used to represent the file tree. Can represent either an artifact
 * or a symbol.
 * @typedef {Object} TreeNode
 * @property {?Array<!TreeNode>} children - Child tree nodes. Empty arrays
 *     indicate this is a leaf node. Null values are placeholders to indicate
 *     children that haven't been loaded in yet.
 * @property {?TreeNode} parent - Parent tree node, null if this is a root node.
 * @property {string} idPath - Full path to this node.
 * @property {string} objPath - Path to the object file containing this symbol.
 * @property {string} srcPath - Path to the source containing this symbol.
 * @property {string} disassembly - The disassembly for the node.
 * @property {string} container - The container for the node.
 * @property {string} component - OWNERS Component for this symbol.
 * @property {string} fullName - The full name of the node.
 * @property {number} shortNameIndex - The name of the node is included in the
 *     `idPath`. This index indicates where to start to slice the `idPath` to
 *     read the name.
 * @property {number} size - Byte size of this node and its children.
 * @property {number|undefined} padding - Padding bytes used by this node.
 * @property {number|undefined} beforeSize - Diff mode only: Byte size of the
 *     node and its children in the "before" binary.
 * @property {number|undefined} address - Start address of this node.
 * @property {number} flags - A bit field to store symbol properties.
 * @property {number} numAliases - Number of aliases for the symbol.
 * @property {string} type - Type of this node. If this node has children, the
 *     string may have a second character to denote the most common child.
 * @property {_DIFF_STATUSES} diffStatus
 * @property {Object<string, !TreeNodeChildStats>} childStats - Stats about
 *     this node's descendants, keyed by symbol type.
 */

/**
 * @typedef {Object} MetricsItem
 * @property {string} name - Item name, used for UI and grouping for total.
 * @property {number} value - Metrics value, can be bytes or count.
 * @property {number|undefined} beforeValue - Optional value for "before".
 */

/**
 * @typedef {Object} MetricsTreeNode
 * @property {string|undefined} name - The full name of the node, and is shown
 *     in the UI.
 * @property {?MetricsTreeNode} parent - Parent tree node, null if this is a
 *     root node.
 * @property {!Array<!MetricsTreeNode>|undefined} children - Child nodes.
 *     Non-existent or null indicates this is a leaf node.
 * @property {!Array<!MetricsItem>|undefined} items - For leaf nodes only, a
 *     list of named values for a metric. Mutually exclusive with |liveItems|.
 * @property {?function(function(string): boolean): !Array<!MetricsItem>|
 *            undefined} liveItems - For leaf nodes only, function to return a
 *     list of named values for a metric, taking a filtering function. To be
 *     included, the filter is eithe rnull, or needs to return true for names of
 *     all ancestor with |isFiltered| true. Mutually exclusive with |item|.
 * @property {boolean|undefined} isFiltered - For group nodes only, whether
 *     filtering is applied to the node.
 * @property {string|undefined} iconKey - For group nodes only, input for
 *     getMetricsIconTemplate() to retrieve icon.
 */

/**
 * Stats about a node's descendants of a certain type.
 * @typedef {Object} TreeNodeChildStats
 * @property {number} size - Byte size.
 * @property {number} count - Number of symbols.
 * @property {number} added
 * @property {number} removed
 * @property {number} changed
 */

/**
 * @typedef {Object} TreeProgress
 * @property {number} percent - Number from (0-1] for progress percentage.
 * @property {string} error - Error message, if an error occurred in the worker.
 *     If empty, then there was no error.
 */

/**
 * @typedef {Object} GetSizeResult
 * @property {string} description - Description of the size, to be shown as
 *     hover text.
 * @property {Node} element - Abbreviated representation of the size, which can
 *     include DOM elements for styling.
 * @property {number} value - The size number used to create the other strings.
 */

/** @typedef {function(!TreeNode, string):!GetSizeResult} GetSize */

/**
 * Properties loaded from .size / .sizediff files.
 * @typedef {Object} SizeProperties
 * @property {boolean} isMultiContainer - Whether multiple containers exist.
 */

/**
 * Nested key-value pairs that stores metadata of .size or .sizediff files.
 * @typedef {Object} MetadataType
 * @property {Object|undefined} before_size_file - Metadata of the "before"
 *     file, grouped by containers.
 * @property {Object} size_file - Metadata of the "main" / "after" file,
 *     grouped by containers.
 */

/**
 * @typedef {Object} LoadTreeResults
 * @property {boolean} isMultiContainer - Whether multiple containers exist.
 * @property {string} beforeBlobUrl - The BLOB url of the "before" file.
 * @property {string} loadBlobUrl - The BLOB url of the "main" / "after" file.
 * @property {?MetadataType} metadata
 */

/**
 * @typedef {Object} BuildOptions
 * @property {string} loadUrl
 * @property {string} beforeUrl
 * @property {boolean} methodCountMode
 * @property {string} groupBy
 * @property {string} includeRegex
 * @property {string} excludeRegex
 * @property {string} includeSections
 * @property {number} minSymbolSize
 * @property {number} flagToFilter
 * @property {boolean} nonOverhead
 * @property {boolean} disassemblyMode
 */

/**
 * @typedef {Object} BuildTreeResults
 * @property {Object} root
 * @property {boolean} diffMode - Whether diff mode is engaged.
 * @property {?LoadTreeResults} loadResults
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

/**
 * @type {Set} Set of all known symbol types. Artifact types are not included.
 */
const _SYMBOL_TYPE_SET =
    new Set(/** @type {Iterable<string>} */ ('bdrtRxmopP'));

/** @type {Array<string> | string} */
const _LOCALE = /** @type {Array<string>} */ (navigator.languages) ||
    navigator.language;

/** @enum {string} Keys in query string and names of input elements. */
const STATE_KEY = {
  LOAD_URL: 'load_url',
  BEFORE_URL: 'before_url',
  BYTE_UNIT: 'byteunit',
  METHOD_COUNT: 'method_count',
  MIN_SIZE: 'min_size',
  GROUP_BY: 'group_by',
  INCLUDE: 'include',
  EXCLUDE: 'exclude',
  TYPE: 'type',
  FLAG_FILTER: 'flag_filter',
};

/**
 * Throws error if |cond| is falsey.
 * @param {boolean} cond The condition to check.
 * @param {string=} msg Message on assert failure.
 */
function assert(cond, msg = 'Assert fail.') {
  if (!cond)
    throw new Error(msg);
}

/**
 * Throws error if |obj| is null or undefined; returns |obj| otherwise.
 * @param {?Object} obj The (non-primitive) object to check.
 * @param {string=} msg Message on assert failure.
 * @return {!Object}
 */
function assertNotNull(obj, msg = 'Assert fail: Object is null.') {
  if (obj == null)  // Using == to also include undefined.
    throw new Error(msg);
  return obj;
}

/**
 * Iterates over each type in the query string. Types can be expressed as
 * repeats of the same key in the query string ("type=b&type=p") or as a long
 * string with multiple characters ("type=bp").
 * @generator
 * @param {Array<string>} typesList All values associated with the "type" key
 *     in the query string.
 */
function* translateTypes(typesList) {
  for (const typeOrTypes of typesList) {
    for (const typeChar of typeOrTypes) {
      yield typeChar;
    }
  }
}

/**
 * Wraps `func` to collapse repeated calls within `wait` into a single call.
 * @template T
 * @param {function & T} func
 * @param {number} wait Time to wait before func can be called again (ms).
 * @returns {T}
 */
function debounce(func, wait) {
  /** @type {number} */
  let timeoutId = 0;
  function debounced(...args) {
    clearTimeout(timeoutId);
    timeoutId = setTimeout(() => func(...args), wait);
  }
  return /** @type {*} */ (debounced);
}

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
