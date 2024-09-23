// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Progress with simple animations. Forked/migrated
 * from Polymer's paper-progress.
 */

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_progress.css.js';
import {getHtml} from './cr_progress.html.js';

export interface CrProgressElement {
  $: {
    primaryProgress: HTMLElement,
  };
}

export class CrProgressElement extends CrLitElement {
  static get is() {
    return 'cr-progress';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The number that represents the current value.
       */
      value: {type: Number},

      /**
       * The number that indicates the minimum value of the range.
       */
      min: {type: Number},

      /**
       * The number that indicates the maximum value of the range.
       */
      max: {type: Number},

      /**
       * Specifies the value granularity of the range's value.
       */
      step: {type: Number},

      /**
       * Use an indeterminate progress indicator.
       */
      indeterminate: {
        type: Boolean,
        reflect: true,
      },

      /**
       * True if the progress is disabled.
       */
      disabled: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  value: number = 0;
  min: number = 0;
  max: number = 100;
  step: number = 1;
  indeterminate: boolean = false;
  disabled: boolean = false;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'progressbar');
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    // Clamp the value to the range.
    if (changedProperties.has('min') || changedProperties.has('max') ||
        changedProperties.has('value') || changedProperties.has('step')) {
      const previous = changedProperties.get('value') || 0;
      const clampedValue = this.clampValue_(this.value);
      this.value = Number.isNaN(clampedValue) ? previous : clampedValue;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('min') || changedProperties.has('max') ||
        changedProperties.has('value') || changedProperties.has('step')) {
      const ratio = (this.value - this.min) / (this.max - this.min);
      this.$.primaryProgress.style.transform = `scaleX(${ratio})`;
      this.setAttribute('aria-valuemin', this.min.toString());
      this.setAttribute('aria-valuemax', this.max.toString());
    }

    if (changedProperties.has('indeterminate') ||
        changedProperties.has('value')) {
      if (this.indeterminate) {
        this.removeAttribute('aria-valuenow');
      } else {
        this.setAttribute('aria-valuenow', this.value.toString());
      }
    }

    if (changedProperties.has('disabled')) {
      this.setAttribute('aria-disabled', this.disabled ? 'true' : 'false');
    }
  }

  private clampValue_(value: number): number {
    return Math.min(this.max, Math.max(this.min, this.calcStep_(value)));
  }

  private calcStep_(value: number): number {
    value = Number.parseFloat(value.toString());

    if (!this.step) {
      return value;
    }

    const numSteps = Math.round((value - this.min) / this.step);
    if (this.step < 1) {
      /**
       * For small values of this.step, if we calculate the step using
       * `Math.round(value / step) * step` we may hit a precision point issue
       * eg. 0.1 * 0.2 =  0.020000000000000004
       * http://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
       *
       * as a work around we can divide by the reciprocal of `step`
       */
      return numSteps / (1 / this.step) + this.min;
    } else {
      return numSteps * this.step + this.min;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-progress': CrProgressElement;
  }
}

customElements.define(CrProgressElement.is, CrProgressElement);
