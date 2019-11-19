// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MockActionModel {
  /**
   * @param {string} title
   * @param {Array<!Entry>} entries
   */
  constructor(title, entries) {
    this.title = title;
    this.entries = entries;
    this.actionsModel = null;
  }

  getTitle() {
    return this.title;
  }

  onCanExecute() {}

  onExecute() {
    cr.dispatchSimpleEvent('invalidated', this.actionsModel);
  }
}

class MockActionsModel extends cr.EventTarget {
  constructor(actions) {
    super();

    this.actions_ = actions;
    Object.keys(actions).forEach(function(key) {
      actions[key].actionsModel = this;
    });
  }

  initialize() {
    return Promise.resolve();
  }

  getActions() {
    return this.actions_;
  }
}
