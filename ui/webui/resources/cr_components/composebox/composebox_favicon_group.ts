// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL, getUrlForCss} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './composebox_favicon_group.css.js';
import {getHtml} from './composebox_favicon_group.html.js';

// Keep in sync with the per-coin `--skeleton-stagger-delay` rules in
// composebox_favicon_group.css.
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
      submittedTabIds: {type: Object},
    };
  }

  // Tabs currently shown as context chips, in display order.
  accessor tabs: TabInfo[] = [];
  // IDs of the tabs in `tabs` that were already submitted in an earlier turn.
  // Their favicons are not live-updated and never show a loading spinner.
  accessor submittedTabIds: Set<number> = new Set();
  protected accessor visibleTabs_: TabInfo[] = [];
  protected accessor remainingCount_: number = 0;
  private loadedFavicons_: Map<number, {url: string, cssUrl: string}> =
      new Map();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('tabs')) {
      this.onTabsChanged_();
    }
  }

  private getTabUrlStr_(url: string|{url: string}): string {
    return typeof url === 'string' ? url : url.url;
  }

  private onTabsChanged_() {
    this.visibleTabs_ = this.tabs.slice(0, MAX_DISPLAY_COUNT);
    this.remainingCount_ = Math.max(0, this.tabs.length - MAX_DISPLAY_COUNT);

    // Synchronously prune cached favicons when the tab list or a tab's
    // navigation URL changes so same-tab navigations never display the
    // previous page's favicon.
    const nextLoadedFavicons = new Map<number, {url: string, cssUrl: string}>();
    for (const tab of this.visibleTabs_) {
      if (!tab.tabId) {
        continue;
      }
      const existing = this.loadedFavicons_.get(tab.tabId);
      if (existing && existing.url === this.getTabUrlStr_(tab.url)) {
        nextLoadedFavicons.set(tab.tabId, existing);
      }
    }
    this.loadedFavicons_ = nextLoadedFavicons;

    // Asynchronously request updated favicons from C++ for visible tabs that
    // do not have a cached favicon for their current URL. Firing
    // 'wait-for-tab-load' prompts the C++ backend to observe the tab and
    // invoke `onTabLoaded` once the favicon finishes loading.
    for (const tab of this.visibleTabs_) {
      if (tab.tabId && !this.submittedTabIds.has(tab.tabId) &&
          !this.loadedFavicons_.has(tab.tabId)) {
        const expectedUrl = this.getTabUrlStr_(tab.url);
        this.fire('wait-for-tab-load', {
          tabId: tab.tabId,
          onTabLoaded: (faviconDataUrl?: string) => {
            const currentTab =
                this.visibleTabs_.find(item => item.tabId === tab.tabId);
            if (!currentTab ||
                this.getTabUrlStr_(currentTab.url) !== expectedUrl) {
              return;
            }
            const newMap = new Map(this.loadedFavicons_);
            if (faviconDataUrl) {
              newMap.set(
                  tab.tabId,
                  {url: expectedUrl, cssUrl: getUrlForCss(faviconDataUrl)});
            } else {
              newMap.delete(tab.tabId);
            }
            this.loadedFavicons_ = newMap;
            // `loadedFavicons_` is non-reactive internal state updated
            // asynchronously, so explicitly trigger a re-render with the
            // loaded favicon.
            this.requestUpdate();
          },
        });
      }
    }
  }

  // Shows a skeleton coin while the tab strip shows the tab as loading.
  protected isTabLoading_(tab: TabInfo): boolean {
    return tab.isLoading && !this.submittedTabIds.has(tab.tabId);
  }

  protected getFaviconUrl_(tab: TabInfo|string|{url: string, tabId?: number}):
      string {
    if (typeof tab === 'object' && tab !== null) {
      const tabObj = tab as {url: string | {url: string}, tabId?: number};
      const urlStr = this.getTabUrlStr_(tabObj.url);
      const isSubmitted =
          tabObj.tabId && this.submittedTabIds.has(tabObj.tabId);
      const cached = tabObj.tabId && !isSubmitted ?
          this.loadedFavicons_.get(tabObj.tabId) :
          undefined;
      return (cached && cached.url === urlStr ? cached.cssUrl : undefined) ||
          getFaviconForPageURL(urlStr, false);
    }
    return getFaviconForPageURL(tab, false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'composebox-favicon-group': ComposeboxFaviconGroupElement;
  }
}

customElements.define(
    ComposeboxFaviconGroupElement.is, ComposeboxFaviconGroupElement);
