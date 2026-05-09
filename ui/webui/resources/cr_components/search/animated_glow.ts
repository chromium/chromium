// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './audio_wave.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {type PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './animated_glow.css.js';
import {getHtml} from './animated_glow.html.js';
import {GlowAnimationState} from './constants.js';

/*
 * Animation glow for expanding, submitting, voice, dragging.
 * Parent must pass animationState to trigger one of 4 animations, or idle.
 * RequiresVoice, isCollapsible are optional. RequiresVoice is
 * false by default. And if it is true, then it renders the eclipse audio wave
 * element as well. IsCollapsible adds an animation for expand (see .css file).
 * Transcript and receivedSpeech are optional. Allows for audio to simulate
 * audio input without opening audio stream.
 */
if (window.CSS && CSS.registerProperty) {
  try {
    CSS.registerProperty({
      name: '--gradient-angle',
      syntax: '<angle>',
      inherits: true,
      initialValue: '0deg',
    });
  } catch (_e) {
  }
  try {
    CSS.registerProperty({
      name: '--mask-angle',
      syntax: '<angle>',
      inherits: true,
      initialValue: '0deg',
    });
  } catch (_e) {
  }
}

export class SearchAnimatedGlowElement extends CrLitElement {
  static get is() {
    return 'search-animated-glow';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      animationState: {
        type: String,
        reflect: true,
      },
      entrypointName: {
        type: String,
        reflect: true,
      },
      dragDropPlaceholder: {type: String},
      requiresVoice: {type: Boolean},
      isCollapsible: {
        type: Boolean,
        reflect: true,
      },
      transcript: {type: String},
      receivedSpeech: {type: Boolean},
      energyEffectAnimationEnabled: {
        type: Boolean,
        reflect: true,
      },
      isZeroState: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  accessor dragDropPlaceholder: string =
      loadTimeData.getString('composeboxDragAndDropHint');
  accessor entrypointName: string = '';
  accessor requiresVoice: boolean = false;
  accessor isCollapsible: boolean = false;
  accessor transcript: string = '';
  accessor receivedSpeech: boolean = false;
  accessor energyEffectAnimationEnabled: boolean = false;
  accessor isZeroState: boolean = false;

  private targetAngle_: number = 0;
  private maskCurrAngle_: number = 0;
  private gradCurrAngle_: number = 0;
  private isDragging_: boolean = false;
  private rafId_: number|null = null;

  private onDragOver_ = (e: DragEvent) => {
    if (!this.isDragging_) {
      return;
    }
    const newAngle = this.getAngleFromEvent_(e);
    if (this.targetAngle_ === 0 && this.maskCurrAngle_ === 0) {
      this.targetAngle_ = newAngle;
      this.maskCurrAngle_ = newAngle;
      this.gradCurrAngle_ = newAngle;
    } else {
      this.targetAngle_ = newAngle;
    }
  };

  private getAngleFromEvent_(e: DragEvent): number {
    const rect = this.getBoundingClientRect();
    const centerX = rect.left + rect.width / 2;
    const centerY = rect.top + rect.height / 2;
    const scaleX = 4;
    const scaleY = 1.0;
    const dx = (e.clientX - centerX) / scaleX;
    const dy = (e.clientY - centerY) / scaleY;
    return Math.atan2(dy, dx) * (180 / Math.PI);
  }

  private lerpAngle_(start: number, end: number, factor: number): number {
    let delta = end - start;
    while (delta > 180) {
      delta -= 360;
    }
    while (delta < -180) {
      delta += 360;
    }
    return start + delta * factor;
  }

  private renderLoop_ = () => {
    const maskLerpFactor = 0.08;
    const gradLerpFactor = 0.03;
    this.maskCurrAngle_ =
        this.lerpAngle_(this.maskCurrAngle_, this.targetAngle_, maskLerpFactor);
    this.gradCurrAngle_ =
        this.lerpAngle_(this.gradCurrAngle_, this.targetAngle_, gradLerpFactor);

    const maskOffset = -167;
    const gradOffset = -165;

    this.style.setProperty(
        '--mask-angle', `${this.maskCurrAngle_ + maskOffset}deg`);
    this.style.setProperty(
        '--gradient-angle', `${this.gradCurrAngle_ + gradOffset}deg`);

    if (this.isDragging_) {
      this.rafId_ = requestAnimationFrame(this.renderLoop_);
    }
  };

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('animationState')) {
      if (this.energyEffectAnimationEnabled &&
          this.animationState === GlowAnimationState.DRAGGING) {
        this.isDragging_ = true;
        window.addEventListener('dragover', this.onDragOver_);
        this.rafId_ = requestAnimationFrame(this.renderLoop_);
      } else if (
          changedProperties.get('animationState') ===
          GlowAnimationState.DRAGGING) {
        this.isDragging_ = false;
        window.removeEventListener('dragover', this.onDragOver_);
        if (this.rafId_ !== null) {
          cancelAnimationFrame(this.rafId_);
          this.rafId_ = null;
        }
        this.style.removeProperty('--mask-angle');
        this.style.removeProperty('--gradient-angle');
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-animated-glow': SearchAnimatedGlowElement;
  }
}

customElements.define(SearchAnimatedGlowElement.is, SearchAnimatedGlowElement);
