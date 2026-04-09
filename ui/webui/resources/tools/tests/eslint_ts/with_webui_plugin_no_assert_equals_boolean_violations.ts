// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test file for @webui-eslint/no-assert-equals-boolean */

declare function assertEquals(a: any, b: any): void;

const bool: boolean = true;
const notBool: boolean|undefined = undefined;
const anyBool: any = true;

// Case with violations
assertEquals(true, bool);
assertEquals(bool, true);
assertEquals(false, bool);
assertEquals(bool, false);

// Case with no violations: boolean|undefined
assertEquals(true, notBool);
assertEquals(notBool, true);
assertEquals(false, notBool);
assertEquals(notBool, false);

// Case with no violations: any
assertEquals(true, anyBool);
assertEquals(anyBool, true);
assertEquals(false, anyBool);
assertEquals(anyBool, false);
