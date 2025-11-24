// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './audio_wave.css.js';
import {getHtml} from './audio_wave.html.js';

export const blurredRectUrl =
    '//resources/cr_components/search/images/eclipse_wave_blurred_rect.png';

// Controls the curvature tightness (0.0 = straight line, 0.5 = full circle)
const BEZIER_TENSION_RATIO: number = 0.38;

// Wave height
const MAX_AMPLITUDE: number = -25;
const MIN_AMPLITUDE: number = -0;

// Vertical offset
const MAX_VERTICAL_SHIFT: number = -10;
const MIN_VERTICAL_SHIFT: number = -0;

// Idle wave: large margin (less width); peak wave: small margin (more width)
const WAVE_SIDE_MARGIN_IDLE: number = 56;
const WAVE_SIDE_MARGIN_PEAK: number = 10;

const STROKE_WIDTH: number = 3;

// At 60 fps
const MS_PER_FRAME: number = 16.67;

const CIRCLE_RAD: number = Math.PI * 2;

/* Fastest speakers speak 10-12 syllables/sec. Average is 4-5;
 * accounting for latency, make it 7.5 -> 8 frames per syllable.
 * using 10 -> ~0.167 syllable/frame -> 6 frames per syllable.
 */
const MIN_FRAMES_PER_SYLLABLE: number = 6;
const MAX_FRAMES_PER_SYLLABLE: number = 8;

// -12 frames = ~200ms compensation due to speech webkit latency
const FRAME_LATENCY: number = -12;

const SMOOTHING_WINDOW_SIZE: number = 3;

const SMOOTHING_BUFFER_SIZE: number = 5;

interface Bump {
  startTime: number;
  duration: number;
  maxVol: number;
}

function clamp(value: number, minVal: number, maxVal: number): number {
  return Math.min(Math.max(value, minVal), maxVal);
}
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
    value = clamp(value, inputMin, inputMax);
  }

  // Is: (val - input_offset) * ratio + output_offset
  return (value - inputMin) *
      ((outputMax - outputMin) / (inputMax - inputMin)) +
      outputMin;
}

// Heuristic based on number of vowel groups, minus edge cases.
function countSyllablesHeuristic(word: string): number {
  word = word.toLocaleLowerCase();
  if (word.length === 0) {
    return 0;
  }
  // Words of length 3 are usually 1 syllable.
  if (word.length <= 3) {
    return 1;
  }

  // Remove silent 'e', 'es', 'ed' at end, as long as it's not '-ted' or '-ded'
  // Don't take non-t/non-d in '-[x]ed'
  word = word.replace(/(?:[^laeiouy]es|(?<=[^td])ed|[^laeiouy]e)$/, '');

  // Remove leading 'y'; it's never a "vowel" like middle y's are.
  word = word.replace(/^y/, '');

  /* Count vowel groups; either 1 or 2 in a row.
   * 2 vowels in row diphthong. Vowels includes y in
   * middle of words. 3 vowel diphthongs excluded for simplicity.
   */
  const vowelGroups = word.match(/[aeiouy]{1,2}/g);

  return vowelGroups ? vowelGroups.length : 1;
}

function weightedAverage(numArray: number[], amountToAverage: number): number {
  let weightedSum = 0;
  let sumOfWeights = 0;
  for (let i = 0; i < amountToAverage; i++) {
    // Lower indices get higher weight (most recent volume)
    const weight = amountToAverage - i;
    weightedSum += (numArray[i] ?? 0) * weight;
    sumOfWeights += weight;
  }
  return sumOfWeights === 0 ? 0 : weightedSum / sumOfWeights;
}

function bezierEasing(
    controlX1: number, controlX2: number, timeProgress: number): number {
  /*
   * Solve a Cubic Bezier curve for a specific time "t":
   * Used to make the wave sensitivity non-linear (reacts more to soft
   * sounds). Standard Newton-Raphson implementation.
   */
  if (timeProgress <= 0) {
    return 0;
  }
  if (timeProgress >= 1) {
    return 1;
  }
  if (controlX1 === 0 && controlX2 === 1) {
    return timeProgress;
  }

  let currentT = timeProgress;
  // Pre-calculate coefficients
  const coeffA = 3 * controlX1;
  const coeffB = 3 * (controlX2 - controlX1) - coeffA;
  const coeffC = 1 - coeffA - coeffB;

  // Newton-Raphson iteration to find t for a given x (timeProgress)
  for (let i = 0; i < 8; i++) {
    const currentX =
        ((coeffC * currentT + coeffB) * currentT + coeffA) * currentT;
    if (Math.abs(currentX - timeProgress) < 1e-6) {
      break;
    }
    const currentSlope =
        (3 * coeffC * currentT + 2 * coeffB) * currentT + coeffA;
    if (Math.abs(currentSlope) < 1e-6) {
      break;
    }
    currentT -= (currentX - timeProgress) / currentSlope;
  }
  return 3 * currentT * currentT - 2 * currentT * currentT * currentT;
}

export interface AudioWaveElement {
  $: {
    eclipseSvgWrapper: HTMLElement,
    eclipseSvg: SVGElement,
    mask: SVGMaskElement,
    thinPath: SVGPathElement,
    lowerGlowPath: SVGPathElement,
    clipPathShape: SVGPathElement,
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
      transcript: {type: String},
      receivedSpeech: {type: Boolean},
    };
  }

  accessor isListening: boolean = false;
  accessor transcript: string = '';
  accessor receivedSpeech: boolean = false;
  protected accessor isExpanding_: boolean = true;

  private containerWidth_: number = 0;
  private animationFrameId_: number|null = null;

  private decayingAmplitude_: number = 0;
  private frame_: number = 0;
  private lastUpdateTime_: number = performance.now();
  private lastWordCount_: number = 0;

  private volumeHistory_: number[] = [];
  private activeSimulatedBumps_: Bump[] = [];

  /* Tracks if first syllable ever heard (so that way do not have duplicate
   * bump, as there is firstSpeech bump; do not double count that.)
   */
  private firstSyllable_: boolean = true;

  /* Observe width changes per element with a recent size change. */
  private resizeObserver: ResizeObserver = new ResizeObserver((entries) => {
    for (const entry of entries) {
      this.containerWidth_ = entry.contentRect.width;
    }
  });

  override connectedCallback() {
    super.connectedCallback();
    if (this.$.eclipseSvg) {
      this.resizeObserver.observe(this.$.eclipseSvg);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('isListening')) {
      this.isListening ? this.onStartListen() : this.onStopListen();
      this.receivedSpeech = false;
    }
    if (changedProperties.has('transcript')) {
      this.handleNewWords();
    }
    if (changedProperties.has('receivedSpeech')) {
      /* Speech recognition's speech state is changed before it outputs a
       * transcript. If received speech (but not results) for first time,
       * add a bump so do not have to wait for transcript to come in
       * from speech webkit. This avoids a delay. If the transcript is
       * still blank, even after it is updated, set firstSyllable_ to false
       * so it is not ignored to avoid double counting syllables here and
       * in the function where syllables are registered and added as audio
       * simulation bumps in the wave.
       */

      if (this.receivedSpeech) {
        this.decayingAmplitude_ = 0.4;
        for (let i = 0; i < this.volumeHistory_.length; i++) {
          this.volumeHistory_[i] = Math.max(this.volumeHistory_[i] ?? 0, 0.3);
        }
        this.makeSimulatedAudioBump(15, 25, this.frame_, 0.14, 0.05);
        if (this.transcript === '') {
          // Do use measure to avoid double counting
          this.firstSyllable_ = false;
        }
      }
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.onStopListen();
    this.resizeObserver.disconnect();
  }

  protected onStartListen() {
    this.isExpanding_ = true;

    this.volumeHistory_ = new Array(SMOOTHING_BUFFER_SIZE).fill(0.001);

    // If animation has not started; start it.
    if (this.animationFrameId_ === null) {
      // Add to queue instead of adding to call stack; (now CPU efficient).
      this.animationFrameId_ = requestAnimationFrame(this.processFrame);
    }
  }

  protected onStopListen() {
    this.frame_ = 0;
    this.decayingAmplitude_ = 0;

    if (this.animationFrameId_ !== null) {
      cancelAnimationFrame(this.animationFrameId_);
      this.animationFrameId_ = null;
    }

    this.isExpanding_ = false;
  }

  /* Arrow function where "this" is AudioWaveElement when handed
   * to requestAnimationFrame(). Processes scheduling
   * and smoothing for animation frame_. */
  private processFrame = () => {
    if (!this.isListening) {
      this.animationFrameId_ = null;
      return;
    }

    const now = performance.now();
    const elapsed = now - this.lastUpdateTime_;

    // Throttle to ensure 60 fps
    if (elapsed > MS_PER_FRAME) {
      this.updateVolume();

      let level = this.volumeHistory_[0];
      // Smoothing to prevent jitter
      if (SMOOTHING_WINDOW_SIZE > 0) {
        level = weightedAverage(this.volumeHistory_, SMOOTHING_WINDOW_SIZE);
      }

      /* Define soft "ease-in", normal ease-out to reduce smaller
       * noises and slightly emphasize louder sounds.
       */
      level = bezierEasing(0.4, 0.6, level ?? 0);
      this.drawEclipseWavePath(level);

      this.lastUpdateTime_ = now - (elapsed % MS_PER_FRAME);
    }

    if (this.isListening) {
      this.animationFrameId_ = requestAnimationFrame(this.processFrame);
    }
  };

  protected drawEclipseWavePath(rawInputLevel: number) {
    this.frame_++;

    // Snap up immediately if new volume is louder.
    // If quieter, hold the previous peak (it will decay slowly in the next
    // step).
    this.decayingAmplitude_ = Math.max(this.decayingAmplitude_, rawInputLevel);
    this.decayingAmplitude_ *= 0.85;  // Decay

    // Wave width calculation (louder = wider)
    const currentSidePadding = mapToRange(
        Math.pow(this.decayingAmplitude_, 2.5),
        0,
        1,
        WAVE_SIDE_MARGIN_IDLE,
        WAVE_SIDE_MARGIN_PEAK,
    );

    // Drawing anchors sitting on left/right ends of wave
    const anchorLeftX = currentSidePadding;
    const anchorRightX = this.containerWidth_ - currentSidePadding;

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

      const audioDisplacement = mapToRange(
          this.decayingAmplitude_, 0, 1, MIN_AMPLITUDE, MAX_AMPLITUDE);

      const baseOffset = mapToRange(
          this.decayingAmplitude_, 0, 1, MIN_VERTICAL_SHIFT,
          MAX_VERTICAL_SHIFT);

      // Formula: Displacement * (1 - x^2) + Offset
      return audioDisplacement * (1 - Math.pow(normalizedX, 2)) + baseOffset;
    };

    // Bezier Control (left and right points) positioning
    const controlPointXLeft = this.containerWidth_ * BEZIER_TENSION_RATIO;
    const controlPointXRight =
        this.containerWidth_ * (1 - BEZIER_TENSION_RATIO);

    // Y-offset for control points (determines "pull")
    const controlPointY = getParabolicDepth(controlPointXLeft);

    // Allow it to float up too, not just stretch up
    const maskTranslateY = mapToRange(
        this.decayingAmplitude_,
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
    this.$.thinPath.setAttribute('d', buildBezierPath(STROKE_WIDTH, true));
    // Glow:
    this.$.lowerGlowPath.setAttribute(
        'd', buildBezierPath(STROKE_WIDTH, false));
    const currentTransform = `translate(0, ${maskTranslateY})`;
    this.$.mask.setAttribute('transform', currentTransform);
    this.$.thinPath.setAttribute('transform', currentTransform);
    this.$.lowerGlowPath.setAttribute('transform', currentTransform);
    this.$.clipPathShape.setAttribute('transform', currentTransform);

    // Should be >= wrapper height.
    const bottomClipY = 1000;
    const topControlY = STROKE_WIDTH * -0.5 + controlPointY;

    // Clip the glow so it does not show above the wave (emanates downwards
    // only).
    const clipPathString = `M ${0},${- maskTranslateY * 0.25}
    L ${anchorLeftX},${0}
    C ${controlPointXLeft},${topControlY} ${controlPointXRight},${
        topControlY} ${anchorRightX},${0}
    L ${this.containerWidth_},${- maskTranslateY * 0.25}
    L ${this.containerWidth_},${bottomClipY}
    L ${0},${bottomClipY}
    Z`;

    this.$.clipPathShape.setAttribute('d', clipPathString);
  }

  protected updateVolume() {
    /* 0 to 1 represents how much decimal % of volume can be
     * added in current frame due to mapping in mapToRange.
     */

    // Start at 0, then ramp up physics during first 40 frames:
    const startRamp = Math.min(1, this.frame_ / 40);

    let ambientSimulatedMotion =  // Fast wave:
        0.01 + (1 + Math.cos(((this.frame_) / 60) * CIRCLE_RAD)) * 0.05;
    ambientSimulatedMotion *=  // Slow wave:
        0.25 +
        (1 + Math.cos(((this.frame_ + 100) / 400) * CIRCLE_RAD)) * 0.2 *
            startRamp;
    // Random noise floor like live mic (random increase):
    ambientSimulatedMotion += 0.01 * Math.random();
    ambientSimulatedMotion *= startRamp;

    // Combine historical value + audio bump simulation:
    this.volumeHistory_.unshift(
        ambientSimulatedMotion + this.getSimulatedAudioBumpsSum());

    // Trim volume history if too long:
    if (this.volumeHistory_.length > SMOOTHING_BUFFER_SIZE) {
      this.volumeHistory_.length = SMOOTHING_BUFFER_SIZE;
    }
  }

  protected handleNewWords() {
    const trimmedTranscript = this.transcript.trim();
    if (trimmedTranscript === '') {
      // In case if input gets cleared, reset animation.
      this.lastWordCount_ = 0;
      return;
    }
    const words = trimmedTranscript.split(/\s+/);
    const currentWordCount = words.length;

    // Ignore shorter and same word counts (due to interim result changes).
    if (currentWordCount <= this.lastWordCount_) {
      this.lastWordCount_ = currentWordCount;
      return;
    }

    // Find completely new words not seen before.
    const newWordCount = currentWordCount - this.lastWordCount_;
    const newWords = words.slice(-newWordCount);  // Get last nth new words.
    this.lastWordCount_ = currentWordCount;

    this.triggerSyllableBumps(newWords);
  }

  protected triggerSyllableBumps(words: string[]) {
    /* Keeps every start time after this time (maintains order of time
     * in activeSimulatedBumps_). Start in "future" to account for latency in
     * speech recognition webkit.
     */
    let frameOffset = FRAME_LATENCY;

    words.forEach(word => {
      const syllableCount = countSyllablesHeuristic(word);
      for (let i = 0; i < syllableCount; i++) {
        /* If not first syllable; else, if first syllable already
         * counted by change in speechReceived state, ignore
         * to avoid double counting
         */
        if (!this.firstSyllable_) {
          this.makeSimulatedAudioBump(
              25, 15, this.frame_ + frameOffset, 0.12, 0.08);
          /* At least min frames, up to slower end of
           * average frames per syllable:
           */
          frameOffset += MIN_FRAMES_PER_SYLLABLE +
              ((MAX_FRAMES_PER_SYLLABLE - MIN_FRAMES_PER_SYLLABLE) *
               Math.random());
        } else {
          this.firstSyllable_ = false;
        }
      }

      // Breath gap:
      frameOffset += 2;
    });
  }

  protected makeSimulatedAudioBump(
      durationMultiplier: number, durationOffset: number, startTime: number,
      maxVolMultiplier: number, maxVolOffset: number) {
    this.activeSimulatedBumps_.push({
      duration: Math.random() * durationMultiplier +
          durationOffset,    // in frames @60fps
      startTime: startTime,  // can be in future
      maxVol: maxVolOffset + Math.random() * maxVolMultiplier,
    });
  }

  protected getSimulatedAudioBumpsSum(): number {
    let simulatedVolumeSum = 0;
    /* Let activeSimulatedBumps_ be a queue ordered by start time.
     * Because of varying duration, it is not strictly ordered by
     * end time; only start time. Cannot assume deletion will only
     * happen at start of queue due to varying end times. Only
     * write i to index for next func call if duration of sound not
     * over. Otherwise, let compaction truncate it to remove it
     * from the list.
     */
    let writeIndex = 0;
    for (let i = 0; i < this.activeSimulatedBumps_.length; i++) {
      const bump = this.activeSimulatedBumps_[i]!;
      const bumpRelativeTime = this.frame_ - bump.startTime;
      const progress = bumpRelativeTime / bump.duration;
      if (progress >= 1) {
        continue;
      }
      /* Multiply by max volume and cos allows for mountain, not
       * slide down (just pure recursive decay). Vertical shift so
       * bottom is 0, then shrink so top is 1.
       * 1 - () to invert and start at 0.
       * Only start if it is not in future.
       */
      if (this.frame_ >= bump.startTime) {
        let newBumpAddition =
            (1 - (1 + Math.cos(progress * CIRCLE_RAD)) * 0.5) * bump.maxVol;
        newBumpAddition = clamp(newBumpAddition, 0, 1);
        simulatedVolumeSum += newBumpAddition;
      }
      this.activeSimulatedBumps_[writeIndex] = bump;
      writeIndex++;
    }
    this.activeSimulatedBumps_.length = writeIndex;  // Truncate dead data.
    return simulatedVolumeSum;
  }

  /* Note: audio stream from hardware does not work. Using simulated.
   * Context:
   * Problem: backend invariant assertion (currentTime >= packetTime)
   * triggered sometimes, causing fatal crash.

   * 2 audio streams cause race condition due to hardware-browser
   * clock drift from poor latency heuristic for timestamp calculation.
   * Speech recognition started the master clock and audio data pipeline/graph.
   * This component tries joining that audio stream. Will run into assertion
   * error since Chrome audio controller (AudioInputDevice) receives packets
   * with timestamps from the future.

   * This is likely due to audio driver's hardware clock becoming
   * non-monotonic with 2 audio streams. This means there may be low-level
   * jitter due to handling 2 audio streams, and it does not add the proper
   * processing "offset" to the "captured" time, potentially adding too
   * much, making the audio timestamp "in the future". According to error
   * logs, "input buffer socket" is full, meaning processing mic input
   * has stalled and the driver uses that and the output buffer to predict
   * when processing might be finished, and it can do this incorrectly.

   * Additionally, this rendering thread is likely less priority
   * than speech recognition webkit and is starved of CPU, causing
   * its browser time to lag, making any "current audio timestamps"
   * seem like they are ahead and therefore in the future, failing the
   * assertion. Need better way to fulfill 2 audio streams without
   * introducing hardware or browser lag.

   * Attempted solutions (with task queue and microtask queues):
   * 1. CLOCK SYNCHRONIZATION (Failed):
   * - Strategy: Set AudioContext clock ahead of the hardware clock using a
   * "Head Start" (Dummy Oscillator + large setTimeout).
   * - Result: Failed. The system's thread scheduler was too unpredictable
   * for fixed delay to be reliable.
   * 2. BUFFER TOLERANCE (Failed):
   * - Strategy: Use latencyHint: 'playback' (larger buffer) and
   * async/await on context.close() (guaranteed cleanup).
   * - Result: Failed. Buffer could not absorb all miscalculations in timestamp.
   * 3. PIPELINE ISOLATION (Failed):
   * - Strategy: Set echoCancellation: false to force a separate
   * "Raw Audio" path.
   * - Result: Failed. The OS driver likely optimized and merged
   * the paths somewhere.
   */
}

declare global {
  interface HTMLElementTagNameMap {
    'audio-wave': AudioWaveElement;
  }
}

customElements.define('audio-wave', AudioWaveElement);
