// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';
import {Command} from './ui/command.js';

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
