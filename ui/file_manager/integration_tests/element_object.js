// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(b/199452030): Fix duplication with runtime_loaded_test_util.js
/**
 * @typedef {{
 *   attributes:Record<string, string>,
 *   text:string,
 *   styles?:(Record<string, string>|undefined),
 *   hidden:boolean,
 *   hasShadowRoot: boolean,
 *   imageWidth?: (number|undefined),
 *   imageHeight?: (number|undefined),
 *   renderedWidth?: (number|undefined),
 *   renderedHeight?: (number|undefined),
 *   renderedTop?: (number|undefined),
 *   renderedLeft?: (number|undefined),
 *   scrollLeft?: (number|undefined),
 *   scrollTop?: (number|undefined),
 *   scrollWidth?: (number|undefined),
 *   scrollHeight?: (number|undefined),
 *  }}
 */
// type.
// @ts-ignore: error TS7005: Variable 'ElementObject' implicitly has an 'any'
// type.
export let ElementObject;
