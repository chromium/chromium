// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {AudioProcessor} from './audio_processor.service.js';
import {getCss} from './recording_wave.css.js';
import {getHtml} from './recording_wave.html.js';

// For wiggle class: persistence/gain. Each number is about half of its previous
// one. This allows for amplitude to get smaller.
const BASE_AMP_1 = 0.53;
const BASE_AMP_2 = 0.25;
const BASE_AMP_3 = 0.12;

// Harmonic series - as waves get smaller in amplitude, they get faster as well.
const BASE_FREQ_1 = 1;
const BASE_FREQ_2 = 2;
const BASE_FREQ_3 = 3;

// Wiggle class - Uses sine formula to generate three levels of randomness to
// add 'natural' energy to the volume bars.
//   Layer 1: Large movement, slow speed (High amplitude, low frequency).
//   Layer 2: Medium movement, medium speed.
//   Layer 3: Tiny movement, high speed (The "micro-jitter").
class Wiggle {
  private readonly getOrganicJitter = () => 0.5 + Math.random();

  // 1D fractal noise (faster/cheaper than Perlin noise).
  private readonly waveParams: Array<{amplitude: number, frequency: number}> = [
    {
      amplitude: BASE_AMP_1 * this.getOrganicJitter(),
      frequency: BASE_FREQ_1 * this.getOrganicJitter(),
    },
    {
      amplitude: BASE_AMP_2 * this.getOrganicJitter(),
      frequency: BASE_FREQ_2 * this.getOrganicJitter(),
    },
    {
      amplitude: BASE_AMP_3 * this.getOrganicJitter(),
      frequency: BASE_FREQ_3 * this.getOrganicJitter(),
    },
  ];

  private readonly angularFrequency: number;
  private phaseOffset: number = Math.random() * 1000;
  private previousTimeSeconds: number = -Infinity;

  // Jitter amp (depth at which the bars shake at),
  // and wiggle frequency (how quick they shake back and forth).
  constructor(private readonly amplitude: number, frequency: number) {
    // Convert frequency to angular frequency:
    this.angularFrequency = 2 * Math.PI * frequency;
  }

  calculateNext(timeSeconds: number): number {
    if (this.previousTimeSeconds === -Infinity) {
      this.previousTimeSeconds = timeSeconds;
    }
    if (timeSeconds > this.previousTimeSeconds) {
      this.phaseOffset +=
          (timeSeconds - this.previousTimeSeconds) * this.angularFrequency;
      this.previousTimeSeconds = timeSeconds;
    }

    // Fourier Series summation (combines large and small values in wave to get
    // current height).
    let wiggle = 0;
    for (const param of this.waveParams) {
      wiggle += param.amplitude * Math.sin(param.frequency * this.phaseOffset);
    }
    return this.amplitude * wiggle;
  }
}

const globalEnergyWiggle = new Wiggle(/*amplitude=*/ 1.1,
                                      /*frequency=*/ 1.5);

export interface Bar {
  level: number;
  isSpawning: boolean;
  isUnspawned: boolean;
  initialScale: number;
  currentScale: number;
  velocity: number;
  jitterFactor: number;
  targetHeightPx: number;
}

export interface RecordingWaveElement {
  $: {
    barsContainer: HTMLElement,
  };
}

interface Rgb {
  r: number;
  g: number;
  b: number;
}

// ======Physics and visual parameters======
// The time (in milliseconds) between rendering new volume bars.
const BAR_INTERVAL_MS = 140;

// Total number of bars allowed to move across the screen (both on and off
// screen) for the sliding left recording wave effect.
const MAX_BARS = 100;

// The array index at which a background dot "wakes up" and springs into
// an active volume bar. This creates the visual right-side padding delay.
const ACTIVATION_DELAY_INDEX = 6;

// Width of bar, in px.
const BAR_WIDTH = 12;

// Space between each bar, in px.
export const BAR_GAP = 7;

// Maximum height of each bar, in px.
const MAX_BAR_HEIGHT = 36;

// Target decimal percentage of the height that the bar should end up at
// after oscillating.
const TARGET_SCALE = 1.0;

// Controls the stiffness of the spring oscillation movement. Higher means more
// force and eventual velocity.
const STIFFNESS = 0.4;

// Friction/slowing down of animation after oscillation movement.
const DAMPING = 0.62;

// Value to simply increase the raw volume level in the animation (output).
const VOLUME_MULTIPLIER = 2.2;

// Minimum volume level scaling (0-1):
const MINIMUM_VOLUME_LEVEL = 0.1;

// Represents in px the height of each volume bar that starts
// off at. They start as balls, not bars.
const DEFAULT_STARTING_HEIGHT = 8;

// ======Smaller parameters for fine tuning of shadows/color======:
// Color changes from right to mid to left.
const COLORS = {
  START: {r: 43, g: 130, b: 255},  // 0%
  MID: {r: 193, g: 181, b: 254},   // Threshold
  END: {r: 248, g: 241, b: 255},   // 100%
};
// Dissapation energy used as an exponent to determine the amount each pill
// fades as it moves left.
const DISSIPATION_EXPONENT = 2.1;

// Ratio of first color to second color:
const COLOR_RATIO_THRESHOLD = 0.6;

// Ratio of each pill's height used to determine the shadow offset of each pill.
const HEIGHT_TO_OFFSET_RATIO = 0.214;

// Ratio of each pill's height used to determine the shadow blur of each pill.
const HEIGHT_TO_BLUR_RATIO = 0.285;

// Scale that dampens the shadow's jitter movement so it is not fully as
// jittery as the bar. Dampens the blur as well to avoid excessive blur.
const SHADOW_DAMPENING_FACTOR = 0.5;

// Ratio to decrease shadow offset of each pill by.
const SHADOW_SIDE_OFFSET_RATIO = 0.33;

// Ratio to decrease blur each pill's shadow by.
const SHADOW_SIDE_BLUR_RATIO = 0.5;

// Linear interpolation.
const lerp = (start: number, end: number, t: number) =>
    start + (end - start) * t;

// Right to left colors in red green blue:
// First 60% is perwinkle blue. The last 40% should become more lavender.
const lerpColor = (color1: Rgb, color2: Rgb, t: number) => ({
  r: Math.round(lerp(color1.r, color2.r, t)),
  g: Math.round(lerp(color1.g, color2.g, t)),
  b: Math.round(lerp(color1.b, color2.b, t)),
});

export class RecordingWaveElement extends CrLitElement {
  static get is() {
    return 'recording-wave';
  }

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
    };
  }

  accessor isListening: boolean = false;

  private barsData_: Bar[] = [];
  private lastDrawTimestamp_: number = performance.now();
  private lastFrameTime_: number = performance.now();
  private animationFrameId_: number|null = null;

  override disconnectedCallback() {
    super.disconnectedCallback();
    AudioProcessor.stopListening();
    if (this.animationFrameId_ !== null) {
      cancelAnimationFrame(this.animationFrameId_);
      this.animationFrameId_ = null;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('isListening')) {
      if (this.isListening) {
        AudioProcessor.startMonitoringLevels();
        this.barsData_ = [];
        // Reset container in case of dirty initial state.
        this.$.barsContainer.replaceChildren();

        for (let i = 0; i < MAX_BARS; i++) {
          this.barsData_.push({
            level: MINIMUM_VOLUME_LEVEL,
            // Is uninitialized.
            isUnspawned: true,
            // Is not in process of spawning (having just
            // been 'turned on').
            isSpawning: false,
            initialScale: 1,
            currentScale: 0,
            velocity: 0,
            jitterFactor: 0,
            targetHeightPx: DEFAULT_STARTING_HEIGHT,
          });

          const pill = document.createElement('div');
          pill.style.width = `${BAR_WIDTH}px`;
          pill.className = 'bar-pill is-unspawned';

          // Append to the end of the container to match array order
          this.$.barsContainer.append(pill);
        }

        this.lastDrawTimestamp_ = performance.now();
        this.lastFrameTime_ = performance.now();
        if (this.animationFrameId_ === null) {
          this.animationFrameId_ = requestAnimationFrame(this.animationLoop_);
        }
      } else {
        AudioProcessor.stopListening();
        this.$.barsContainer.replaceChildren();
        if (this.animationFrameId_ !== null) {
          cancelAnimationFrame(this.animationFrameId_);
          this.animationFrameId_ = null;
        }
      }
    }
  }

  // Animation: volume bar start off as balls (uninitialized volume),
  // scroll from right to left, and once they are the 6th ball from the right,
  // they become active: they react to volume, and change color and shape (into
  // a bar).

  // This animation is split into 3 main parts:
  // 1. The 6th uninitialized "empty volume" ball's height and width "spring" to
  // a target height that represents the volume at the current time T. The ball
  // springs up to become a bar; it is now "spawned"/"initialized"/"active".
  // Spring physics are used instead of a simple peak->depth->peak bounce.
  // 2. Each bar slides from right to left (RTL) with time T.
  // 3. Each active bar changes from blue to purple WRT time T when sliding RTL.
  private animationLoop_ = () => {
    if (!this.isListening) {
      return;
    }

    const now = performance.now();

    // Normalize total time elapsed since last frame as a
    // progress percentage by 33.33ms (30FPS).
    // This is used for height/width animations, but not sliding of bars.
    const timeDelta = (now - this.lastFrameTime_) / 33.33;
    this.lastFrameTime_ = now;

    // Time elapsed since last frame. This is used as a progress bar between
    // frames to draw "pseudo frames" for sliding of bars.
    let elapsed = now - this.lastDrawTimestamp_;

    // If it is time to create a new bar:
    if (elapsed > BAR_INTERVAL_MS) {
      // Calculate jitter.
      const timeSec = now / 1000;
      const jitterFactor = globalEnergyWiggle.calculateNext(timeSec);

      // Add to front. Is o(n) operation, but since max bar number is
      // constant 100, this operation effectively becomes o(1).
      this.barsData_.unshift({
        level: MINIMUM_VOLUME_LEVEL,
        isUnspawned: true,
        isSpawning: false,
        initialScale: 1,
        currentScale: 0,
        velocity: 0,
        jitterFactor: jitterFactor,
        targetHeightPx: DEFAULT_STARTING_HEIGHT,
      });

      // Create the 'uninitialized'/'inactive'/unspawned pill (looks like a
      // ball).
      const pill = document.createElement('div');
      pill.style.width = `${BAR_WIDTH}px`;

      pill.className = 'bar-pill is-unspawned';
      this.$.barsContainer.prepend(pill);

      // While there are more bars than should be shown:
      while (this.barsData_.length > MAX_BARS) {
        this.barsData_.pop();
        // Remove from DOM:
        if (this.$.barsContainer.lastElementChild) {
          this.$.barsContainer.removeChild(
              this.$.barsContainer.lastElementChild);
        }
      }

      // Each frame should be drawn every X seconds. However, there are delays.
      // Take this excess delay and account for it in the next timeout for the
      // next frame. This is to avoid falling behind when operations take longer
      // than expected.
      const delayedRemainder = elapsed % BAR_INTERVAL_MS;
      this.lastDrawTimestamp_ = now - delayedRemainder;
      elapsed = delayedRemainder;
    }

    // In case there is a delay (meaning part of the next frame has started)
    // and the refresh rate of the screen is faster than the frame rate of the
    // animation (meaning there will be a visual gap between the frames),
    // calculate the progress of each bar between frames, and apply it to its
    // position so that there are "pseudo frames" during the time of delay,
    // making the animation of bars sliding to the left smoother.
    const progress = Math.min(elapsed / BAR_INTERVAL_MS, 1);
    const horizontalOffset = (1 - progress) * (BAR_WIDTH + BAR_GAP);
    this.$.barsContainer.style.transform = `translateX(${horizontalOffset}px)`;

    this.barsData_.forEach((bar, index) => {
      // If the bar has become the `ACTIVATION_DELAY_INDEX`th bar from the right
      // and is ready for activation/initialization:
      if (index === ACTIVATION_DELAY_INDEX && bar.isUnspawned) {
        bar.isUnspawned = false;
        bar.isSpawning = true;

        let liveLevel = AudioProcessor.getVolume() * VOLUME_MULTIPLIER;

        // Hide small noise.
        if (liveLevel < 0.02) {
          liveLevel = 0;
        }

        bar.level = liveLevel;
        // Volume is overridden to be at most 1, but at least 0.1 as a
        // percentage of the max allowed height.
        bar.targetHeightPx =
            Math.max(Math.min(liveLevel, 1), 0.1) * MAX_BAR_HEIGHT;
        bar.initialScale = 4 / bar.targetHeightPx;
      }
      // If it has been initialized before and is 'springing' towards its goal
      // volume height.
      if (bar.isSpawning) {
        // Uses Hooke's law: F = kx (force = spring constant * displacement).
        // Spring constant == `STIFFNESS`: higher means more stiffness means
        // more force/movement.
        const force = STIFFNESS * (TARGET_SCALE - bar.currentScale);
        // Update velocity based on force using y=Ft+b, where velocity_new is
        // `y` and velocity_old is `b`. `F` is force, and `t` is time delta.
        // Force is used instead of acceleration since for simpler simulation
        // purposes, mass and thus inertia are effectively ignored, as mass is
        // assumed to be '1'. Multiply by damping to add friction.
        bar.velocity =
            (bar.velocity + force * timeDelta) * Math.pow(DAMPING, timeDelta);

        // Update progress in its oscillation, which is used to determine
        // current height/width of bar:
        bar.currentScale += bar.velocity * timeDelta;

        // If the bar is close enough to its final size and almost stopped
        // (less velocity), snap it to exactly 1.0 and disable physics
        // processing to save CPU. Velocity must be near 0, as position due to
        // oscillation (repeated cycles back and forth) does not indicate purely
        // by itself if the spring oscillation is finished.
        if (Math.abs(TARGET_SCALE - bar.currentScale) < 0.005 &&
            Math.abs(bar.velocity) < 0.001) {
          bar.currentScale = TARGET_SCALE;
          bar.isSpawning = false;
        }
      }

      // Determine colors (the further left, the more lavender it is).
      // It starts off as perwinkle blue when the bar spawns in on the right.
      const progressRatio = index / Math.max(this.barsData_.length - 1, 1);

      // Colors change from right to mid (set threshold) to left. See if are
      // before/after threshold.
      const isBeforeThreshold = progressRatio <= COLOR_RATIO_THRESHOLD;

      const startColor = isBeforeThreshold ? COLORS.START : COLORS.MID;
      const endColor = isBeforeThreshold ? COLORS.MID : COLORS.END;

      // Calculate normalized ratio based on if it is before or after threshold.
      // Want ratio normalized based on the section that is being focused on
      // (before, or after):
      const normRatio = isBeforeThreshold ?
          progressRatio / COLOR_RATIO_THRESHOLD :
          (progressRatio - COLOR_RATIO_THRESHOLD) / (1 - COLOR_RATIO_THRESHOLD);

      // Final red green blue:
      const {r, g, b} = lerpColor(startColor, endColor, normRatio);

      // Apply a color gradient and 'glow' depth/shadow based on the
      // bar's horizontal position. Bars start of as blue, and older bars (left)
      // become more purple and transparent to simulate energy dissipation.
      const color = bar.isUnspawned ? '#8ab4f8' : `rgb(${r}, ${g}, ${b})`;

      const jitter = bar.jitterFactor || 0;

      // Vertical naturally changing position of overall shadow relative to the
      // bar without dampening. Two levels of jitter, with one having dampening.
      const offsetY = bar.targetHeightPx * HEIGHT_TO_OFFSET_RATIO + jitter;
      // Naturally changing blur ratio with dampening on jitter and final value:
      const blurRadius = bar.targetHeightPx * HEIGHT_TO_BLUR_RATIO +
          jitter * SHADOW_DAMPENING_FACTOR;

      // Specifically for the shadows on the side:
      const sideOffsetY = (offsetY * SHADOW_SIDE_OFFSET_RATIO) +
          jitter * SHADOW_DAMPENING_FACTOR;
      // Specifically to define the edges (shadows on the side):
      const sideBlurRadius = blurRadius * SHADOW_SIDE_BLUR_RATIO;

      // Non linear ease-out formula to decide fade out:
      // max(0.1, (1-progress)^2.1). Floor is 10% (0.1).
      const shadowOpacity =
          Math.max(Math.pow(1 - progressRatio, DISSIPATION_EXPONENT), 0.1);
      const shadowColor = `rgba(237, 202, 255, ${shadowOpacity})`;

      // Create 4 shadows:
      // - Top Shadow: creates a "cap" of light at the top.
      // - Side Shadows: These create the "rounded" tube effect by creating 3D
      // rounded
      //   corners on left/right of bar.
      // - Bottom Shadow: Adds a subtle base glow.
      const boxShadow = bar.isUnspawned ?
          'none' :
          `inset 0px ${offsetY}px ${blurRadius}px -1px ${shadowColor}, inset ${
              sideOffsetY}px 0px ${sideBlurRadius}px -1px ${
              shadowColor}, inset -${sideOffsetY}px 0px ${
              sideBlurRadius}px -1px ${shadowColor}, inset 0px -${
              sideOffsetY}px ${sideBlurRadius}px -1px ${shadowColor}`;

      // Update attributes for next frame.
      const pill = this.$.barsContainer.children[index] as HTMLElement;
      if (pill) {
        if (bar.isUnspawned) {
          // Repeatedly ensure uninitialized elements do not have any effects.
          pill.style.background = '';
          pill.style.boxShadow = '';
          pill.style.transform = '';
          pill.style.height = '';
          pill.classList.add('is-unspawned');
        } else {
          pill.style.background = color;
          pill.style.boxShadow = boxShadow;
          pill.style.height = `${bar.targetHeightPx}px`;
          // Animate the volume bar's dimensions based on spawn progress (0
          // to 1.0)
          // - scaleX: Expands the width from 50% to full size.
          // - scaleY: Stretches the height from the initial 4px scale up to
          // full size.
          const scaleX = lerp(0.5, 1, bar.currentScale);
          const scaleY = lerp(bar.initialScale, 1, bar.currentScale);
          pill.style.transform = `scaleX(${scaleX}) scaleY(${scaleY})`;

          // Redundancy check/action:
          pill.classList.remove('is-unspawned');
        }
      }
    });

    this.animationFrameId_ = requestAnimationFrame(this.animationLoop_);
  };
}

declare global {
  interface HTMLElementTagNameMap {
    'recording-wave': RecordingWaveElement;
  }
}

customElements.define(RecordingWaveElement.is, RecordingWaveElement);
