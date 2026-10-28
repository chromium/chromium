// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css} from '//resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup} from '//resources/lit/v3_0/lit.rollup.js';

// Cases with violations below: Re-exporting (directly or indirectly) from a
// non-barrel file (a file that also contains local declarations/statements).
export {nothing} from '//resources/lit/v3_0/lit.rollup.js';
export type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
export {css};
export type {CSSResultGroup};

// Cases with no violations below: Exporting local declarations via
// 'export {...}' is allowed.
const localConst = 1;
interface LocalInterface {}
export {localConst};
export type {LocalInterface};
