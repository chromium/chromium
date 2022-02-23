// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {metrics} from '../../../common/js/metrics.js';
import {util} from '../../../common/js/util.js';
import {FakeEntry} from '../../../externs/files_app_entry_interfaces.js';
import {VolumeManager} from '../../../externs/volume_manager.js';
import {FilesTooltip} from '../../elements/files_tooltip.js';
import {PathComponent} from '../path_component.js';

import {BreadCrumb} from './breadcrumb.js';
import {ListContainer} from './list_container.js';

/**
 * Location line.
 */
export class LocationLine extends EventTarget {
  /**
   * @param {!Element} breadcrumbs Container element for breadcrumbs.
   * @param {!VolumeManager} volumeManager Volume manager.
   * @param {!ListContainer} listContainer List Container.
   */
  constructor(breadcrumbs, volumeManager, listContainer) {
    super();

    this.breadcrumbs_ = breadcrumbs;
    this.volumeManager_ = volumeManager;
    /** @private {!ListContainer} */
    this.listContainer_ = listContainer;
    this.entry_ = null;
    this.components_ = [];

    /** @private {?FilesTooltip} */
    this.filesTooltip_ = null;
  }

  /**
   * @param {?FilesTooltip} filesTooltip
   * */
  set filesTooltip(filesTooltip) {
    this.filesTooltip_ = filesTooltip;

    this.filesTooltip_.addTargets(
        this.breadcrumbs_.querySelectorAll('[has-tooltip]'));
  }

  /**
   * Shows path of |entry|.
   *
   * @param {!Entry|!FakeEntry} entry Target entry or fake entry.
   */
  show(entry) {
    if (entry === this.entry_) {
      return;
    }

    this.entry_ = entry;

    const components =
        PathComponent.computeComponentsFromEntry(entry, this.volumeManager_);

    // Root "/" paths have no components, crbug.com/1107391.
    if (!components.length) {
      return;
    }

    this.updateNg_(components);
  }

  /**
   * Returns the breadcrumb path components.
   * @return {!Array<!PathComponent>}
   */
  getCurrentPathComponents() {
    return this.components_;
  }

  /**
   * Updates the breadcrumb display for files-ng.
   * @param {!Array<!PathComponent>} components Path components.
   * @private
   */
  updateNg_(components) {
    this.components_ = Array.from(components);

    let breadcrumbs =
        /** @type {!BreadCrumb} */ (document.querySelector('bread-crumb'));
    if (!breadcrumbs) {
      breadcrumbs = document.createElement('bread-crumb');
      breadcrumbs.id = 'breadcrumbs';
      this.breadcrumbs_.appendChild(breadcrumbs);
      breadcrumbs.setSignalCallback(this.breadCrumbSignal_.bind(this));
    }

    let path = components[0].name.replace(/\//g, '%2F');
    for (let i = 1; i < components.length; i++) {
      path += '/' + components[i].name.replace(/\//g, '%2F');
    }

    breadcrumbs.path = path;
    this.breadcrumbs_.hidden = false;
  }

  /**
   * Hide breadcrumbs div.
   */
  hide() {
    this.breadcrumbs_.hidden = true;
  }

  /**
   * Navigate to a Path component.
   * @param {number} index The index of clicked path component.
   */
  navigateToIndex_(index) {
    // Last breadcrumb component is the currently selected folder, skip
    // navigation and just move the focus to file list.
    // TODO(files-ng): this if clause is not used or needed by files-ng.
    if (index >= this.components_.length - 1) {
      this.listContainer_.focus();
      return;
    }

    const pathComponent = this.components_[index];
    pathComponent.resolveEntry().then(entry => {
      const pathClickEvent = new Event('pathclick');
      pathClickEvent.entry = entry;
      this.dispatchEvent(pathClickEvent);
    });
    metrics.recordUserAction('ClickBreadcrumbs');
  }

  /**
   * Signal handler for the bread-crumb element.
   * @param {string} signal Identifier of which bread crumb was activated.
   */
  breadCrumbSignal_(signal) {
    if (typeof signal === 'number') {
      this.navigateToIndex_(Number(signal));
    }
  }
}
