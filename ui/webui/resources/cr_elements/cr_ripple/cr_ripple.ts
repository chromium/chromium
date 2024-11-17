// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_ripple.css.js';

const MAX_RADIUS_PX: number = 300;
const MIN_DURATION_MS: number = 800;

/** @return The distance between (x1, y1) and (x2, y2). */
function distance(x1: number, y1: number, x2: number, y2: number): number {
  const xDelta = x1 - x2;
  const yDelta = y1 - y2;
  return Math.sqrt(xDelta * xDelta + yDelta * yDelta);
}

export class CrRippleElement extends CrLitElement {
  static get is() {
    return 'cr-ripple';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      holdDown: {type: Boolean},
      recenters: {type: Boolean},
      noink: {type: Boolean},
    };
  }

  holdDown: boolean = false;
  recenters: boolean = false;
  noink: boolean = false;

  private ripples_: Element[] = [];
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    assert(this.parentNode);
    const keyEventTarget =
        this.parentNode.nodeType === Node.DOCUMENT_FRAGMENT_NODE ?
        (this.parentNode as ShadowRoot).host :
        this.parentElement!;

    this.eventTracker_.add(keyEventTarget, 'pointerdown', (e: Event) => {
      const handled = this.uiDownAction(e as PointerEvent);

      if (!handled) {
        return;
      }

      this.eventTracker_.add(keyEventTarget, 'pointermove', (e: Event) => {
        // Only call setPointerCapture() if 'pointermove' happens and not
        // in 'pointerdown', so that clicking any links or other elements
        // within the parent works as expected.
        this.setPointerCapture((e as PointerEvent).pointerId);
        this.eventTracker_.remove(keyEventTarget, 'pointermove');
      });
    });

    const cancelOrUp = (e: Event) => {
      this.eventTracker_.remove(keyEventTarget, 'pointermove');
      this.uiUpAction(e as PointerEvent);
    };

    this.eventTracker_.add(keyEventTarget, 'pointercancel', cancelOrUp);
    this.eventTracker_.add(keyEventTarget, 'pointerup', cancelOrUp);

    this.eventTracker_.add(keyEventTarget, 'keydown', (e: KeyboardEvent) => {
      if (e.defaultPrevented) {
        return;
      }

      if (e.key === 'Enter') {
        this.onEnterKeydown_();
        return;
      }

      if (e.key === ' ') {
        this.onSpaceKeydown_();
      }
    });

    this.eventTracker_.add(keyEventTarget, 'keyup', (e: KeyboardEvent) => {
      if (e.defaultPrevented) {
        return;
      }

      if (e.key === ' ') {
        this.onSpaceKeyup_();
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('holdDown')) {
      this.holdDownChanged_(this.holdDown, changedProperties.get('holdDown'));
    }
  }

  uiDownAction(e?: PointerEvent): boolean {
    if (e !== undefined && e.button !== 0) {
      // Ignore secondary mouse button clicks.
      return false;
    }

    if (this.noink) {
      return false;
    }

    this.downAction_(e);
    return true;
  }

  private downAction_(e?: PointerEvent) {
    if (this.ripples_.length && this.holdDown) {
      return;
    }

    this.showRipple_(e);
  }

  clear() {
    this.hideRipple_();
    this.holdDown = false;
  }

  showAndHoldDown() {
    this.ripples_.forEach(ripple => {
      ripple.remove();
    });
    this.ripples_ = [];
    this.holdDown = true;
  }

  private showRipple_(e?: PointerEvent) {
    const rect = this.getBoundingClientRect();

    const roundedCenterX = function() {
      return Math.round(rect.width / 2);
    };
    const roundedCenterY = function() {
      return Math.round(rect.height / 2);
    };

    let x = 0;
    let y = 0;
    const centered = !e;
    if (centered) {
      x = roundedCenterX();
      y = roundedCenterY();
    } else {
      x = Math.round(e.clientX - rect.left);
      y = Math.round(e.clientY - rect.top);
    }

    const corners = [
      {x: 0, y: 0},
      {x: rect.width, y: 0},
      {x: 0, y: rect.height},
      {x: rect.width, y: rect.height},
    ];

    const cornerDistances = corners.map(function(corner) {
      return Math.round(distance(x, y, corner.x, corner.y));
    });

    const radius =
        Math.min(MAX_RADIUS_PX, Math.max.apply(Math, cornerDistances));

    const startTranslate = `${x - radius}px, ${y - radius}px`;
    let endTranslate = startTranslate;
    if (this.recenters && !centered) {
      endTranslate =
          `${roundedCenterX() - radius}px, ${roundedCenterY() - radius}px`;
    }

    const ripple = document.createElement('div');
    ripple.classList.add('ripple');
    ripple.style.height = ripple.style.width = (2 * radius) + 'px';

    this.ripples_.push(ripple);
    this.shadowRoot!.appendChild(ripple);

    ripple.animate(
        {
          transform: [
            `translate(${startTranslate}) scale(0)`,
            `translate(${endTranslate}) scale(1)`,
          ],
        },
        {
          duration: Math.max(MIN_DURATION_MS, Math.log(radius) * radius) || 0,
          easing: 'cubic-bezier(.2, .9, .1, .9)',
          fill: 'forwards',
        });
  }

  uiUpAction(e?: PointerEvent) {
    if (this.noink) {
      return;
    }

    this.upAction_(e);
  }

  private upAction_(e?: PointerEvent) {
    if (this.holdDown) {
      return;
    }

    this.hideRipple_(e);
  }

  private hideRipple_(e?: PointerEvent) {
    if (e !== undefined && this.hasPointerCapture(e.pointerId)) {
      this.releasePointerCapture(e.pointerId);
    }

    if (this.ripples_.length === 0) {
      return;
    }

    this.ripples_.forEach(function(ripple) {
      const opacity =
          ripple.computedStyleMap().get('opacity') as CSSUnitValue | null;
      if (opacity === null) {
        ripple.remove();
        return;
      }

      const animation = ripple.animate(
          {
            opacity: [opacity!.value!, 0],
          },
          {
            duration: 150,
            fill: 'forwards',
          });
      animation.finished.then(() => {
        ripple.remove();
      });
    });
    this.ripples_ = [];
  }

  private onEnterKeydown_() {
    this.uiDownAction();
    window.setTimeout(() => {
      this.uiUpAction();
    }, 1);
  }

  private onSpaceKeydown_() {
    this.uiDownAction();
  }

  private onSpaceKeyup_() {
    this.uiUpAction();
  }

  private holdDownChanged_(
      newHoldDown: boolean, oldHoldDown: boolean|undefined) {
    if (oldHoldDown === undefined) {
      return;
    }
    if (newHoldDown) {
      this.downAction_();
    } else {
      this.upAction_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-ripple': CrRippleElement;
  }
}

customElements.define(CrRippleElement.is, CrRippleElement);
