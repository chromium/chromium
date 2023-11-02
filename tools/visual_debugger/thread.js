// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents a thread making a debug call to
// the visual debugger.
//
class Thread {
    // Keeps track of all threads that debug calls came in from
    // while the app is running.
    static registered_threads = {};

    constructor(json) {
      this.threadName_ = json.thread_name;
      // Calls from each thread is enabled by default.
      this.enabled_ = true;
      // Thread color overriding filter colors is disabled by default.
      this.overrideFilters_ = false;
      this.drawColor_ = '#000000';
      this.fillAlpha_ = "10";

      // Add new thread to a pool of thread objects.
      Thread.registered_threads[this.threadName_] = this;

      // Create thread filter chip.
      const threadChip = createThreadChip(this);
      const threadFilters = document.querySelector('#threads');
      threadFilters.appendChild(threadChip);
    }

    static isThreadRegistered(threadName) {
      // If thread already registered, return true.
      return (threadName in Thread.registered_threads);
    }

    static getThread(threadName) {
        return Thread.registered_threads[threadName];
    }

    toggleEnableThread() {
      this.enabled_ = !this.enabled_;
    }
  }