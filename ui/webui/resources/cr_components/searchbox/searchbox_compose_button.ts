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
  private targetAngle = 0;
  private maskCurrAngle = 0;
  private gradientCurrAngle = 0;
  private maskVelocity = 0;
  private springStrength = 0.05;
  private friction = 0.75;
  private isHovering = false;
  private animationFrame = 0;

  // Legacy rotation variables:
  private aimButtonAngle = 0;
  private currentAngle = 0;
  private velocity = 0;
  private rotationEase = 0.01;
  private legacyFriction = 0.9;
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
      energyEffectAnimationEnabled_: {
        type: Boolean,
        reflect: true,
        attribute: 'energy-effect-animation-enabled',
      },
    };
  }

  protected accessor ntpRealboxNextEnabled_: boolean =
      loadTimeData.getBoolean('ntpRealboxNextEnabled');

  protected accessor energyEffectAnimationEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectAnimationEnabled');

  protected accessor composeIcon_: string =
      '//resources/cr_components/searchbox/icons/search_spark.svg';

  protected accessor showAnimation_: boolean = false;

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
      if (this.animationFrame) {
        cancelAnimationFrame(this.animationFrame);
      }
      if (this.energyEffectAnimationEnabled_) {
        this.$.glowAnimationWrapper.classList.add('hovering');
        this.isHovering = true;
        this.animationFrame = requestAnimationFrame(this.renderLoop);
      } else {
        this.animationFrame = requestAnimationFrame(this.updateRotation);
      }
    }
  };

  protected onMouseLeave_ = () => {
    if (this.$.glowAnimationWrapper) {
      this.$.glowAnimationWrapper.removeEventListener(
          'mousemove', this.onMouseMove);
      if (this.energyEffectAnimationEnabled_) {
        this.$.glowAnimationWrapper.classList.remove('hovering');
        this.isHovering = false;
      } else {
        cancelAnimationFrame(this.animationFrame);
      }
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

  private getAngleFromEvent(e: MouseEvent): number {
    const rect = this.$.glowAnimationWrapper.getBoundingClientRect();
    const centerX = rect.left + rect.width / 2;
    const centerY = rect.top + rect.height / 2;
    const scaleX = 4;
    const scaleY = 1.5;
    const dx = (e.clientX - centerX) / scaleX;
    const dy = (e.clientY - centerY) / scaleY;
    return Math.atan2(dy, dx) * (180 / Math.PI);
  }

  private lerpAngle(start: number, end: number, factor: number): number {
    let delta = end - start;
    // Normalize delta to between -180 and 180
    // This makes to take the shortest path around the circle.
    while (delta > 180) {
      delta -= 360;
    }
    while (delta < -180) {
      delta += 360;
    }
    return start + delta * factor;
  }

  private onMouseMove = (e: MouseEvent) => {
    if (this.energyEffectAnimationEnabled_) {
      const newAngle = this.getAngleFromEvent(e);
      if (this.targetAngle === 0 && this.maskCurrAngle === 0) {
        this.targetAngle = newAngle;
        this.maskCurrAngle = newAngle;
        this.gradientCurrAngle = newAngle;
      } else {
        this.targetAngle = newAngle;
      }
    } else {
      this.setCurrentAngle(e.clientX, e.clientY);
    }
  };

  private renderLoop = () => {
    let delta = this.targetAngle - this.maskCurrAngle;
    while (delta > 180) {
      delta -= 360;
    }
    while (delta < -180) {
      delta += 360;
    }

    this.maskVelocity += delta * this.springStrength;
    this.maskVelocity *= this.friction;
    this.maskCurrAngle += this.maskVelocity;

    // 1. Calculate current distance to the mask
    let gradientDelta = this.maskCurrAngle - this.gradientCurrAngle;
    while (gradientDelta > 180) {
      gradientDelta -= 360;
    }
    while (gradientDelta < -180) {
      gradientDelta += 360;
    }

    const distance = Math.abs(gradientDelta);

    // 2. Define the angular window where the easing curve is active.
    // 90 degrees captures the wide sweeps of the mouse.
    const maxDistanceReference = 90;
    const proximity = Math.min(distance / maxDistanceReference, 1);

    // 3. Simulate cubic-bezier(0, 0, 0, 1)
    // Raising this to the 3rd power creates that extreme "sudden brake"
    // effect close to the target, matching the heavy flatline of the bezier.
    const easeOutModifier = Math.pow(proximity, 3);

    // 5. Apply the interpolation
    this.gradientCurrAngle = this.lerpAngle(
        this.gradientCurrAngle,
        this.maskCurrAngle,
        easeOutModifier,
    );

    const maskOffset = -167;
    const gradOffset = 25;

    this.$.glowAnimationWrapper.style.setProperty(
        '--mask-angle', `${this.maskCurrAngle + maskOffset}deg`);

    this.$.glowAnimationWrapper.style.setProperty(
        '--gradient-angle',
        `${this.gradientCurrAngle + maskOffset + gradOffset}deg`);

    // Keep rendering until both the mask stabilizes AND the gradient fully settles
    if (this.isHovering || Math.abs(this.maskVelocity) > 0.01 ||
        Math.abs(gradientDelta) > 0.05) {
      this.animationFrame = requestAnimationFrame(this.renderLoop);
    }
  };

  // Legacy math & rotation methods:
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
        this.velocity *= this.legacyFriction;

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
