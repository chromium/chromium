// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './audio_wave.css.js';
import {getHtml} from './audio_wave.html.js';

export const blurredRectUrl =
    '//resources/images/eclipse_wave_blurred_rect.png';

// Controls the curvature tightness (0.0 = straight line, 0.5 = full circle)
const BEZIER_TENSION_RATIO: number = 0.35;

// Wave height
const MAX_AMPLITUDE: number = -25;
const MIN_AMPLITUDE: number = -0;

// Vertical offset
const MAX_VERTICAL_SHIFT: number = -10;
const MIN_VERTICAL_SHIFT: number = -0;

// Idle wave: large margin (less width); peak wave: small margin (more width)
const WAVE_SIDE_MARGIN_IDLE: number = 56;
const WAVE_SIDE_MARGIN_PEAK: number = 0;

const STROKE_WIDTH: number = 3;

// At 60 fps
const MS_PER_FRAME = 16.67;

const CIRCLE_RAD = Math.PI * 2;

/*
 * Linear Interpolation that maps one unit to another unit, like volume to px
 */
function mapToRange(
    value: number,
    inputMin: number,
    inputMax: number,
    outputMin: number,
    outputMax: number,
    shouldClamp = false,
    ): number {
  if (shouldClamp) {
    inputMax = Math.min(value, inputMax);
    inputMin = Math.max(value, inputMin);
  }

  // Is: (val - input_offset) * ratio + output_offset
  return (value - inputMin) *
      ((outputMax - outputMin) / (inputMax - inputMin)) +
      outputMin;
}
export interface AudioWaveElement {
  $: {
    'eclipse-svg-wrapper': HTMLElement,
    'eclipse-svg': SVGElement,
    'mask': SVGMaskElement,
    'thin-path': SVGPathElement,
    'lower-glow-path': SVGPathElement,
    'clip-path-shape': SVGPathElement,
  };
}

/**
 * Voice input visualizer.
 */
export class AudioWaveElement extends CrLitElement {
  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isListening: {
        reflect: true,
        type: Boolean,
      },
      isExpanding_: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  accessor isListening: boolean = false;
  protected accessor isExpanding_: boolean = true;

  private eclipseSvgWrapperEl?: HTMLElement;
  private maskEl?: SVGMaskElement;
  private thinPathEl?: SVGPathElement;
  private lowerGlowPathEl?: SVGPathElement;
  private clipPathEl?: SVGPathElement;
  private eclipseSvgEl?: SVGElement;

  private containerWidth: number = 0;
  private animationFrameId: number|null = null;

  private decayingAmplitude: number = 0;
  private frame: number = 0;
  private lastUpdateTime: number = performance.now();

  /* Observe width changes per element with a recent size change. */
  private resizeObserver: ResizeObserver = new ResizeObserver((entries) => {
    for (const entry of entries) {
      this.containerWidth = entry.contentRect.width;
    }
  });

  override connectedCallback() {
    super.connectedCallback();
    if (this.eclipseSvgEl) {
      this.resizeObserver.observe(this.eclipseSvgEl);
    }
  }
  override firstUpdated() {
    this.eclipseSvgWrapperEl = this.$['eclipse-svg-wrapper'];
    this.maskEl = this.$['mask'];
    this.thinPathEl = this.$['thin-path'];
    this.lowerGlowPathEl = this.$['lower-glow-path'];
    this.clipPathEl = this.$['clip-path-shape'];
    this.eclipseSvgEl = this.$['eclipse-svg'];
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('isListening')) {
      this.isListening ? this.onStartListen() : this.onStopListen();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.onStopListen();
    this.resizeObserver.disconnect();
  }

  protected onStartListen() {
    if (!this.eclipseSvgWrapperEl) {
      return;
    }

    this.isExpanding_ = true;

    // If animation has not started; start it.
    if (this.animationFrameId === null) {
      // Add to queue instead of adding to call stack; (now CPU efficient).
      this.animationFrameId = requestAnimationFrame(this.processFrame);
    }
  }

  protected onStopListen() {
    this.frame = 0;
    this.decayingAmplitude = 0;

    if (this.animationFrameId !== null) {
      cancelAnimationFrame(this.animationFrameId);
      this.animationFrameId = null;
    }

    this.isExpanding_ = false;
  }

  /* Arrow function where "this" is AudioWaveElement when handed
   * to requestAnimationFrame(). Processes scheduling
   * and smoothing for animation frame. */
  private processFrame = () => {
    if (!this.isListening) {
      this.animationFrameId = null;
      return;
    }

    const now = performance.now();
    const elapsed = now - this.lastUpdateTime;

    // Throttle to ensure 60 fps
    if (elapsed > MS_PER_FRAME) {
      // Offset + cos((time + translation) / (frames in a breath) * full circle)
      // * ratio Jitter (fast cycles)
      let ambientSimulatedMotion =
          0.01 + (1 + Math.cos(((this.frame) / 12) * CIRCLE_RAD)) * 0.05;
      // Random noise floor like live mic (random number from offset to 1.0)
      ambientSimulatedMotion *= (0.95 + Math.random() * 0.05);
      // Multiply it for swell
      ambientSimulatedMotion *=
          0.6 + (1 + Math.cos(((this.frame + 100) / 160) * CIRCLE_RAD)) * 0.2;
      // Start at 0, then ramp up physics.
      const startRamp = Math.min(1, this.frame / 160);
      ambientSimulatedMotion *= startRamp;
      this.drawEclipseWavePath(ambientSimulatedMotion, startRamp);

      this.lastUpdateTime = now - (elapsed % MS_PER_FRAME);
    }

    if (this.isListening) {
      this.animationFrameId = requestAnimationFrame(this.processFrame);
    }
  };

  protected drawEclipseWavePath(rawInputLevel: number, startRamp: number) {
    if (!this.thinPathEl || !this.lowerGlowPathEl || !this.maskEl ||
        !this.clipPathEl) {
      return;
    }

    this.frame++;

    // Snap up immediately if new volume is louder.
    // If quieter, hold the previous peak (it will decay slowly in the next
    // step).
    this.decayingAmplitude = Math.max(this.decayingAmplitude, rawInputLevel);

    // Idle state: slight "murmur"
    // Offset + cos(time / (frames in a breath) * full circle) * ratio
    const idleBreathingOffset =
        ((1 + Math.cos((((this.frame + 50) % 120) / 120) * CIRCLE_RAD)) / 2) *
        0.4 * startRamp;

    // Initial decay amplitude of wave
    this.decayingAmplitude -=
        ((this.decayingAmplitude - idleBreathingOffset) / 1) * 0.2;

    const effectiveAmplitude =
        Math.max(this.decayingAmplitude, idleBreathingOffset);

    // Wave width calculation (louder = wider)
    const currentSidePadding = mapToRange(
        Math.pow(effectiveAmplitude, 2.5),
        0,
        1,
        WAVE_SIDE_MARGIN_IDLE,
        WAVE_SIDE_MARGIN_PEAK,
    );

    // Drawing anchors sitting on left/right ends of wave
    const anchorLeftX = currentSidePadding;
    const anchorRightX = this.containerWidth - currentSidePadding;

    // Center position and width of hypothetical parabola
    const waveCenterX = (anchorLeftX + anchorRightX) / 2;
    const waveHalfWidth = (anchorRightX - anchorLeftX) / 2;

    // Calculates how high control points need to be in order to create perfect
    // parabolic arch shape
    const getParabolicDepth = (xPosition: number): number => {
      if (waveHalfWidth === 0) {
        return 0;
      }

      const normalizedX = (xPosition - waveCenterX) / waveHalfWidth;

      const audioDisplacement =
          mapToRange(effectiveAmplitude, 0, 1, MIN_AMPLITUDE, MAX_AMPLITUDE);

      const baseOffset = mapToRange(
          effectiveAmplitude, 0, 1, MIN_VERTICAL_SHIFT, MAX_VERTICAL_SHIFT);

      // Formula: Displacement * (1 - x^2) + Offset
      return audioDisplacement * (1 - Math.pow(normalizedX, 2)) + baseOffset;
    };

    // Bezier Control (left and right points) positioning
    const controlPointXLeft = this.containerWidth * BEZIER_TENSION_RATIO;
    const controlPointXRight = this.containerWidth * (1 - BEZIER_TENSION_RATIO);

    // Y-offset for control points (determines "pull")
    const controlPointY = getParabolicDepth(controlPointXLeft);

    // Allow it to float up too, not just stretch up
    const maskTranslateY = mapToRange(
        effectiveAmplitude,
        0,
        1,
        MIN_VERTICAL_SHIFT,
        MAX_VERTICAL_SHIFT,
    );

    const buildBezierPath =
        (thickness: number, isSolidLine: boolean): string => {
          // If solid line, the bottom curve mirrors the top.
          // Else, is glow, so inverts
          const topY = thickness * -0.5 + controlPointY;
          const bottomY =
              thickness * 0.5 + (isSolidLine ? controlPointY : -controlPointY);

          return `M ${anchorLeftX},${0}
                  C ${controlPointXLeft},${topY} ${controlPointXRight},${
              topY} ${anchorRightX},${0}
                  C ${controlPointXRight},${bottomY} ${controlPointXLeft},${
              bottomY} ${anchorLeftX},${0}
                  Z`;
        };
    // Line:
    this.thinPathEl.setAttribute('d', buildBezierPath(STROKE_WIDTH, true));
    // Glow:
    this.lowerGlowPathEl.setAttribute(
        'd', buildBezierPath(STROKE_WIDTH, false));
    const currentTransform = `translate(0, ${maskTranslateY})`;
    this.maskEl.setAttribute('transform', currentTransform);
    this.thinPathEl.setAttribute('transform', currentTransform);
    this.lowerGlowPathEl.setAttribute('transform', currentTransform);
    this.clipPathEl.setAttribute('transform', currentTransform);

    // Should be >= wrapper height.
    const bottomClipY = 1000;
    const topControlY = STROKE_WIDTH * -0.5 + controlPointY;

    // Clip the glow so it does not show above the wave (emanates downwards
    // only).
    const clipPathString = `M ${0},${- maskTranslateY * 0.25}
    L ${anchorLeftX},${0}
    C ${controlPointXLeft},${topControlY} ${controlPointXRight},${
        topControlY} ${anchorRightX},${0}
    L ${this.containerWidth},${- maskTranslateY * 0.25}
    L ${this.containerWidth},${bottomClipY}
    L ${0},${bottomClipY}
    Z`;

    this.clipPathEl.setAttribute('d', clipPathString);
  }

  // Problem: audio stream from hardware does not work. Use simulated.
  protected shouldUseSimulatedAudio(): boolean {
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'audio-wave': AudioWaveElement;
  }
}

customElements.define('audio-wave', AudioWaveElement);
