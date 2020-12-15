// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {CrostiniImpl} from './crostini.m.js';
// #import {Crostini} from '../../../externs/background/crostini.m.js';

/* #ignore */ 'use strict';

/**
 * Crostini shared path state handler factory for foreground tests. Change it
 * to a mock when tests need to override {CrostiniImpl} behavior.
 * @return {!Crostini}
 */
/* #export */ function createCrostiniForTest() {
  return new CrostiniImpl();
}
