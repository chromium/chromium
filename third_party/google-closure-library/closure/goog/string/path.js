/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utilities for dealing with POSIX path strings. Based on
 * Python's os.path and posixpath.
 */

goog.provide('goog.string.path');

goog.require('goog.array');
goog.require('goog.string');


/**
 * Returns the final component of a pathname.
 * See http://docs.python.org/library/os.path.html#os.path.basename
 * @param {string} path A pathname.
 * @return {string} path The final component of a pathname, i.e. everything
 *     after the final slash.
 */
goog.string.path.baseName = function(path) {
  'use strict';
  const i = path.lastIndexOf('/') + 1;
  return path.slice(i);
};


/**
 * Alias to goog.string.path.baseName.
 * @param {string} path A pathname.
 * @return {string} path The final component of a pathname.
 * @deprecated Use goog.string.path.baseName.
 */
goog.string.path.basename = goog.string.path.baseName;


/**
 * Returns the directory component of a pathname.
 * See http://docs.python.org/library/os.path.html#os.path.dirname
 * @param {string} path A pathname.
 * @return {string} The directory component of a pathname, i.e. everything
 *     leading up to the final slash.
 */
goog.string.path.dirname = function(path) {
  'use strict';
  const i = path.lastIndexOf('/') + 1;
  let head = path.slice(0, i);
  // If the path isn't all forward slashes, trim the trailing slashes.
  if (!/^\/+$/.test(head)) {
    head = head.replace(/\/+$/, '');
  }
  return head;
};


/**
 * Extracts the extension part of a pathname.
 * @param {string} path The path name to process.
 * @return {string} The extension if any, otherwise the empty string.
 */
goog.string.path.extension = function(path) {
  'use strict';
  const separator = '.';
  // Combining all adjacent periods in the basename to a single period.
  const baseName = goog.string.path.baseName(path).replace(/\.+/g, separator);
  const separatorIndex = baseName.lastIndexOf(separator);
  return separatorIndex <= 0 ? '' : baseName.slice(separatorIndex + 1);
};


// TODO(johnlenz): goog.string.path.join should not accept undefined
/**
 * Joins one or more path components (e.g. 'foo/' and 'bar' make 'foo/bar').
 * An absolute component will discard all previous component.
 * See http://docs.python.org/library/os.path.html#os.path.join
 * @param {...(string|undefined)} var_args One of more path components.
 * @return {string} The path components joined.
 */
goog.string.path.join = function(var_args) {
  'use strict';
  let path = arguments[0];

  for (let i = 1; i < arguments.length; i++) {
    const arg = arguments[i];
    if (goog.string.startsWith(arg, '/')) {
      path = arg;
    } else if (path == '' || goog.string.endsWith(path, '/')) {
      path += arg;
    } else {
      path += '/' + arg;
    }
  }

  return path;
};


/**
 * Normalizes a pathname by collapsing duplicate separators, parent directory
 * references ('..'), and current directory references ('.').
 * See http://docs.python.org/library/os.path.html#os.path.normpath
 * @param {string} path One or more path components.
 * @return {string} The path after normalization.
 */
goog.string.path.normalizePath = function(path) {
  'use strict';
  if (path == '') {
    return '.';
  }

  let initialSlashes = '';
  // POSIX will keep two slashes, but three or more will be collapsed to one.
  if (goog.string.startsWith(path, '/')) {
    initialSlashes = '/';
    if (goog.string.startsWith(path, '//') &&
        !goog.string.startsWith(path, '///')) {
      initialSlashes = '//';
    }
  }

  const parts = path.split('/');
  const newParts = [];

  for (let i = 0; i < parts.length; i++) {
    const part = parts[i];

    // '' and '.' don't change the directory, ignore.
    if (part == '' || part == '.') {
      continue;
    }

    // A '..' should pop a directory unless this is not an absolute path and
    // we're at the root, or we've travelled upwards relatively in the last
    // iteration.
    if (part != '..' || (!initialSlashes && !newParts.length) ||
        goog.array.peek(newParts) == '..') {
      newParts.push(part);
    } else {
      newParts.pop();
    }
  }

  const returnPath = initialSlashes + newParts.join('/');
  return returnPath || '.';
};


/**
 * Splits a pathname into "dirname" and "baseName" components, where "baseName"
 * is everything after the final slash. Either part may return an empty string.
 * See http://docs.python.org/library/os.path.html#os.path.split
 * @param {string} path A pathname.
 * @return {!Array<string>} An array of [dirname, basename].
 */
goog.string.path.split = function(path) {
  'use strict';
  const head = goog.string.path.dirname(path);
  const tail = goog.string.path.baseName(path);
  return [head, tail];
};

// TODO(nnaze): Implement other useful functions from os.path
