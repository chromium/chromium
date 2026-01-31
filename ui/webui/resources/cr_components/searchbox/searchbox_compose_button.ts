// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './searchbox_compose_button.css.js';
import {getHtml} from './searchbox_compose_button.html.js';

export interface SearchboxComposeButtonElement {
  $: {
    glowAnimationWrapper: HTMLElement,
  };
}

const SearchboxComposeButtonElementBase = I18nMixinLit(CrLitElement);

export class SearchboxComposeButtonElement extends
    SearchboxComposeButtonElementBase {
  private aimButtonAngle = 0;
  private currentAngle = 0;
  private velocity = 0;
  private animationFrame = 0;
  private rotationEase = 0.01;
  private friction = 0.9;
  private then = performance.now();

  static get is() {
    return 'cr-searchbox-compose-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      composeIcon_: {
        type: String,
        reflect: true,
      },
      showAnimation_: {
        type: Boolean,
        reflect: true,
      },
      ntpRealboxNextEnabled_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected accessor ntpRealboxNextEnabled_: boolean =
      loadTimeData.getBoolean('ntpRealboxNextEnabled');

  protected accessor composeIcon_: string =
      '//resources/cr_components/searchbox/icons/search_spark.svg';

  protected accessor showAnimation_: boolean =
      loadTimeData.getBoolean('searchboxShowComposeAnimation');

  override firstUpdated() {
    if (this.$.glowAnimationWrapper) {
      if (this.ntpRealboxNextEnabled_) {
        this.$.glowAnimationWrapper.addEventListener(
            'mouseenter', this.onMouseEnter_);
        this.$.glowAnimationWrapper.addEventListener(
            'mouseleave', this.onMouseLeave_);
      }

      if (!this.showAnimation_) {
        this.$.glowAnimationWrapper.classList.remove('play');
      } else {
        this.$.glowAnimationWrapper.addEventListener('animationend', () => {
          this.$.glowAnimationWrapper.classList.remove('play');
        });
      }
    }
  }

  protected onMouseEnter_ = () => {
    if (this.$.glowAnimationWrapper) {
      this.$.glowAnimationWrapper.classList.remove('play-landing-animation');
      this.$.glowAnimationWrapper.addEventListener(
          'mousemove', this.onMouseMove);
      this.animationFrame = requestAnimationFrame(this.updateRotation);
    }
  };

  protected onMouseLeave_ = () => {
    if (this.$.glowAnimationWrapper) {
      this.$.glowAnimationWrapper.removeEventListener(
          'mousemove', this.onMouseMove);
      cancelAnimationFrame(this.animationFrame);
    }
  };

  protected onClick_(e: MouseEvent) {
    e.preventDefault();
    this.fire('compose-click', {
      button: e.button,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    });
  }

  private updateRotation = () => {
    const now = performance.now();
    const elapsed = now - this.then;

    // Prevents updating more than 60 fps.
    if (elapsed > 16.67) {
      let delta = this.currentAngle - this.aimButtonAngle;
      // If the delta is too small to be noticeable, don't calculate and update.
      if (Math.abs(delta) > 10) {
        // Converts the delta into the shortest angle from -180 to 180 degrees.
        delta = (((delta % 360) + 540) % 360) - 180;
        // Normalizes the delta to a clamped decimal between 0.0 and 1.0.
        const normalizedDelta = this.clampValue(0, 100, Math.abs(delta)) / 300;
        // Multiply delta by the ease and normalized delta for ease-in
        // cushioning.
        this.velocity += delta * this.rotationEase * normalizedDelta;
        // Apply velocity to the current angle.
        this.aimButtonAngle += this.velocity;
        // Apply friction to the velocity so it does not accelerate
        // indefinitely.
        this.velocity *= this.friction;

        this.setRotationCss(this.aimButtonAngle);
      }

      this.then = now - (elapsed % 16.67);
    }

    this.animationFrame = requestAnimationFrame(this.updateRotation);
  };

  private setRotationCss(angle: number) {
    this.$.glowAnimationWrapper.style.setProperty(
        '--mouse-angle', `${angle + 180}deg`);
  }

  private onMouseMove = (e: MouseEvent) => {
    this.setCurrentAngle(e.clientX, e.clientY);
  };

  private setCurrentAngle(mouseX: number, mouseY: number) {
    const elRect = this.$.glowAnimationWrapper.getBoundingClientRect();
    const elX = elRect.x + elRect.width * 0.5;
    const elY = elRect.y + elRect.height * 0.5;
    this.currentAngle = this.getAngleTo(mouseX, mouseY, elX, elY);
  }

  private getAngleTo(x1: number, y1: number, x2: number, y2: number) {
    return Math.atan2(y2 - y1, x2 - x1) * (180 / Math.PI);
  }

  private clampValue(min: number, max: number, val: number) {
    return Math.max(min, Math.min(val, max));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-compose-button': SearchboxComposeButtonElement;
  }
}

customElements.define(
    SearchboxComposeButtonElement.is, SearchboxComposeButtonElement);
