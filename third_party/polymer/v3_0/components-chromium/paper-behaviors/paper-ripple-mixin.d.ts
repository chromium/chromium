// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {dom} from '../polymer/lib/legacy/polymer.dom.js';
import {PolymerElement} from '../polymer/polymer-element.js';

export {PaperRippleMixin};

/**
 * Note: This file is forked from Polymer's paper-ripple-behavior.d.ts
 *
 * `PaperRippleMixin` dynamically implements a ripple when the element has
 * focus via pointer or keyboard.
 *
 * NOTE: This behavior is intended to be used in conjunction with and after
 * `IronButtonState` and `IronControlState`.
 */
export interface PaperRippleMixinInterface {
  /**
   * If true, the element will not produce a ripple effect when interacted
   * with via the pointer.
   */
  noink: boolean|null|undefined;
  _rippleContainer: Element|null|undefined;

  /**
   * Ensures a `<paper-ripple>` element is available when the element is
   * focused.
   */
  _buttonStateChanged(): void;

  /**
   * In addition to the functionality provided in `IronButtonState`, ensures
   * a ripple effect is created when the element is in a `pressed` state.
   */
  _downHandler(event: any): void;

  /**
   * Ensures this element contains a ripple effect. For startup efficiency
   * the ripple effect is dynamically on demand when needed.
   *
   * @param optTriggeringEvent (optional) event that triggered the
   * ripple.
   */
  ensureRipple(optTriggeringEvent?: Event): void;

  /**
   * Returns the `<paper-ripple>` element used by this element to create
   * ripple effects. The element's ripple is created on demand, when
   * necessary, and calling this method will force the
   * ripple to be created.
   */
  getRipple(): any;

  /**
   * Returns true if this element currently contains a ripple effect.
   */
  hasRipple(): boolean;

  /**
   * Create the element's ripple effect via creating a `<paper-ripple>`.
   * Override this method to customize the ripple element.
   *
   * @returns Returns a `<paper-ripple>` element.
   */
  _createRipple(): PaperRippleElement;
  _noinkChanged(noink: any): void;
}

type Constructor<T> = new (...args: any[]) => T;

declare const PaperRippleMixin: <T extends Constructor<PolymerElement>>(
    superClass: T) => T & Constructor<PaperRippleMixinInterface>;

import {PaperRippleElement} from '../paper-ripple/paper-ripple.js';
