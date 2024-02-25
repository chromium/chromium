// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

export class MockActionModel extends EventTarget {
  constructor(
      public title: string, public entries: Entry[] = [],
      public actionsModel: MockActionsModel|null = null) {
    super();
  }

  getTitle() {
    return this.title;
  }

  onCanExecute() {}

  onExecute() {
    dispatchSimpleEvent(this, 'invalidated', true);
  }
}

export class MockActionsModel extends EventTarget {
  constructor(private actions_: Record<string, MockActionModel> = {}) {
    super();

    Object.keys(this.actions_).forEach((key) => {
      if (this.actions_[key]) {
        this.actions_[key]!.actionsModel = this;
      }
    });
  }

  initialize() {
    return Promise.resolve();
  }

  getActions() {
    return this.actions_;
  }
}
