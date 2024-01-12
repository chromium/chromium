// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../widgets/xf_breadcrumb.js';

import {recordUserAction} from '../common/js/metrics.js';
import {str} from '../common/js/translations.js';
import {SEARCH_RESULTS_KEY} from '../common/js/url_constants.js';
import {changeDirectory} from '../state/ducks/current_directory.js';
import type {FileKey} from '../state/file_key.js';
import {type PathComponent, PropStatus, type State} from '../state/state.js';
import {getStore, getVolumeType, type Store} from '../state/store.js';
import {type BreadcrumbClickedEvent, XfBreadcrumb} from '../widgets/xf_breadcrumb.js';

/**
 * The controller of breadcrumb. The Breadcrumb element only renders a given
 * path. This controller component is responsible for constructing the path
 * and passing it to the Breadcrumb element.
 */
export class BreadcrumbContainer {
  private store_: Store;
  private currentFileKey_: FileKey|null;
  private container_: HTMLElement;
  private pathKeys_: FileKey[];

  constructor(container: HTMLElement) {
    this.container_ = container;
    this.store_ = getStore();
    this.store_.subscribe(this);
    this.currentFileKey_ = null;
    this.pathKeys_ = [];
  }

  onStateChanged(state: State) {
    const {currentDirectory, search} = state;
    let key = currentDirectory && currentDirectory.key;
    if (!key || !currentDirectory) {
      this.hide_();
      return;
    }

    if (search && search.status !== undefined) {
      // Search results do not have the corresponding directory in the
      // directory tree. When V2 version of search is active, we short-circuit
      // the process to show the correct label and exit.
      this.show_(SEARCH_RESULTS_KEY, [
        {
          name: 'search',
          label: str('SEARCH_RESULTS_LABEL'),
          key: SEARCH_RESULTS_KEY,
        },
      ]);
      return;
    }

    // If the current location is somewhere in Drive, all files in Drive can
    // be listed as search results regardless of current location.
    // In this case, showing current location is confusing, so use the Drive
    // root "My Drive" as the current location.
    if (search && search.query && search.status === PropStatus.SUCCESS) {
      const fileData = state.allEntries[currentDirectory.key];
      if (getVolumeType(state, fileData)) {
        const root = currentDirectory.pathComponents[0];
        if (root) {
          key = root.key;
          this.show_(root.key!, [root]);
          return;
        }
      }
    }

    if (currentDirectory.status === PropStatus.SUCCESS &&
        this.currentFileKey_ !== key) {
      this.show_(
          state.currentDirectory?.key || '',
          state.currentDirectory?.pathComponents || []);
    }
  }

  private hide_() {
    const breadcrumb = document.querySelector('xf-breadcrumb');
    if (breadcrumb) {
      breadcrumb.hidden = true;
    }
  }

  private show_(key: FileKey, pathComponents: PathComponent[]) {
    let breadcrumb = document.querySelector('xf-breadcrumb');
    if (!breadcrumb) {
      breadcrumb = document.createElement('xf-breadcrumb');
      breadcrumb.id = 'breadcrumbs';
      breadcrumb.addEventListener(
          XfBreadcrumb.events.BREADCRUMB_CLICKED,
          this.breadcrumbClick_.bind(this));
      this.container_.appendChild(breadcrumb);
    }

    const path =
        pathComponents.map(p => p.label.replace(/\//g, '%2F')).join('/');
    breadcrumb!.path = path;
    this.currentFileKey_ = key;
    this.pathKeys_ = pathComponents.map(p => p.key);
  }

  private breadcrumbClick_(event: BreadcrumbClickedEvent) {
    const index = Number(event.detail.partIndex);
    if (isNaN(index) || index < 0) {
      return;
    }
    // The leaf path isn't clickable.
    if (index >= this.pathKeys_.length - 1) {
      return;
    }

    const fileKey = this.pathKeys_[index];
    this.store_.dispatch(changeDirectory({toKey: fileKey as FileKey}));
    recordUserAction('ClickBreadcrumbs');
  }
}
