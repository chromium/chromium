// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview CrContainerShadowMixin holds logic for showing a drop shadow
 * near the top of a container element, when the content has scrolled. Inherits
 * from CrScrollObserverMixin.
 *
 * Elements using this mixin are expected to define a #container element which
 * is the element being scrolled.
 *
 * If the #container element has a show-bottom-shadow attribute, a drop shadow
 * will also be shown near the bottom of the container element, when there
 * is additional content to scroll to. Examples:
 *
 * For both top and bottom shadows:
 * <div id="container" show-bottom-shadow>...</div>
 *
 * For top shadow only:
 * <div id="container">...</div>
 *
 * The mixin will take care of inserting an element with ID
 * 'cr-container-shadow-top' which holds the drop shadow effect, and,
 * optionally, an element with ID 'cr-container-shadow-bottom' which holds the
 * same effect. Note that the show-bottom-shadow attribute is inspected only
 * during connectedCallback(), and any changes that occur after that point
 * will not be respected.
 *
 * Clients should either use the existing shared styling in
 * cr_shared_style.css, '#cr-container-shadow-[top/bottom]' and
 * '#cr-container-shadow-top:has(+ #container.can-scroll:not(.scrolled-to-top))'
 * and '#container.can-scroll:not(.scrolled-to-bottom) +
 *     #cr-container-shadow-bottom'
 * or define their own styles.
 */

import {assert} from '//resources/js/assert.js';
import type {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {CrScrollObserverMixinInterface} from './cr_scroll_observer_mixin.js';
import {CrScrollObserverMixin} from './cr_scroll_observer_mixin.js';

export enum CrContainerShadowSide {
  TOP = 'top',
  BOTTOM = 'bottom',
}

type Constructor<T> = new (...args: any[]) => T;

export const CrContainerShadowMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<CrContainerShadowMixinInterface> => {
      const superClassBase = CrScrollObserverMixin(superClass);

      class CrContainerShadowMixin extends superClassBase implements
          CrContainerShadowMixinInterface {
        private dropShadows_: Map<CrContainerShadowSide, HTMLElement> =
            new Map();
        private sides_: CrContainerShadowSide[] = [];

        override connectedCallback() {
          super.connectedCallback();

          const container = this.shadowRoot!.querySelector('#container');
          assert(container);
          const hasBottomShadow = container.hasAttribute('show-bottom-shadow');
          this.sides_ = hasBottomShadow ?
              [CrContainerShadowSide.TOP, CrContainerShadowSide.BOTTOM] :
              [CrContainerShadowSide.TOP];
          this.sides_!.forEach(side => {
            // The element holding the drop shadow effect to be shown.
            const shadow = document.createElement('div');
            shadow.id = `cr-container-shadow-${side}`;
            shadow.classList.add('cr-container-shadow');
            this.dropShadows_.set(side, shadow);
          });

          container.parentNode!.insertBefore(
              this.dropShadows_.get(CrContainerShadowSide.TOP)!, container);
          if (hasBottomShadow) {
            container.parentNode!.insertBefore(
                this.dropShadows_.get(CrContainerShadowSide.BOTTOM)!,
                container.nextSibling);
          }
        }

        /**
         * Toggles the force-shadow class. If |enabled| is true, shadows will be
         * forced to show regardless of scroll state when using the shared
         * styles in cr_shared_style.css. If false, shadows can be shown using
         * classes set by CrScrollObserverMixin.
         */
        setForceDropShadows(enabled: boolean) {
          assert(this.sides_.length > 0);
          for (const side of this.sides_) {
            this.dropShadows_.get(side)!.classList.toggle(
                'force-shadow', enabled);
          }
        }
      }

      return CrContainerShadowMixin;
    });

export interface CrContainerShadowMixinInterface extends
    CrScrollObserverMixinInterface {
  setForceDropShadows(enabled: boolean): void;
}
