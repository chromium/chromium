// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type TestFunctionName = string;
export type TestFunction = (() => void)|(() => Promise<void>);

/**
 * Namespace for test cases.
 */
export const testcase: Record<TestFunctionName, TestFunction> = {};
