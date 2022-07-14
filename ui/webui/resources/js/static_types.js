// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from './assert.m.js';

/** @typedef {!Array<string>} */
let TaggedLiterals;

/**
 * @param {!TaggedLiterals} arr
 * @return {boolean} Whether the passed tagged template literal is a valid
 *     array.
 */
function isValidArray(arr) {
  if (arr instanceof Array && Object.isFrozen(arr)) {
    return true;
  }

  return false;
}

/**
 * Checks if the passed tagged template literal only contains static string.
 * And return the string in the literal if so.
 * @param {!TaggedLiterals} literal
 * @return {string}
 * @throws {Error} If the passed argument is not supported literals.
 */
function getStaticString(literal) {
  if (isValidArray(literal) && !!literal.raw && isValidArray(literal.raw) &&
      literal.length === literal.raw.length && literal.length === 1) {
    return literal.join('');
  }

  assertNotReached('Static Types only allows static Template literals');
}

/**
 * @param {string} ignore
 * @param {!TaggedLiterals} literal
 * @return {string}
 */
function createTypes(ignore, literal) {
  return getStaticString(literal);
}

/**
 * Rules used to enforce static literal checks.
 * @type {!TrustedTypePolicyOptions}
 * @suppress {checkTypes}
 */
const rules = {
  createHTML: createTypes,
  createScript: createTypes,
  createScriptURL: createTypes,
};

/**
 * This policy returns Trusted Types if the passed literal is static.
 * @type {!TrustedTypePolicy|!TrustedTypePolicyOptions}
 */
let staticPolicy;
if (window.trustedTypes) {
  staticPolicy = trustedTypes.createPolicy('static-types', rules);
} else {
  staticPolicy = rules;
}

/**
 * @param {!TaggedLiterals} literal
 * @return {!TrustedHTML|string} Returns TrustedHTML if the passed literal is
 *     static.
 * @suppress {checkTypes}
 */
export function getTrustedHTML(literal) {
  return staticPolicy.createHTML('', literal);
}

/**
 * @param {!TaggedLiterals} literal
 * @return {!TrustedScript|string} Returns TrustedScript if the passed literal
 *     is static.
 * @suppress {checkTypes}
 */
export function getTrustedScript(literal) {
  return staticPolicy.createScript('', literal);
}

/**
 * @param {!TaggedLiterals} literal
 * @return {!TrustedScriptURL|string} Returns TrustedScriptURL if the passed
 *     literal is static.
 * @suppress {checkTypes}
 */
export function getTrustedScriptURL(literal) {
  return staticPolicy.createScriptURL('', literal);
}
