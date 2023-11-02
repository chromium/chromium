// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Crostini} from '../../externs/background/crostini.js';

import {CrostiniImpl} from './crostini.js';


/**
 * Crostini shared path state handler factory for foreground tests. Change it
 * to a mock when tests need to override {CrostiniImpl} behavior.
 * @return {!Crostini}
 */
export function createCrostiniForTest() {
  return new CrostiniImpl();
}
