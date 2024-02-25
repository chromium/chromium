// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';

/**
 * @return Whether the passed tagged template literal is a valid array.
 */
function isValidArray(arr: TemplateStringsArray|readonly string[]): boolean {
  if (arr instanceof Array && Object.isFrozen(arr)) {
    return true;
  }

  return false;
}

/**
 * Checks if the passed tagged template literal only contains static string.
 * And return the string in the literal if so.
 * Throws an Error if the passed argument is not supported literals.
 */
function getStaticString(literal: TemplateStringsArray): string {
  const isStaticString = isValidArray(literal) && !!literal.raw &&
      isValidArray(literal.raw) && literal.length === literal.raw.length &&
      literal.length === 1;
  assert(isStaticString, 'static_types.js only allows static strings');

  return literal.join('');
}

function createTypes(_ignore: string, literal: TemplateStringsArray): string {
  return getStaticString(literal);
}

/**
 * Rules used to enforce static literal checks.
 */
const rules: TrustedTypePolicyOptions = {
  createHTML: createTypes,
  createScript: createTypes,
  createScriptURL: createTypes,
};

/**
 * This policy returns Trusted Types if the passed literal is static.
 */
let staticPolicy: TrustedTypePolicy|TrustedTypePolicyOptions;
if (window.trustedTypes) {
  staticPolicy = window.trustedTypes.createPolicy('static-types', rules);
} else {
  staticPolicy = rules;
}

/**
 * Returns TrustedHTML if the passed literal is static.
 */
export function getTrustedHTML(literal: TemplateStringsArray): (TrustedHTML|
                                                                string) {
  return staticPolicy.createHTML!('', literal);
}

/**
 * Returns TrustedScript if the passed literal is static.
 */
export function getTrustedScript(literal: TemplateStringsArray): (TrustedScript|
                                                                  string) {
  return staticPolicy.createScript!('', literal);
}

/**
 * Returns TrustedScriptURL if the passed literal is static.
 */
export function getTrustedScriptURL(literal: TemplateStringsArray):
    (TrustedScriptURL|string) {
  return staticPolicy.createScriptURL!('', literal);
}
