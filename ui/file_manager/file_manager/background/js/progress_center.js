// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {ProgressCenterPanelInterface} from '../../externs/progress_center_panel.js';

/**
 * Implementation of {ProgressCenter} at the background page.
 * @implements {ProgressCenter}
 * @final
 */
export class ProgressCenterImpl {
  constructor() {
    /**
     * Current items managed by the progress center.
     * @private @const @type {!Array<!ProgressCenterItem>}
     */
    this.items_ = [];

    /**
     * List of panel UI managed by the progress center.
     * @private @const @type {!Array<ProgressCenterPanelInterface>}
     */
    this.panels_ = [];

    /**
     * Inhibit end of operation updates for testing.
     * @private
     */
    this.neverNotifyCompleted_ = false;
  }

  /**
   * Turns off sending updates when a file operation reaches 'completed' state.
   * Used for testing UI that can be ephemeral otherwise.
   * @public
   */
  neverNotifyCompleted() {
    if (window.IN_TEST) {
      this.neverNotifyCompleted_ = true;
    }
  }

  /**
   * Updates the item in the progress center.
   * If the item has a new ID, the item is added to the item list.
   *
   * @param {ProgressCenterItem} item Updated item.
   */
  updateItem(item) {
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
    for (let i = 0; i < this.panels_.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      this.panels_[i].updateItem(item);
    }
  }

  /**
   * Requests to cancel the progress item.
   * @param {string} id Progress ID to be requested to cancel.
   */
  requestCancel(id) {
    const item = this.getItemById(id);
    if (item && item.cancelCallback) {
      item.cancelCallback();
    }
  }

  /**
   * Adds a panel UI to the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  addPanel(panel) {
    if (this.panels_.indexOf(panel) !== -1) {
      return;
    }

    // Update the panel list.
    this.panels_.push(panel);

    // Set the current items.
    for (let i = 0; i < this.items_.length; i++) {
      // @ts-ignore: error TS2345: Argument of type 'ProgressCenterItem |
      // undefined' is not assignable to parameter of type 'ProgressCenterItem'.
      panel.updateItem(this.items_[i]);
    }

    // Register the cancel callback.
    // @ts-ignore: error TS2339: Property 'cancelCallback' does not exist on
    // type 'ProgressCenterPanelInterface'.
    panel.cancelCallback = this.requestCancel.bind(this);

    // Register the dismiss error item callback.
    // @ts-ignore: error TS2551: Property 'dismissErrorItemCallback' does not
    // exist on type 'ProgressCenterPanelInterface'. Did you mean
    // 'dismissErrorItem'?
    panel.dismissErrorItemCallback = this.dismissErrorItem_.bind(this);
  }

  /**
   * Removes a panel UI from the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  removePanel(panel) {
    const index = this.panels_.indexOf(panel);
    if (index === -1) {
      return;
    }

    this.panels_.splice(index, 1);
    // @ts-ignore: error TS2339: Property 'cancelCallback' does not exist on
    // type 'ProgressCenterPanelInterface'.
    panel.cancelCallback = null;
  }

  /**
   * Obtains item by ID.
   * @param {string} id ID of progress item.
   * @return {ProgressCenterItem|undefined} Progress center item having the
   *     specified ID. Null if the item is not found.
   */
  getItemById(id) {
    return this.items_[this.getItemIndex_(id)];
  }

  /**
   * Obtains item index that have the specifying ID.
   * @param {string} id Item ID.
   * @return {number} Item index. Returns -1 If the item is not found.
   * @private
   */
  getItemIndex_(id) {
    for (let i = 0; i < this.items_.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      if (this.items_[i].id === id) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Requests all panels to dismiss an error item.
   * @param {string} id Item ID.
   * @private
   */
  dismissErrorItem_(id) {
    const index = this.getItemIndex_(id);
    if (index > -1) {
      this.items_.splice(index, 1);
    }

    for (let i = 0; i < this.panels_.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      this.panels_[i].dismissErrorItem(id);
    }
  }

  /**
   * Testing method to construct a new notification panel item.
   * @private
   * @param {Object|undefined} props partial properties from
   *     the {ProgressCenterItem}.
   * @return {!ProgressCenterItem}
   */
  constructTestItem_(props) {
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
   * @private
   * @param {ProgressCenterItem} item the panel item to be added.
   *
   * @suppress {checkTypes} add new property to the struct.
   */
  addItemToPanel_(item) {
    // Make notification panel item show immediately.
    // @ts-ignore: error TS2339: Property 'PENDING_TIME_MS_' does not exist on
    // type 'ProgressCenterPanelInterface'.
    this.panels_[0].pendingTimeMs_ = 0;
    // Make notification panel item keep showing for 5 minutes.
    // @ts-ignore: error TS2339: Property 'TIMEOUT_TO_REMOVE_MS_' does not exist
    // on type 'ProgressCenterPanelInterface'.
    this.panels_[0].timeoutToRemoveMs_ = 5 * 60 * 1000;
    // Add the item to the panel.
    this.items_.push(item);
    this.updateItem(item);
  }

  /**
   * Testing method to add a new "progressing" state notification panel item.
   *
   * @private
   * @param {Object|undefined} props partial properties from
   *     the {ProgressCenterItem}.
   */
  // @ts-ignore: error TS6133: 'addProcessingTestItem_' is declared but its
  // value is never read.
  addProcessingTestItem_(props) {
    // @ts-ignore: error TS2345: Argument of type '{ constructor?: Function |
    // undefined; toString?: (() => string) | undefined; toLocaleString?: (() =>
    // string) | undefined; valueOf?: (() => Object) | undefined;
    // hasOwnProperty?: ((v: PropertyKey) => boolean) | undefined; ... 4 more
    // ...; remainingTime: number; }' is not assignable to parameter of type
    // 'Object'.
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
   *
   * @private
   * @param {Object|undefined} props partial properties from
   *     the {ProgressCenterItem}.
   *
   * @suppress {missingProperties} access private properties.
   */
  // @ts-ignore: error TS6133: 'addCompletedTestItem_' is declared but its value
  // is never read.
  addCompletedTestItem_(props) {
    // @ts-ignore: error TS2345: Argument of type '{ constructor?: Function |
    // undefined; toString?: (() => string) | undefined; toLocaleString?: (() =>
    // string) | undefined; valueOf?: (() => Object) | undefined;
    // hasOwnProperty?: ((v: PropertyKey) => boolean) | undefined;
    // isPrototypeOf?: ((v: Object) => boolean) | undefined;
    // propertyIsEnumerable?: ((v: PropertyKey) =>...' is not assignable to
    // parameter of type 'Object'.
    const item = this.constructTestItem_({
      state: ProgressItemState.COMPLETED,
      progressValue: 100,
      ...props,
    });
    // Completed item needs to be in the panel before it completes.
    const oldItem = item.clone();
    oldItem.state = ProgressItemState.PROGRESSING;
    // @ts-ignore: error TS2339: Property 'items_' does not exist on type
    // 'ProgressCenterPanelInterface'.
    this.panels_[0].items_[item.id] = oldItem;
    this.addItemToPanel_(item);
    return item;
  }

  /**
   * Testing method to add a new "error" state notification panel item.
   *
   * @private
   * @param {Object|undefined} props partial properties from
   *     the {ProgressCenterItem}.
   *
   * @suppress {missingProperties} access private properties.
   */
  // @ts-ignore: error TS6133: 'addErrorTestItem_' is declared but its value is
  // never read.
  addErrorTestItem_(props) {
    // @ts-ignore: error TS2345: Argument of type '{ constructor?: Function |
    // undefined; toString?: (() => string) | undefined; toLocaleString?: (() =>
    // string) | undefined; valueOf?: (() => Object) | undefined;
    // hasOwnProperty?: ((v: PropertyKey) => boolean) | undefined;
    // isPrototypeOf?: ((v: Object) => boolean) | undefined;
    // propertyIsEnumerable?: ((v: PropertyKey) =>...' is not assignable to
    // parameter of type 'Object'.
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
   *
   * @private
   * @param {Object|undefined} props partial properties from
   *     the {ProgressCenterItem}.
   *
   * @suppress {missingProperties} access private properties.
   */
  // @ts-ignore: error TS6133: 'addScanningTestItem_' is declared but its value
  // is never read.
  addScanningTestItem_(props) {
    // @ts-ignore: error TS2345: Argument of type '{ constructor?: Function |
    // undefined; toString?: (() => string) | undefined; toLocaleString?: (() =>
    // string) | undefined; valueOf?: (() => Object) | undefined;
    // hasOwnProperty?: ((v: PropertyKey) => boolean) | undefined; ... 4 more
    // ...; remainingTime: number; }' is not assignable to parameter of type
    // 'Object'.
    const item = this.constructTestItem_({
      state: ProgressItemState.SCANNING,
      progressValue: Math.ceil(Math.random() * 90),
      remainingTime: 100,
      ...props,
    });
    // Scanning item needs to be in the panel before it starts to scan.
    const oldItem = item.clone();
    // @ts-ignore: error TS2339: Property 'items_' does not exist on type
    // 'ProgressCenterPanelInterface'.
    this.panels_[0].items_[item.id] = oldItem;
    this.addItemToPanel_(item);
    return item;
  }

  /**
   * Testing method to add a new "paused" state notification panel item.
   *
   * @private
   * @param {Object|undefined} props partial properties from
   *     the {ProgressCenterItem}.
   *
   * @suppress {missingProperties} access private properties.
   */
  // @ts-ignore: error TS6133: 'addPausedTestItem_' is declared but its value is
  // never read.
  addPausedTestItem_(props) {
    // @ts-ignore: error TS2345: Argument of type '{ constructor?: Function |
    // undefined; toString?: (() => string) | undefined; toLocaleString?: (() =>
    // string) | undefined; valueOf?: (() => Object) | undefined;
    // hasOwnProperty?: ((v: PropertyKey) => boolean) | undefined;
    // isPrototypeOf?: ((v: Object) => boolean) | undefined;
    // propertyIsEnumerable?: ((v: PropertyKey) =>...' is not assignable to
    // parameter of type 'Object'.
    const item = this.constructTestItem_({
      state: ProgressItemState.PAUSED,
      ...props,
    });
    // Paused item needs to be in the panel before it pauses.
    const oldItem = item.clone();
    // @ts-ignore: error TS2339: Property 'items_' does not exist on type
    // 'ProgressCenterPanelInterface'.
    this.panels_[0].items_[item.id] = oldItem;
    this.addItemToPanel_(item);
    return item;
  }
}
