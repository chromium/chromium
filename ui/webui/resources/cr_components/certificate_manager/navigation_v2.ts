// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Page navigation utility code.
 */

import {assert, assertNotReached} from '//resources/js/assert.js';
import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * The different pages that can be shown.
 */
export enum Page {
  LOCAL_CERTS = 'localcerts',
  CLIENT_CERTS = 'clientcerts',
  CRS_CERTS = 'crscerts',
  // Sub-pages
  ADMIN_CERTS = 'localcerts/admincerts',
  PLATFORM_CERTS = 'localcerts/platformcerts',
  PLATFORM_CLIENT_CERTS = 'clientcerts/platformclientcerts',
}

export class Route {
  constructor(page: Page) {
    this.page = page;
  }

  page: Page;

  path(): string {
    return '/' + this.page;
  }

  isSubpage(): boolean {
    switch (this.page) {
      case Page.ADMIN_CERTS:
      case Page.PLATFORM_CERTS:
      case Page.PLATFORM_CLIENT_CERTS:
        return true;
      case Page.LOCAL_CERTS:
      case Page.CLIENT_CERTS:
      case Page.CRS_CERTS:
        return false;
    }
  }
}

/**
 * A helper object to manage in-page navigations.
 */
export class Router {
  static getInstance(): Router {
    return routerInstance || (routerInstance = new Router());
  }

  private currentRoute_: Route = new Route(Page.LOCAL_CERTS);
  private previousRoute_: Route|null = null;
  private routeObservers_: Set<RouteObserverMixinInterface> = new Set();

  constructor() {
    this.processRoute_();

    window.addEventListener('popstate', () => {
      this.processRoute_();
    });
  }

  addObserver(observer: RouteObserverMixinInterface) {
    assert(!this.routeObservers_.has(observer));
    this.routeObservers_.add(observer);
  }

  removeObserver(observer: RouteObserverMixinInterface) {
    assert(this.routeObservers_.delete(observer));
  }

  get currentRoute(): Route {
    return this.currentRoute_;
  }

  get previousRoute(): Route|null {
    return this.previousRoute_;
  }

  /**
   * Navigates to a page and pushes a new history entry.
   */
  navigateTo(page: Page) {
    const newRoute = new Route(page);
    if (this.currentRoute_.path() === newRoute.path()) {
      return;
    }

    const oldRoute = this.currentRoute_;
    this.currentRoute_ = newRoute;
    const path = this.currentRoute_.path();
    const state = {url: path};
    history.pushState(state, '', path);
    this.notifyObservers_(oldRoute);
  }

  private notifyObservers_(oldRoute: Route) {
    assert(oldRoute !== this.currentRoute_);
    this.previousRoute_ = oldRoute;

    for (const observer of this.routeObservers_) {
      observer.currentRouteChanged(this.currentRoute_, oldRoute);
    }
  }

  static getPageFromPath(path: string): Page|undefined {
    const page = path.substring(1) as Page;
    return Object.values(Page).includes(page) ? page : undefined;
  }

  /**
   * Helper function to set the current page from the path and notify all
   * observers.
   */
  private processRoute_() {
    const page = Router.getPageFromPath(location.pathname);

    if (!page) {
      return;
    }

    const oldRoute = this.currentRoute_;
    this.currentRoute_ = new Route(oldRoute.page);
    this.currentRoute_.page = page;
    this.notifyObservers_(oldRoute);
  }
}

let routerInstance: Router|null = null;

type Constructor<T> = new (...args: any[]) => T;

export const RouteObserverMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<RouteObserverMixinInterface> => {
      class RouteObserverMixin extends superClass {
        override connectedCallback() {
          super.connectedCallback();

          Router.getInstance().addObserver(this);

          this.currentRouteChanged(
              Router.getInstance().currentRoute,
              Router.getInstance().currentRoute);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          Router.getInstance().removeObserver(this);
        }

        currentRouteChanged(_newRoute: Route, _oldRoute?: Route): void {
          assertNotReached();
        }
      }

      return RouteObserverMixin;
    });

export interface RouteObserverMixinInterface {
  currentRouteChanged(newRoute: Route, oldRoute?: Route): void;
}
