// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {foo} from './bar.js';

export function testFoo(): boolean {
  return foo() == 'foo';
}
