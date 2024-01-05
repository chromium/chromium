// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import type {ProgressCenterPanel} from '../../foreground/js/ui/progress_center_panel.js';

/**
 * Implementation of ProgressCenter at the background page.
 */
export class ProgressCenter {
  /**
   * Current items managed by the progress center.
   */
  private items_: ProgressCenterItem[] = [];

  /**
   * List of panel UI managed by the progress center.
   */
  private panels_: ProgressCenterPanel[] = [];

  /**
   * Inhibit end of operation updates for testing.
   */
  private neverNotifyCompleted_ = false;

  /**
   * Turns off sending updates when a file operation reaches 'completed' state.
   * Used for testing UI that can be ephemeral otherwise.
   */
  neverNotifyCompleted() {
    if (window.IN_TEST) {
      this.neverNotifyCompleted_ = true;
    }
  }

  /**
   * Updates the item in the progress center.
   * If the item has a new ID, the item is added to the item list.
   */
  updateItem(item: ProgressCenterItem) {
    // Update item.
    const index = this.getItemIndex_(item.id);
    if (item.state === ProgressItemState.PROGRESSING ||
        item.state === ProgressItemState.SCANNING) {
      if (index === -1) {
        this.items_.push(item);
      } else {
        this.items_[index] = item;
      }
    } else {
      // Error item is not removed until user explicitly dismiss it.
      if (item.state !== ProgressItemState.ERROR && index !== -1) {
        if (this.neverNotifyCompleted_) {
          item.state = ProgressItemState.PROGRESSING;
          return;
        }
        this.items_.splice(index, 1);
      }
    }

    // Update panels.
    for (const panelItem of this.panels_) {
      panelItem.updateItem(item);
    }
  }

  /**
   * Requests to cancel the progress item.
   * @param id Progress ID to be requested to cancel.
   */
  requestCancel(id: string) {
    const item = this.getItemById(id);
    if (item && item.cancelCallback) {
      item.cancelCallback();
    }
  }

  /**
   * Adds a panel UI to the notification center.
   * @param panel Panel UI.
   */
  addPanel(panel: ProgressCenterPanel) {
    if (this.panels_.indexOf(panel) !== -1) {
      return;
    }

    // Update the panel list.
    this.panels_.push(panel);

    // Set the current items.
    for (const item of this.items_) {
      panel.updateItem(item);
    }

    // Register the cancel callback.
    panel.cancelCallback = this.requestCancel.bind(this);

    // Register the dismiss error item callback.
    panel.dismissErrorItemCallback = this.dismissErrorItem_.bind(this);
  }

  /**
   * Removes a panel UI from the notification center.
   * @param panel Panel UI.
   */
  removePanel(panel: ProgressCenterPanel) {
    const index = this.panels_.indexOf(panel);
    if (index === -1) {
      return;
    }

    this.panels_.splice(index, 1);
    panel.cancelCallback = null;
  }

  /**
   * Obtains item by ID.
   * @param id ID of progress item.
   * @return Progress center item having the
   *     specified ID. Null if the item is not found.
   */
  getItemById(id: string): ProgressCenterItem|undefined {
    return this.items_[this.getItemIndex_(id)];
  }

  /**
   * Obtains item index that have the specifying ID.
   * @param id Item ID.
   * @return Item index. Returns -1 If the item is not found.
   */
  private getItemIndex_(id: string): number {
    for (const [i, item] of this.items_.entries()) {
      if (item.id === id) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Requests all panels to dismiss an error item.
   * @param id Item ID.
   */
  private dismissErrorItem_(id: string) {
    const index = this.getItemIndex_(id);
    if (index > -1) {
      this.items_.splice(index, 1);
    }

    for (const panelItem of this.panels_) {
      panelItem.dismissErrorItem(id);
    }
  }

  /**
   * Testing method to construct a new notification panel item.
   * @param props partial properties from the `ProgressCenterItem`.
   */
  private constructTestItem_(props: Partial<ProgressCenterItem> = {}):
      ProgressCenterItem {
    const item = new ProgressCenterItem();
    const defaults = {
      id: Math.ceil(Math.random() * 10000).toString(),
      itemCount: Math.ceil(Math.random() * 5),
      sourceMessage: 'fake_file.test',
      destinationMessage: 'Downloads',
      type: ProgressItemType.COPY,
      progressMax: 100,
    };
    // Apply defaults and overrides.
    Object.assign(item, defaults, props);

    return item;
  }

  /**
   * Testing method to add the notification panel item to the notification
   * panel.
   * @param item the panel item to be added.
   */
  private addItemToPanel_(item: ProgressCenterItem) {
    this.panels_[0]!.setTimingForTests(
        // Make notification panel item show immediately.
        0,
        // Make notification panel item keep showing for 5 minutes.
        5 * 60 * 1000);
    // Add the item to the panel.
    this.items_.push(item);
    this.updateItem(item);
  }

  /**
   * Testing method to add a new "progressing" state notification panel item.
   * @param props partial properties from the `ProgressCenterItem`.
   */
  protected addProcessingTestItem_(props: Partial<ProgressCenterItem> = {}) {
    const item = this.constructTestItem_({
      state: ProgressItemState.PROGRESSING,
      progressValue: Math.ceil(Math.random() * 90),
      remainingTime: 150,
      ...props,
    });
    this.addItemToPanel_(item);
    return item;
  }

  /**
   * Testing method to add a new "completed" state notification panel item.
   * @param props partial properties from the `ProgressCenterItem`.
   */
  protected addCompletedTestItem_(props: Partial<ProgressCenterItem> = {}) {
    const item = this.constructTestItem_({
      state: ProgressItemState.COMPLETED,
      progressValue: 100,
      ...props,
    });
    // Completed item needs to be in the panel before it completes.
    const oldItem = item.clone();
    oldItem.state = ProgressItemState.PROGRESSING;
    this.panels_[0]?.updateItem(oldItem);
    this.addItemToPanel_(item);
    return item;
  }

  /**
   * Testing method to add a new "error" state notification panel item.
   * @param props partial properties from the `ProgressCenterItem`.
   */
  protected addErrorTestItem_(props: Partial<ProgressCenterItem> = {}) {
    const item = this.constructTestItem_({
      state: ProgressItemState.ERROR,
      message: 'Something went wrong. This is a very long error message.',
      ...props,
    });
    item.extraButton.set(ProgressItemState.ERROR, {
      text: 'Learn more',
      callback: () => {},
    });
    this.addItemToPanel_(item);
    return item;
  }

  /**
   * Testing method to add a new "scanning" state notification panel item.
   * @param props partial properties from the `ProgressCenterItem`.
   */
  protected addScanningTestItem_(props: Partial<ProgressCenterItem> = {}) {
    const item = this.constructTestItem_({
      state: ProgressItemState.SCANNING,
      progressValue: Math.ceil(Math.random() * 90),
      remainingTime: 100,
      ...props,
    });
    // Scanning item needs to be in the panel before it starts to scan.
    const oldItem = item.clone();
    this.panels_[0]?.updateItem(oldItem);
    this.addItemToPanel_(item);
    return item;
  }

  /**
   * Testing method to add a new "paused" state notification panel item.
   * @param props partial properties from the `ProgressCenterItem`.
   */
  protected addPausedTestItem_(props: Partial<ProgressCenterItem> = {}) {
    const item = this.constructTestItem_({
      state: ProgressItemState.PAUSED,
      ...props,
    });
    // Paused item needs to be in the panel before it pauses.
    const oldItem = item.clone();
    this.panels_[0]?.updateItem(oldItem);
    this.addItemToPanel_(item);
    return item;
  }
}
