// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
* @fileoverview Work-around for
* https://github.com/google/closure-compiler/issues/3143, such that WebUI code
* can use the native EventTarget class.
* TODO(dpapad): Remove this entire file if/when that issue is fixed.
*/

/**
 * @constructor
 * @implements {EventTarget}
 */
export const NativeEventTarget = self['EventTarget'];

/** @override */ NativeEventTarget.prototype.addEventListener;
/** @override */ NativeEventTarget.prototype.dispatchEvent;
/** @override */ NativeEventTarget.prototype.removeEventListener;

