// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css} from '//resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup} from '//resources/lit/v3_0/lit.rollup.js';

export {nothing} from '//resources/lit/v3_0/lit.rollup.js';
export type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
export {css};
export type {CSSResultGroup};

const foo: number = 3;
const bar: number = foo;

function foo() {
  return foo + bar;
}
