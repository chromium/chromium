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

const globalEnergyWiggle = new Wiggle(/*amplitude=*/ 2,
                                      /*frequency=*/ 0.5);

export interface Bar {
  level: number;
  isSpawning: boolean;
  isUnspawned: boolean;
  initialScale: number;
  spawnTimeMs: number;
  currentScaleX: number;
  currentScaleY: number;
  jitterFactor: number;
  targetHeightPx: number;
}

export interface RecordingWaveElement {
  $: {
    barsContainer: HTMLElement,
  };
}

// ======Physics and visual parameters======
// The time (in milliseconds) between rendering new volume bars.
const BAR_INTERVAL_MS = 120;

// Total number of bars allowed to move across the screen (both on and off
// screen) for the sliding left recording wave effect.
const MAX_BARS = 100;

// The array index at which a background dot "wakes up" and springs into
// an active volume bar. This creates the visual right-side padding delay.
const ACTIVATION_DELAY_INDEX = 6;

// Max width of bar, in px. Minimum width is set in `bar-pill.is-unspawned`
// CSS rule.
const BAR_WIDTH = 12;

// Space between each bar, in px.
export const BAR_GAP = 3;

// Maximum height of each bar, in px. Minimum height is set in
// `bar-pill.is-unspawned` CSS rule.
const MAX_BAR_HEIGHT = 36;

// Spring constants to match Android spring behavior.
// Force of the spring.
const SPRING_STIFFNESS = 200.0;

// Friction to slow down speed of height change of pills.
const SPRING_DAMPING_HEIGHT = 0.3;

// Friction to slow down speed of width change of pills.
const SPRING_DAMPING_WIDTH = 0.7;

// Time it takes for the spring to settle to a stop.
const SPRING_SETTLE_TIME_SECONDS = 0.65;

// Value to simply increase the raw volume level in the animation (output).
const VOLUME_MULTIPLIER = 1;

// Minimum volume level scaling (0-1):
const MINIMUM_VOLUME_LEVEL = 0.1;

// Represents in px the height of each volume bar that starts
// off at. They start as balls, not bars.
const DEFAULT_STARTING_HEIGHT = 8;

// ======Smaller parameters for fine tuning of shadows/color======:
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

// The ratio (0.0 to 1.0) along the wave's width at which the pill shadow
// should be completely faded out (transparent).
const SHADOW_FULLY_FADED_RATIO = 0.85;

// RGB channels for the pill shadow color (light purple).
const SHADOW_COLOR_RGB = '224, 165, 255';

// Linear interpolation.
const lerp = (start: number, end: number, t: number) =>
    start + (end - start) * t;

interface ColorStop {
  ratio: number;
  r: number;
  g: number;
  b: number;
}

// Note: Initial colors are set in `bar-pill.is-unspawned` CSS rule.
// Points to change color at:
const LIGHT_STOPS: ColorStop[] = [
  {ratio: 0.0, r: 201, g: 210, b: 255},  // #C9D2FF
  {ratio: 0.03, r: 49, g: 134, b: 255},  // #3186FF
  {ratio: 0.4, r: 23, g: 116, b: 255},   // #1774FF
  {ratio: 0.7, r: 169, g: 168, b: 255},  // #A9A8FF
  {ratio: 0.9, r: 201, g: 210, b: 255},  // #C9D2FF
  {ratio: 1.0, r: 236, g: 240, b: 255},  // #ECF0FF
];

// Points to change color at in dark mode:
const DARK_STOPS: ColorStop[] = [
  {ratio: 0.0, r: 55, g: 70, b: 109},    // #37466D
  {ratio: 0.03, r: 49, g: 134, b: 255},  // #3186FF
  {ratio: 0.4, r: 23, g: 116, b: 255},   // #1774FF
  {ratio: 0.7, r: 118, g: 117, b: 212},  // #7675D4
  {ratio: 0.9, r: 76, g: 86, b: 143},    // #4C568F
  {ratio: 1.0, r: 55, g: 70, b: 109},    // #37466D
];

// Computes the color at a given ratio along a multi-stop (point) gradient.
// A ratio is essentially the current progress the pill has made when traversing
// right to left. This function finds the two color stops, the current ratio
// (progress made), and linearly interpolates between them to blend the two
// stop's colors.
function getGradientColor(ratio: number, stops: ColorStop[]): string {
  const clampedRatio = Math.max(0, Math.min(1, ratio));

  // Surrounding color stops (initialized to temporary values).
  let left = stops[0]!;
  let right = stops[stops.length - 1]!;

  // Linearly search with two variables for the two adjacent stops
  // that the ratio (progress) belongs in. Avoid binary search since
  // array is effectively O(1) since there is a constant number of stops
  // relative to input.
  for (let i = 0; i < stops.length - 1; i++) {
    const currentStop = stops[i]!;
    const nextStop = stops[i + 1]!;
    if (clampedRatio >= currentStop.ratio && clampedRatio <= nextStop.ratio) {
      left = currentStop;
      right = nextStop;
      break;
    }
  }

  const range = right.ratio - left.ratio;
  // Calculate progress of ratio within the given range.
  const progress = range > 0 ? (clampedRatio - left.ratio) / range : 0;

  // Mix the RGB channels of the left and right stops based on the progress.
  const r = Math.round(left.r + (right.r - left.r) * progress);
  const g = Math.round(left.g + (right.g - left.g) * progress);
  const b = Math.round(left.b + (right.b - left.b) * progress);

  return `rgb(${r}, ${g}, ${b})`;
}

// Solves the closed-form analytical position of an under-damped harmonic
// spring at continuous time t (seconds), assuming initial position 0, target
// position 1, and initial velocity 0.
function solveAnalyticalSpring(
  timeSeconds: number,
  stiffness: number,
  dampingRatio: number,
): number {
  if (timeSeconds <= 0) {
    return 0;
  }
  const undampedFrequency = Math.sqrt(stiffness);  // (angular frequency)

  const dampedFrequency =
      undampedFrequency * Math.sqrt(1.0 - dampingRatio * dampingRatio);

  // Coefficient derived from boundary conditions.
  // It ensures the spring starts at rest (initial velocity = 0 at t = 0).
  // Under underdamped conditions, this scaling factor is:
  const phaseOffsetCoefficient =
      (dampingRatio * undampedFrequency) / dampedFrequency;

  // Rate at which oscillation amplitude decays over time.
  const decayEnvelope =
      Math.exp(-dampingRatio * undampedFrequency * timeSeconds);

  // Sinusoidal oscillation components (harmonic motion).
  const oscillation = Math.cos(dampedFrequency * timeSeconds) +
      phaseOffsetCoefficient * Math.sin(dampedFrequency * timeSeconds);

  // Position starts at 0 and ends at 1.
  return 1.0 - decayEnvelope * oscillation;
}

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
      darkThemeColorsEnabled: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  accessor isListening: boolean = false;
  accessor darkThemeColorsEnabled: boolean = true;

  private barsData_: Bar[] = [];
  private lastDrawTimestamp_: number = performance.now();
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

    if (changedProperties.has('isListening') ||
        changedProperties.has('darkThemeColorsEnabled')) {
      const isDark =
          window.matchMedia('(prefers-color-scheme: dark)').matches &&
          this.darkThemeColorsEnabled;
      this.style.setProperty(
          '--color-recording-wave', isDark ? '#37466d' : '#c9d2ff');
    }

    if (changedProperties.has('isListening')) {
      if (this.isListening) {
        AudioProcessor.startMonitoringLevels();
        this.barsData_ = [];
        // Reset container in case of dirty initial state.
        this.$.barsContainer.replaceChildren();

        this.$.barsContainer.style.setProperty('--bar-width', `${BAR_WIDTH}px`);

        for (let i = 0; i < MAX_BARS; i++) {
          this.barsData_.push({
            level: MINIMUM_VOLUME_LEVEL,
            // Is uninitialized.
            isUnspawned: true,
            // Is not in process of spawning (having just
            // been 'turned on').
            isSpawning: false,
            initialScale: 1,
            spawnTimeMs: 0,
            currentScaleX: 0,
            currentScaleY: 0,
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

    const isDark = window.matchMedia('(prefers-color-scheme: dark)').matches &&
        this.darkThemeColorsEnabled;
    const stops = isDark ? DARK_STOPS : LIGHT_STOPS;

    const now = performance.now();

    // Time elapsed since the last bar was created. Note that this is used solely
    // for deciding when to spawn a new bar and for smooth sliding/scrolling
    // transitions of the entire wave container to the left. It is not used for
    // the spring physics simulation of individual bars, which operates on absolute time
    // and no longer delta time (the time since last frame).
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
        spawnTimeMs: 0,
        currentScaleX: 0,
        currentScaleY: 0,
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
        bar.spawnTimeMs = performance.now();
        bar.currentScaleX = 0;
        bar.currentScaleY = 0;
      }
      // If it has been initialized before and is 'springing' towards its goal
      // volume height and width.
      if (bar.isSpawning) {
        const t = (performance.now() - bar.spawnTimeMs) / 1000;
        const currentScaleY = solveAnalyticalSpring(
            t, SPRING_STIFFNESS, SPRING_DAMPING_HEIGHT);
        const currentScaleX = solveAnalyticalSpring(
            t, SPRING_STIFFNESS, SPRING_DAMPING_WIDTH);

        // Check if the spring has completed its oscillation and settled (reached
        // the mechanical noise floor). Once done, snap the scales to 1.0 and
        // disable further physics processing to conserve CPU cycles.
        const isDone = t > SPRING_SETTLE_TIME_SECONDS;

        bar.currentScaleY = isDone ? 1.0 : currentScaleY;
        bar.currentScaleX = isDone ? 1.0 : currentScaleX;
        bar.isSpawning = !isDone;
      }

      // Determine colors using gradient stops based on progressRatio.
      const progressRatio = index / Math.max(this.barsData_.length - 1, 1);
      const color = getGradientColor(progressRatio, stops);

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

      // Shadow's progress relative to its final ratio (percentage) at which the
      // shadow should be fully transparent.
      const shadowProgress = progressRatio / SHADOW_FULLY_FADED_RATIO;
      // Linear fade-out: full opacity (1.0) on the right, fading to
      // transparent (0.0) at the fade threshold.
      const shadowOpacity = Math.max(0, 1.0 - shadowProgress);
      const shadowColor = `rgba(${SHADOW_COLOR_RGB}, ${shadowOpacity})`;

      // Create 4 shadows:
      // - Top Shadow: creates a "cap" of light at the top.
      // - Side Shadows: These create the "rounded" tube effect by creating 3D
      // rounded corners on left/right of bar.
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
          //   full size.
          const scaleX = lerp(0.5, 1, bar.currentScaleX);
          const scaleY = lerp(bar.initialScale, 1, bar.currentScaleY);
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
