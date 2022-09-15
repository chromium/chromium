// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin to be used by Polymer elements that want to
 * automatically remove WebUI listeners when detached.
 */

import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {addWebUIListener, removeWebUIListener, WebUIListener} from './cr.m.js';

type Constructor<T> = new (...args: any[]) => T;

export const WebUIListenerMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<WebUIListenerMixinInterface> => {
      class WebUIListenerMixin extends superClass implements
          WebUIListenerMixinInterface {
        /**
         * Holds WebUI listeners that need to be removed when this element is
         * destroyed.
         */
        private webUIListeners_: WebUIListener[] = [];

        /**
         * Adds a WebUI listener and registers it for automatic removal when
         * this element is detached. Note: Do not use this method if you intend
         * to remove this listener manually (use addWebUIListener directly
         * instead).
         *
         * @param eventName The event to listen to.
         * @param callback The callback run when the event is fired.
         */
        addWebUIListener(eventName: string, callback: Function) {
          this.webUIListeners_.push(addWebUIListener(eventName, callback));
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          while (this.webUIListeners_.length > 0) {
            removeWebUIListener(this.webUIListeners_.pop()!);
          }
        }
      }
      return WebUIListenerMixin;
    });

export interface WebUIListenerMixinInterface {
  addWebUIListener(eventName: string, callback: Function): void;
}
