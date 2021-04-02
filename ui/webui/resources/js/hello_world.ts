// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dpapad): Delete this dummy file once the TypeScript demo page is no
// longer needed.
import {world} from './cr/world.js';

export function helloWorld(): string {
  return `Hello ${world()}`;
}
