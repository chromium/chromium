// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mixin to be used by CrLitElement subclasses that want to
 * automatically remove WebUI listeners when detached.
 */

import type {WebUiListener} from '//resources/js/cr.js';
import {addWebUiListener, removeWebUiListener} from '//resources/js/cr.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

type Constructor<T> = new (...args: any[]) => T;

export const WebUiListenerMixinLit =
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<WebUiListenerMixinLitInterface> => {
      class WebUiListenerMixinLit extends superClass implements
          WebUiListenerMixinLitInterface {
        /**
         * Holds WebUI listeners that need to be removed when this element is
         * destroyed.
         */
        private webUiListeners_: WebUiListener[] = [];

        /**
         * Adds a WebUI listener and registers it for automatic removal when
         * this element is detached. Note: Do not use this method if you intend
         * to remove this listener manually (use addWebUiListener directly
         * instead).
         *
         * @param eventName The event to listen to.
         * @param callback The callback run when the event is fired.
         */
        addWebUiListener(eventName: string, callback: Function) {
          this.webUiListeners_.push(addWebUiListener(eventName, callback));
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          while (this.webUiListeners_.length > 0) {
            removeWebUiListener(this.webUiListeners_.pop()!);
          }
        }
      }
      return WebUiListenerMixinLit;
    };

export interface WebUiListenerMixinLitInterface {
  addWebUiListener(eventName: string, callback: Function): void;
}
