// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';

function safeDecodeURIComponent(s: string): string {
  try {
    return window.decodeURIComponent(s);
  } catch (_e) {
    // If the string can't be decoded, return it verbatim.
    return s;
  }
}

function getCurrentPathname(): string {
  return safeDecodeURIComponent(window.location.pathname);
}

function getCurrentHash(): string {
  return safeDecodeURIComponent(window.location.hash.slice(1));
}

let instance: CrRouter|null = null;

export class CrRouter extends EventTarget {
  private path_: string = getCurrentPathname();
  private query_: string = window.location.search.slice(1);
  private hash_: string = getCurrentHash();

  /**
   * If the user was on a URL for less than `dwellTime_` milliseconds, it
   * won't be added to the browser's history, but instead will be replaced
   * by the next entry.
   *
   * This is to prevent large numbers of entries from clogging up the user's
   * browser history. Disable by setting to a negative number.
   */
  private dwellTime_: number = 2000;

  private lastChangedAt_: number;

  constructor() {
    super();
    this.lastChangedAt_ = window.performance.now() - (this.dwellTime_ - 200);
    window.addEventListener('hashchange', () => this.hashChanged_());
    window.addEventListener('popstate', () => this.urlChanged_());
  }

  setDwellTime(dwellTime: number) {
    this.dwellTime_ = dwellTime;
    this.lastChangedAt_ = window.performance.now() - this.dwellTime_;
  }

  getPath(): string {
    return this.path_;
  }

  getQueryParams(): URLSearchParams {
    return new URLSearchParams(this.query_);
  }

  getHash(): string {
    return this.hash_;
  }

  setHash(hash: string) {
    this.hash_ = safeDecodeURIComponent(hash);
    if (this.hash_ !== getCurrentHash()) {
      this.updateState_();
    }
  }

  setQueryParams(params: URLSearchParams) {
    this.query_ = params.toString();
    if (this.query_ !== window.location.search.substring(1)) {
      this.updateState_();
    }
  }

  setPath(path: string) {
    assert(path.startsWith('/'));
    this.path_ = safeDecodeURIComponent(path);
    if (this.path_ !== getCurrentPathname()) {
      this.updateState_();
    }
  }

  private hashChanged_() {
    const oldHash = this.hash_;
    this.hash_ = getCurrentHash();
    if (this.hash_ !== oldHash) {
      this.dispatchEvent(new CustomEvent(
          'cr-router-hash-changed',
          {bubbles: true, composed: true, detail: this.hash_}));
    }
  }

  // Dispatches cr-router-*-changed events if portions of the URL change from
  // window events.
  private urlChanged_() {
    this.hashChanged_();

    const oldPath = this.path_;
    this.path_ = getCurrentPathname();
    if (oldPath !== this.path_) {
      this.dispatchEvent(new CustomEvent(
          'cr-router-path-changed',
          {bubbles: true, composed: true, detail: this.path_}));
    }

    const oldQuery = this.query_;
    this.query_ = window.location.search.substring(1);
    if (oldQuery !== this.query_) {
      this.dispatchEvent(new CustomEvent(
          'cr-router-query-params-changed',
          {bubbles: true, composed: true, detail: this.getQueryParams()}));
    }
  }

  // Updates the window history state if the URL is updated from setters.
  private updateState_() {
    const url = new URL(window.location.origin);
    const pathPieces = this.path_.split('/');
    url.pathname =
        pathPieces.map(piece => window.encodeURIComponent(piece)).join('/');
    if (this.query_) {
      url.search = this.query_;
    }
    if (this.hash_) {
      url.hash = window.encodeURIComponent(this.hash_);
    }

    const now = window.performance.now();
    const shouldReplace = this.lastChangedAt_ + this.dwellTime_ > now;
    this.lastChangedAt_ = now;

    if (shouldReplace) {
      window.history.replaceState({}, '', url.href);
    } else {
      window.history.pushState({}, '', url.href);
    }
  }

  static getInstance(): CrRouter {
    return instance || (instance = new CrRouter());
  }

  static resetForTesting() {
    instance = null;
  }
}
