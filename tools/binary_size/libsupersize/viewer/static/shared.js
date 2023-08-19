// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * Types, constants and basic utils shared by UI and Web Worker scripts.
 */

/**
 * Node object used to represent the file tree. Can represent either an artifact
 * or a symbol.
 * @typedef {Object} TreeNode
 * @property {?Array<!TreeNode>} children - Child tree nodes. Empty arrays
 *     indicate this is a leaf node. Null values are placeholders to indicate
 *     children that haven't been loaded in yet.
 * @property {?TreeNode} parent - Parent tree node, null if this is a root node.
 * @property {string} id - Unique identifier of a node.
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
 * @typedef {Object} QueryAncestryResults
 * @property {!Array<number>} ancestorIds
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
