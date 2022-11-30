// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class cooperates with blink_perf.py to perform actions. Currently only
// stopping service workers is supported.
class ServiceWorkerPerfTools {
  constructor() {
    this.actionDoneCallback = null;
    this.actionRequired = false;
    this.action = null;
    this.enabled = false;
  }

  // This should be called before other methods.
  enable() {
    this.enabled = true;
  }

  // Call this to stop all service workers. When the returned promise is
  // resolved, all service workers are stopped.
  stopWorkers() {
    return this.performAction('stop-workers');
  }

  // Call this to notify blink_perf.py to stop waiting for more actions. When
  // the returned promise is resolved, blink_perf.py has stopped waiting.
  quit() {
    return this.performAction('quit');
  }

  // Called by blink_perf.py after an action has been performed.
  notifyActionDone() {
    if (!this.actionDoneCallback)
      throw new Error('There is no pending action!');

    this.actionDoneCallback();
    this.actionDoneCallback = null;
    this.action = null;
    this.actionRequired = false;
  }

  performAction(action) {
    if (!this.enabled) {
      throw new TypeError('ServiceWorkerPerfTools is not enabled,' +
          ' call enable() first!');
    }
    if (this.actionRequired) {
      throw new Error('There is already a pending action:', this.action);
    }
    const promise = new Promise(resolve => {
      this.actionDoneCallback = resolve;
    });
    this.action = action;
    this.actionRequired = true;
    return promise;
  }
}
window.serviceWorkerPerfTools = new ServiceWorkerPerfTools;