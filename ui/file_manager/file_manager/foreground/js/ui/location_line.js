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
   * Updates breadcrumbs widths in order to truncate it properly.
   */
  truncate() {
    if (!this.breadcrumbs_.firstChild) {
      return;
    }

    // Assume style.width == clientWidth (items have no margins).

    for (let item = this.breadcrumbs_.firstChild; item;
         item = item.nextSibling) {
      item.removeAttribute('style');
      item.removeAttribute('collapsed');
      item.removeAttribute('hidden');
    }

    const containerWidth = this.breadcrumbs_.getBoundingClientRect().width;

    let pathWidth = 0;
    let currentWidth = 0;
    let lastSeparator;
    for (let item = this.breadcrumbs_.firstChild; item;
         item = item.nextSibling) {
      if (item.className == 'separator') {
        pathWidth += currentWidth;
        currentWidth = item.getBoundingClientRect().width;
        lastSeparator = item;
      } else {
        currentWidth += item.getBoundingClientRect().width;
      }
    }
    if (pathWidth + currentWidth <= containerWidth) {
      return;
    }
    if (!lastSeparator) {
      this.breadcrumbs_.lastChild.style.width =
          Math.min(currentWidth, containerWidth) + 'px';
      return;
    }
    const lastCrumbSeparatorWidth = lastSeparator.getBoundingClientRect().width;
    // Current directory name may occupy up to 70% of space or even more if the
    // path is short.
    let maxPathWidth = Math.max(
        Math.round(containerWidth * 0.3), containerWidth - currentWidth);
    maxPathWidth = Math.min(pathWidth, maxPathWidth);

    const parentCrumb = lastSeparator.previousSibling;

    // Pre-calculate the minimum width for crumbs.
    parentCrumb.setAttribute('collapsed', '');
    const minCrumbWidth = parentCrumb.getBoundingClientRect().width;
    parentCrumb.removeAttribute('collapsed');

    let collapsedWidth = 0;
    if (parentCrumb &&
        pathWidth - parentCrumb.getBoundingClientRect().width + minCrumbWidth >
            maxPathWidth) {
      // At least one crumb is hidden completely (or almost completely).
      // Show sign of hidden crumbs like this:
      // root > some di... > ... > current directory.
      parentCrumb.setAttribute('collapsed', '');
      collapsedWidth =
          Math.min(maxPathWidth, parentCrumb.getBoundingClientRect().width);
      maxPathWidth -= collapsedWidth;
      if (parentCrumb.getBoundingClientRect().width != collapsedWidth) {
        parentCrumb.style.width = collapsedWidth + 'px';
      }

      lastSeparator = parentCrumb.previousSibling;
      if (!lastSeparator) {
        return;
      }
      collapsedWidth += lastSeparator.clientWidth;
      maxPathWidth = Math.max(0, maxPathWidth - lastSeparator.clientWidth);
    }

    pathWidth = 0;
    for (let item = this.breadcrumbs_.firstChild; item != lastSeparator;
         item = item.nextSibling) {
      // TODO(serya): Mixing access item.clientWidth and modifying style and
      // attributes could cause multiple layout reflows.
      if (pathWidth === maxPathWidth) {
        item.setAttribute('hidden', '');
      } else {
        if (item.classList.contains('separator')) {
          // If the current separator and the following crumb don't fit in the
          // breadcrumbs area, hide remaining separators and crumbs.
          if (pathWidth + item.getBoundingClientRect().width + minCrumbWidth >
              maxPathWidth) {
            item.setAttribute('hidden', '');
            maxPathWidth = pathWidth;
          } else {
            pathWidth += item.getBoundingClientRect().width;
          }
        } else {
          // If the current crumb doesn't fully fit in the breadcrumbs area,
          // shorten the crumb and hide remaining separators and crums.
          if (pathWidth + item.getBoundingClientRect().width > maxPathWidth) {
            item.style.width = (maxPathWidth - pathWidth) + 'px';
            pathWidth = maxPathWidth;
          } else {
            pathWidth += item.getBoundingClientRect().width;
          }
        }
      }
    }

    currentWidth =
        Math.min(currentWidth, containerWidth - pathWidth - collapsedWidth);
    this.breadcrumbs_.lastChild.style.width =
        (currentWidth - lastCrumbSeparatorWidth) + 'px';
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

  /**
   * Execute an element.
   * @param {number} index The index of clicked path component.
   * @param {!Event} event The MouseEvent object.
   * @private
   */
  onClick_(index, event) {
    let button = event.target;

    // Remove 'focused' state from the clicked button.
    while (button && !button.classList.contains('breadcrumb-path')) {
      button = button.parentElement;
    }
    if (button) {
      button.blur();
    }
    this.navigateToIndex_(index);
  }
}
