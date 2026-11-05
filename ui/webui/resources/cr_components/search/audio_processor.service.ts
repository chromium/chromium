// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AudioLevelsProcessor} from './audio_levels.js';
import type {Bump} from './audio_simulation_utils.js';
import {AMPLITUDE_DECAY_RATE, applySensitivityEasing, clamp, getAmbientSimulatedMotion, makeSimulatedAudioBump, MS_PER_FRAME, SPEECH_RECEIVED_BUMP_DURATION_MULT, SPEECH_RECEIVED_BUMP_DURATION_OFFSET, SPEECH_RECEIVED_BUMP_MAX_VOL_MULT, SPEECH_RECEIVED_BUMP_MAX_VOL_OFFSET, triggerSyllableBumps, updateBumpsAndGetSum} from './audio_simulation_utils.js';

const SPEECH_RECEIVED_VOL_SPIKE: number = 0.4;

export class AudioProcessorService {
  private audioContext: AudioContext|null = null;
  private analyser: AnalyserNode|null = null;
  private levels: Uint8Array<ArrayBuffer>|Float32Array<ArrayBuffer>|null = null;
  private audioLevelsProcessor = new AudioLevelsProcessor();
  private mediaStream: MediaStream|null = null;
  private microphoneSource: MediaStreamAudioSourceNode|null = null;

  private simulate: boolean = false;
  private isListening: boolean = false;
  private startTimeMs: number = 0;
  private lastVolumeTimeMs: number = 0;
  private decayingAmplitude: number = 0;
  private activeSimulatedBumps: Bump[] = [];
  private receivedSpeech: boolean = false;
  private lastWordCount: number = 0;
  private firstSyllable: boolean = true;

  setSimulate(simulate: boolean): void {
    this.simulate = simulate;
  }

  // Returning a boolean lets the UI know if it actually worked
  async startMonitoringLevels(): Promise<boolean> {
    if (this.simulate) {
      this.isListening = true;
      this.startTimeMs = performance.now();
      this.lastVolumeTimeMs = this.startTimeMs;
      this.decayingAmplitude = 0;
      this.activeSimulatedBumps = [];
      this.firstSyllable = true;
      this.lastWordCount = 0;
      this.receivedSpeech = false;
      return true;
    }

    // Do not create another audio stream if one already exists.
    if (this.audioContext) {
      return false;
    }

    try {
      this.mediaStream =
          await navigator.mediaDevices.getUserMedia({audio: true});

      this.audioContext = new AudioContext({sampleRate: 16000});

      if (this.audioContext.state === 'suspended') {
        await this.audioContext.resume();
      }


      // Used to derive numbers like volume and frequency from the audio stream.
      this.analyser = this.audioContext.createAnalyser();

      this.microphoneSource =
          this.audioContext.createMediaStreamSource(this.mediaStream);

      // Plug the mic straight into the analyser. Done.
      this.microphoneSource.connect(this.analyser);

      // Fast Fourier Transform - higher means more detail, but slower
      // calculations.
      this.analyser.fftSize = 1024;

      // Set the default smoothing to 0 to utilize the Chrome custom smoother.
      this.analyser.smoothingTimeConstant = 0;

      // frequencyBinCount is half of fftSize, so 512 in this case.
      // Assuming a 16000Hz sample rate, the maximum measurable frequency
      // is 8000Hz. Each bucket represents 15.625 Hz (8000Hz / 512).
      // `AudioLevelsProcessor` limits speech band analysis to bins [7, 70]:
      //   - Bucket [0, 7): low frequencies (0 to ~109Hz; rumbles, hums,
      //   clipped).
      //   - Bucket [7, 70]: speech band fundamental frequencies (~109Hz to
      //   ~1094Hz, used).
      //   - Bucket (70, 512): high frequencies (>1094Hz; noise, clicks,
      //   sibilance, clipped).
      const bucketSize = this.analyser.frequencyBinCount;
      this.levels = new Float32Array(bucketSize);
      return true;

    } catch (err) {
      if (this.mediaStream) {
        this.mediaStream.getTracks().forEach(track => track.stop());
        this.mediaStream = null;
      }
      return false;
    }
  }

  getVolume(): number {
    if (this.simulate) {
      if (!this.isListening) {
        return 0;
      }
      const now = performance.now();
      const currentVirtualFrame = this.getCurrentVirtualFrame();

      const ambientSimulatedMotion =
          getAmbientSimulatedMotion(currentVirtualFrame);

      const sum =
          updateBumpsAndGetSum(this.activeSimulatedBumps, currentVirtualFrame);

      const rawInputLevel = ambientSimulatedMotion + sum;

      const elapsedMs = now - this.lastVolumeTimeMs;
      const elapsedFrames = elapsedMs / MS_PER_FRAME;
      this.lastVolumeTimeMs = now;

      if (elapsedFrames > 0) {
        this.decayingAmplitude *= Math.pow(AMPLITUDE_DECAY_RATE, elapsedFrames);
      }
      // Attack: snap up immediately if new volume is louder.
      this.decayingAmplitude = Math.max(this.decayingAmplitude, rawInputLevel);

      const level = applySensitivityEasing(this.decayingAmplitude);
      return clamp(level, 0, 1);
    }

    if (!this.analyser || !this.levels) {
      return 0;
    }

    const floatBuffer = this.levels as Float32Array<ArrayBuffer>;
    this.analyser.getFloatFrequencyData(floatBuffer);
    return this.audioLevelsProcessor.process(floatBuffer, performance.now());
  }

  updateTranscript(transcript: string): void {
    if (!this.simulate || !this.isListening) {
      return;
    }
    const trimmedTranscript = transcript.trim();
    if (trimmedTranscript === '') {
      this.lastWordCount = 0;
      return;
    }
    const words = trimmedTranscript.split(/\s+/);
    const currentWordCount = words.length;

    if (currentWordCount <= this.lastWordCount) {
      this.lastWordCount = currentWordCount;
      return;
    }

    const newWordCount = currentWordCount - this.lastWordCount;
    const newWords = words.slice(-newWordCount);
    this.lastWordCount = currentWordCount;

    this.triggerSyllableBumps(newWords);
  }

  updateReceivedSpeech(receivedSpeech: boolean): void {
    if (!this.simulate || !this.isListening) {
      return;
    }
    if (receivedSpeech && !this.receivedSpeech) {
      this.receivedSpeech = true;
      const currentVirtualFrame = this.getCurrentVirtualFrame();
      makeSimulatedAudioBump(
          this.activeSimulatedBumps, SPEECH_RECEIVED_BUMP_DURATION_MULT,
          SPEECH_RECEIVED_BUMP_DURATION_OFFSET, currentVirtualFrame,
          SPEECH_RECEIVED_BUMP_MAX_VOL_MULT,
          SPEECH_RECEIVED_BUMP_MAX_VOL_OFFSET);
      this.decayingAmplitude = SPEECH_RECEIVED_VOL_SPIKE;
    } else if (!receivedSpeech) {
      this.receivedSpeech = false;
    }
  }

  // Delegated to `audio_simulation_utils.ts` for shared syllable bump logic.
  private triggerSyllableBumps(words: string[]): void {
    const currentVirtualFrame = this.getCurrentVirtualFrame();
    const {firstSyllable} = triggerSyllableBumps(
        this.activeSimulatedBumps, words, currentVirtualFrame,
        this.firstSyllable);
    this.firstSyllable = firstSyllable;
  }

  private getCurrentVirtualFrame(): number {
    return (performance.now() - this.startTimeMs) / MS_PER_FRAME;
  }

  // Cleanup.
  async stopListening(): Promise<void> {
    this.isListening = false;
    if (this.simulate) {
      this.activeSimulatedBumps = [];
      this.decayingAmplitude = 0;
      this.lastWordCount = 0;
      this.firstSyllable = true;
      return;
    }
    if (this.mediaStream) {
      this.mediaStream.getTracks().forEach((track) => track.stop());
      this.mediaStream = null;
    }
    if (this.audioContext) {
      await this.audioContext.close();
      this.audioContext = null;
      this.analyser = null;
      this.microphoneSource = null;
    }
  }
}

export const AudioProcessor = new AudioProcessorService();
