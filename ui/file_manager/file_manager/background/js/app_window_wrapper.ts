// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openWindow} from '../../common/js/api.js';
import {AsyncQueue} from '../../common/js/async_util.js';
import type {FilesAppState} from '../../common/js/files_app_state.js';

/** Coordinates the creation of new windows for Files app.  */
export class AppWindowWrapper {
  private appState_: FilesAppState|null = null;
  private openingOrOpened_: boolean = false;
  protected queue_ = new AsyncQueue();

  /**
   * Gets the launch lock, used to synchronize the asynchronous initialization
   * steps.
   */
  async getLaunchLock(): Promise<() => void> {
    return this.queue_.lock();
  }

  /**
   * Opens the window.
   * @return Resolves when the window is launched.
   */
  async launch(appState: FilesAppState): Promise<void> {
    // Check if the window is opened or not.
    if (this.openingOrOpened_) {
      console.warn('The window is already opened.');
      return Promise.resolve();
    }
    this.openingOrOpened_ = true;

    // Save application state.
    this.appState_ = appState;

    return this.launch_();
  }

  /**
   * Opens a new window for the SWA. Returns a Promise which resolves when the
   * window is launched.
   */
  private async launch_(): Promise<void> {
    const unlock = await this.getLaunchLock();
    try {
      await this.createWindow_();
    } catch (error) {
      console.error(error);
    } finally {
      unlock();
    }
  }

  /**
   * Return a Promise which resolves when the new window is opened.
   */
  private async createWindow_(): Promise<void> {
    const url = this.appState_!.currentDirectoryURL?.toString() || '';
    const result = await openWindow({
      currentDirectoryURL: url,
      selectionURL: this.appState_!.selectionURL,
    });

    if (!result) {
      throw new Error(`Failed to create window for ${url}`);
    }
  }
}
