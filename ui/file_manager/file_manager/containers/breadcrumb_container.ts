// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../widgets/xf_breadcrumb.js';

import {metrics} from '../common/js/metrics.js';
import {CurrentDirectory, PropStatus, State} from '../externs/ts/state.js';
import {changeDirectory} from '../state/actions.js';
import {FileKey} from '../state/file_key.js';
import {getStore, Store} from '../state/store.js';
import {BREADCRUMB_CLICKED, BreadcrumbClickedEvent} from '../widgets/xf_breadcrumb.js';

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
    const currentDir = state.currentDirectory;
    const key = currentDir && currentDir.key;
    if (!key || !currentDir) {
      this.hide_();
      return;
    }

    if (currentDir.status == PropStatus.SUCCESS &&
        this.currentFileKey_ !== key) {
      this.show_(state.currentDirectory);
    }
  }

  private hide_() {
    const breadcrumb = document.querySelector('xf-breadcrumb');
    if (breadcrumb) {
      breadcrumb.hidden = true;
    }
  }

  private show_(currentDir?: CurrentDirectory) {
    let breadcrumb = document.querySelector('xf-breadcrumb');
    if (!breadcrumb) {
      breadcrumb = document.createElement('xf-breadcrumb');
      breadcrumb.addEventListener(
          BREADCRUMB_CLICKED, this.breadcrumbClick_.bind(this));
      this.container_.appendChild(breadcrumb);
    }

    const path = !currentDir ?
        '' :
        currentDir.pathComponents.map(p => p.label).join('/');
    breadcrumb!.path = path;
    this.currentFileKey_ = currentDir ? currentDir.key : null;
    this.pathKeys_ =
        currentDir ? currentDir.pathComponents.map(p => p.key) : [];
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
    metrics.recordUserAction('ClickBreadcrumbs');
  }
}
