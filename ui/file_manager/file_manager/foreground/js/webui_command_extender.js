// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {Command} from 'chrome://resources/js/cr/ui/command.m.js';
// #import {dispatchPropertyChange} from 'chrome://resources/js/cr.m.js';

/**
 * Sets 'hidden' property of a cr.ui.Command instance and dispatches
 * 'hiddenChange' event manually so that associated cr.ui.MenuItem can handle
 * the event.
 * TODO(fukino): Remove this workaround when crbug.com/481941 is fixed.
 *
 * @param {boolean} value New value of hidden property.
 */
cr.ui.Command.prototype.setHidden = function(value) {
  if (value === this.hidden) {
    return;
  }

  const oldValue = this.hidden;
  this.hidden = value;
  cr.dispatchPropertyChange(this, 'hidden', value, oldValue);
};
