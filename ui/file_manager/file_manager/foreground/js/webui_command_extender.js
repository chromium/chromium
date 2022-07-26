// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchPropertyChange} from 'chrome://resources/js/cr.m.js';
import {Command} from 'chrome://resources/js/cr/ui/command.js';

/**
 * Sets 'hidden' property of a Command instance and dispatches
 * 'hiddenChange' event manually so that associated MenuItem can handle
 * the event.
 * TODO(fukino): Remove this workaround when crbug.com/481941 is fixed.
 *
 * @param {boolean} value New value of hidden property.
 */
Command.prototype.setHidden = function(value) {
  if (value === this.hidden) {
    return;
  }

  const oldValue = this.hidden;
  this.hidden = value;
  dispatchPropertyChange(this, 'hidden', value, oldValue);
};
