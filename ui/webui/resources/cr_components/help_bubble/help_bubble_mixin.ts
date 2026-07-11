// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic common to components that support a help bubble.
 *
 * A component implementing this mixin should call
 * registerHelpBubble() to associate specific element identifiers
 * referenced  in an IPH or Tutorials journey with the ids of the HTML elements
 * that journey cares about (typically, points for help bubbles to anchor to).
 *
 * Multiple components in the same WebUI may have this mixin. Each mixin will
 * receive ALL help bubble-related messages from its associated WebUIController
 * and determines if any given message is relevant. This is done by checking
 * against registered identifier.
 *
 * See README.md for more information.
 */

import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {HelpBubbleElement} from './help_bubble.js';
import type {BrowserProxy, HelpBubbleParams} from './help_bubble.mojom-webui.js';
import {browserProxyFactory} from './help_bubble.mojom-webui.js';
import type {HelpBubbleController, HelpBubbleOptions, Trackable} from './help_bubble_controller.js';
import {HelpBubbleMixinCommon} from './help_bubble_mixin_common.js';
import type {HelpBubbleMixinInterface} from './help_bubble_mixin_interface.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * See HelpBubbleMixinInterface for documentation.
 */
export const HelpBubbleMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<HelpBubbleMixinInterface> => {
      class HelpBubbleMixin extends superClass implements
          HelpBubbleMixinInterface {
        private impl_: HelpBubbleMixinCommon;

        constructor(...args: any[]) {
          super(...args);
          this.impl_ =
              new HelpBubbleMixinCommon(this.createHelpBubbleProxy(), this);
        }

        createHelpBubbleProxy(): BrowserProxy {
          return browserProxyFactory.getInstance();
        }

        override connectedCallback() {
          super.connectedCallback();
          this.impl_.onConnected();
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          this.impl_.onDisconnected();
        }

        registerHelpBubble(
            nativeId: string, trackable: Trackable,
            options: HelpBubbleOptions = {}): HelpBubbleController|null {
          return this.impl_.registerHelpBubble(nativeId, trackable, options);
        }

        unregisterHelpBubble(nativeId: string): void {
          this.impl_.unregisterHelpBubble(nativeId);
        }

        isHelpBubbleShowing(): boolean {
          return this.impl_.isHelpBubbleShowing();
        }

        isHelpBubbleShowingForTesting(id: string): boolean {
          return this.impl_.isHelpBubbleShowingForTesting(id);  // IN-TEST
        }

        getHelpBubbleForTesting(id: string): HelpBubbleElement|null {
          return this.impl_.getHelpBubbleForTesting(id);  // IN-TEST
        }

        getSortedAnchorStatusesForTesting(): Array<[string, boolean]> {
          return this.impl_.getSortedAnchorStatusesForTesting();  // IN-TEST
        }

        canShowHelpBubble(controller: HelpBubbleController): boolean {
          return this.impl_.canShowHelpBubble(controller);
        }

        showHelpBubble(
            controller: HelpBubbleController, params: HelpBubbleParams): void {
          this.impl_.showHelpBubble(controller, params);
        }

        hideHelpBubble(nativeId: string): boolean {
          return this.impl_.hideHelpBubble(nativeId);
        }

        notifyHelpBubbleAnchorActivated(nativeId: string): boolean {
          return this.impl_.notifyHelpBubbleAnchorActivated(nativeId);
        }

        notifyHelpBubbleAnchorCustomEvent(
            nativeId: string, customEvent: string): boolean {
          return this.impl_.notifyHelpBubbleAnchorCustomEvent(
              nativeId, customEvent);
        }
      }

      return HelpBubbleMixin;
    });
