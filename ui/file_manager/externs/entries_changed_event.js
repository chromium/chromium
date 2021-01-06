// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {externsValidation} this file is used as externs and also
 * as JS module, Closure fails to compile as JS module.
 */

// clang-format off
// #import {util} from '../file_manager/common/js/util.m.js';
// clang-format on

/* #export */ class EntriesChangedEvent extends Event {
  /** @param {string} eventName */
  constructor(eventName) {
    super(eventName);

    /** @type {util.EntryChangedKind} */
    this.kind;

    /** @type {Array<!Entry>} */
    this.entries;
  }
}
