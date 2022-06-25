// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic common to components that support a help bubble.
 */

import {assert} from 'chrome://resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HelpBubbleElement} from './help_bubble.js';
import {HelpBubbleClientCallbackRouter, HelpBubbleHandlerInterface, HelpBubbleParams} from './help_bubble.mojom-webui.js';
import {HelpBubbleProxyImpl} from './help_bubble_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

export const HelpBubbleMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<HelpBubbleMixinInterface> => {
      class HelpBubbleMixin extends superClass implements
          HelpBubbleMixinInterface {
        private helpBubbleHandler_: HelpBubbleHandlerInterface;
        private helpBubbleCallbackRouter_: HelpBubbleClientCallbackRouter;
        /**
         * A map from the name of the native identifier used in the tutorial or
         * IPH definition to the target element ID the help bubble should attach
         * to.
         *
         * Example entry:
         *   "kHeightenSecuritySettingsElementId" => "toggleSecureMode"
         */
        private helpBubbleNativeToTargetId_: Map<string, string> = new Map();
        private pendingHelpBubbleParams_: HelpBubbleParams|null = null;
        private listenerIds_: Array<number> = [];
        private helpBubbleHostDisplayObserver_: IntersectionObserver|null =
            null;
        private helpBubbleHostWasVisible_: boolean = false;

        constructor(...args: any[]) {
          super(...args);

          this.helpBubbleHandler_ =
              HelpBubbleProxyImpl.getInstance().getHandler();
          this.helpBubbleCallbackRouter_ =
              HelpBubbleProxyImpl.getInstance().getCallbackRouter();
        }

        override connectedCallback() {
          super.connectedCallback();

          const router = this.helpBubbleCallbackRouter_;
          this.listenerIds_.push(
              router.showHelpBubble.addListener(
                  this.onShowHelpBubble_.bind(this)),
              router.toggleFocusForAccessibility.addListener(
                  this.onToggleHelpBubbleFocusForAccessibility_.bind(this)),
              router.hideHelpBubble.addListener(
                  this.onHideHelpBubble_.bind(this)));

          this.helpBubbleHostDisplayObserver_ =
              new IntersectionObserver((entries, _observer) => {
                const isVisible = entries[0].isIntersecting;
                if (isVisible !== this.helpBubbleHostWasVisible_) {
                  this.helpBubbleHostWasVisible_ = isVisible;
                  if (this.hideHelpBubble()) {
                    // Help bubble was closed but not via a close button.
                    this.helpBubbleHandler_.helpBubbleClosed(false);
                  }
                  this.helpBubbleHandler_.helpBubbleHostVisibilityChanged(
                      isVisible);
                }
              }, {root: document.body});
          this.helpBubbleHostDisplayObserver_.observe(this);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          for (const listenerId of this.listenerIds_) {
            this.helpBubbleCallbackRouter_.removeListener(listenerId);
          }
          this.listenerIds_ = new Array();
          if (this.helpBubbleHostDisplayObserver_) {
            this.helpBubbleHostDisplayObserver_.disconnect();
            this.helpBubbleHostDisplayObserver_ = null;
          }
        }

        registerHelpBubbleIdentifier(nativeId: string, htmlId: string): void {
          assert(!this.helpBubbleNativeToTargetId_.has(nativeId));
          this.helpBubbleNativeToTargetId_.set(nativeId, htmlId);

          if (this.pendingHelpBubbleParams_ &&
              this.pendingHelpBubbleParams_.nativeIdentifier === nativeId) {
            this.onShowHelpBubble_(this.pendingHelpBubbleParams_);
          }
        }

        isHelpBubbleShowing(): boolean {
          const bubble = this.getHelpBubble_();
          return !!bubble && bubble.open;
        }

        showHelpBubble(anchorId: string, params: HelpBubbleParams): boolean {
          const bubble = this.getHelpBubble_();
          if (!bubble || bubble.open) {
            return false;
          }

          bubble.anchorId = anchorId;
          bubble.position = params.position;
          bubble.body = params.bodyText;
          bubble.show();
          return true;
        }

        hideHelpBubble(): boolean {
          const bubble = this.getHelpBubble_();
          if (!bubble || !bubble.open) {
            return false;
          }

          bubble.hide();
          return true;
        }

        private onShowHelpBubble_(params: HelpBubbleParams): void {
          if (!this.helpBubbleNativeToTargetId_.has(params.nativeIdentifier)) {
            this.pendingHelpBubbleParams_ = params;
            return;
          }
          this.pendingHelpBubbleParams_ = null;

          const bubble = this.getHelpBubble_();
          if (!bubble) {
            return;
          }

          if (bubble.open) {
            bubble.hide();
          }

          const anchorId: string =
              this.helpBubbleNativeToTargetId_.get(params.nativeIdentifier)!;

          this.showHelpBubble(anchorId, params);
        }

        private onToggleHelpBubbleFocusForAccessibility_() {
          const bubble = this.getHelpBubble_();
          if (!bubble) {
            return;
          }

          const anchorElement = bubble.getAnchorElement();
          if (anchorElement) {
            anchorElement.focus();
          }
        }

        private onHideHelpBubble_(): void {
          this.pendingHelpBubbleParams_ = null;
          this.hideHelpBubble();
        }

        private getHelpBubble_(): HelpBubbleElement|null {
          return this.shadowRoot!.querySelector('help-bubble');
        }
      }

      return HelpBubbleMixin;
    });

export interface HelpBubbleMixinInterface {
  registerHelpBubbleIdentifier(nativeId: string, htmlId: string): void;
  isHelpBubbleShowing(): boolean;
  showHelpBubble(anchorId: string, params: HelpBubbleParams): boolean;
  hideHelpBubble(): boolean;
}
