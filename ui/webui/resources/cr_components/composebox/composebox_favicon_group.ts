// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './composebox_favicon_group.css.js';
import {getHtml} from './composebox_favicon_group.html.js';

const MAX_DISPLAY_COUNT = 3;

export class ComposeboxFaviconGroupElement extends CrLitElement {
  static get is() {
    return 'composebox-favicon-group';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tabs: {type: Array},
      visibleTabs_: {type: Array},
      remainingCount_: {type: Number},
    };
  }

  accessor tabs: TabInfo[] = [];
  protected accessor visibleTabs_: TabInfo[] = [];
  protected accessor remainingCount_: number = 0;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('tabs')) {
      this.onTabsChanged_();
    }
  }

  private onTabsChanged_() {
    this.visibleTabs_ = this.tabs.slice(0, MAX_DISPLAY_COUNT);
    this.remainingCount_ = Math.max(0, this.tabs.length - MAX_DISPLAY_COUNT);
  }

  protected getFaviconUrl_(url: string): string {
    return getFaviconForPageURL(url, false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'composebox-favicon-group': ComposeboxFaviconGroupElement;
  }
}

customElements.define(
    ComposeboxFaviconGroupElement.is, ComposeboxFaviconGroupElement);
