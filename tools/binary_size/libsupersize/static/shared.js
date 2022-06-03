// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

/**
 * @fileoverview
 * Constants used by both the UI and Web Worker scripts.
 */

/**
 * @typedef {object} TreeNode Node object used to represent the file tree. Can
 * represent either an artifact or a symbol.
 * @prop {TreeNode[] | null} children Child tree nodes. Null values indicate
 * that there are children, but that they haven't been loaded in yet. Empty
 * arrays indicate this is a leaf node.
 * @prop {TreeNode | null} parent Parent tree node. null if this is a root node.
 * @prop {string} idPath Full path to this node.
 * @prop {string} srcPath Path to the source containing this symbol.
 * @prop {string} component OWNERS Component for this symbol.
 * @prop {number} shortNameIndex The name of the node is include in the idPath.
 * This index indicates where to start to slice the idPath to read the name.
 * @prop {number} size Byte size of this node and its children.
 * @prop {string} type Type of this node. If this node has children, the string
 * may have a second character to denote the most common child.
 * @prop {number} flags
 * @prop {{[type: string]: TreeNodeChildStats}} childStats Stats about this
 * node's descendants, organized by symbol type.
 */
/**
 * @typedef {object} TreeNodeChildStats Stats about a node's descendants of
 * a certain type.
 * @prop {number} size Byte size
 * @prop {number} count Number of symbols
 */

/**
 * @typedef {object} TreeProgress
 * @prop {TreeNode} root Root node and its direct children.
 * @prop {number} percent Number from (0-1] to represent percentage.
 * @prop {boolean} diffMode True if we are currently showing the diff of two
 * different size files.
 * @prop {string} [error] Error message, if an error occured in the worker.
 * If unset, then there was no error.
 */

/**
 * @typedef {object} GetSizeResult
 * @prop {string} description Description of the size, shown as hover text
 * @prop {Node} element Abbreviated representation of the size, which can
 * include DOM elements for styling.
 * @prop {number} value The size number used to create the other strings.
 */

/**
 * @typedef {(node: TreeNode, unit: string) => GetSizeResult} GetSize
 */

/**
 * @typedef {object} SizeProperties Properties loaded from .size / .sizediff
 * files.
 * @prop {boolean} isMultiContainer Whether multiple containers exist.
 */

/**
 * Abberivated keys used by FileEntrys in the JSON data file. These must match
 * _COMPACT_*_KEY variables in html_report.py.
 */
const _KEYS = Object.freeze({
  COMPONENT_INDEX: /** @type {'c'} */ ('c'),
  SOURCE_PATH: /** @type {'p'} */ ('p'),
  FILE_SYMBOLS: /** @type {'s'} */ ('s'),
  SIZE: /** @type {'b'} */ ('b'),
  COUNT: /** @type {'u'} */ ('u'),
  FLAGS: /** @type {'f'} */ ('f'),
  SYMBOL_NAME: /** @type {'n'} */ ('n'),
  NUM_ALIASES: /** @type {'a'} */ ('a'),
  TYPE: /** @type {'t'} */ ('t'),
});

/** Abberivated keys used by FileEntrys in the JSON data file. */
const _FLAGS = Object.freeze({
  ANONYMOUS: 2 ** 0,
  STARTUP: 2 ** 1,
  UNLIKELY: 2 ** 2,
  REL: 2 ** 3,
  REL_LOCAL: 2 ** 4,
  GENERATED_SOURCE: 2 ** 5,
  CLONE: 2 ** 6,
  HOT: 2 ** 7,
  COVERAGE: 2 ** 8,
  UNCOMPRESSED: 2 ** 9,
});

/**
 * @enum {number} Various byte units and the corresponding amount of bytes
 * that one unit represents.
 */
const _BYTE_UNITS = Object.freeze({
  GiB: 1024 ** 3,
  MiB: 1024 ** 2,
  KiB: 1024 ** 1,
  B: 1024 ** 0,
});

/**
 * @enum {number} All possible states for a delta symbol.
 */
const _DIFF_STATUSES = Object.freeze({
  UNCHANGED: 0,
  CHANGED: 1,
  ADDED: 2,
  REMOVED: 3,
});

/**
 * Special types used by artifacts, such as folders and files.
 */
const _ARTIFACT_TYPES = {
  DIRECTORY: 'D',
  COMPONENT: 'C',
  FILE: 'F',
  JAVA_CLASS: 'J',
};
const _ARTIFACT_TYPE_SET = new Set(Object.values(_ARTIFACT_TYPES));

/** Type for a code/.text symbol */
const _CODE_SYMBOL_TYPE = 't';
/** Type for a dex method symbol */
const _DEX_METHOD_SYMBOL_TYPE = 'm';
/** Type for a non-method dex symbol */
const _DEX_SYMBOL_TYPE = 'x';
/** Type for an 'other' symbol */
const _OTHER_SYMBOL_TYPE = 'o';

/** Set of all known symbol types. Artifact types are not included. */
const _SYMBOL_TYPE_SET = new Set('bdrtRxmopP');

/** Name used by a directory created to hold symbols with no name. */
const _NO_NAME = '(No path)';

/** Key where type is stored in the query string state. */
const _TYPE_STATE_KEY = 'type';

/** @type {string | string[]} */
const _LOCALE = navigator.languages || navigator.language;

/**
 * Returns shortName for a tree node.
 * @param {TreeNode} node
 */
function shortName(node) {
  return node.idPath.slice(node.shortNameIndex);
}

/**
 * Iterate through each type in the query string. Types can be expressed as
 * repeats of the same key in the query string ("type=b&type=p") or as a long
 * string with multiple characters ("type=bp").
 * @param {string[]} typesList All values associated with the "type" key in the
 * query string.
 */
function* types(typesList) {
  for (const typeOrTypes of typesList) {
    for (const typeChar of typeOrTypes) {
      yield typeChar;
    }
  }
}

/**
 * Limit how frequently `func` is called.
 * @template {T}
 * @param {T & Function} func
 * @param {number} wait Time to wait before func can be called again (ms).
 * @returns {T}
 */
function debounce(func, wait) {
  /** @type {number} */
  let timeoutId;
  function debounced(...args) {
    clearTimeout(timeoutId);
    timeoutId = setTimeout(() => func(...args), wait);
  }
  return /** @type {any} */ (debounced);
}

/**
 * Returns tree if a symbol has a certain bit flag
 * @param {number} flag Bit flag from `_FLAGS`
 * @param {TreeNode} symbolNode
 */
function hasFlag(flag, symbolNode) {
  return (symbolNode.flags & flag) === flag;
}
