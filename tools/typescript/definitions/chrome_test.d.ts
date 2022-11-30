// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.test API */
// TODO(crbug.com/1203307): Auto-generate this file.

declare namespace chrome {
  export namespace test {
    export function assertEq<T>(expected: T, actual: T, message?: string): void;
    export function assertFalse(value: boolean, message?: string): void;
    export function assertTrue(value: boolean, message?: string): asserts value;
    export function fail(message?: string): never;
    export function runTests(tests: Array<() => void>): void;
    export function runWithUserGesture(callback: () => void): void;
    export function succeed(message?: string): void;
  }
}
