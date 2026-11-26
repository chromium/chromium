// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AudioLevelsProcessor} from './audio_levels.js';
import type {Bump} from './audio_simulation_utils.js';
import {clamp, getAmbientSimulatedMotion, makeSimulatedAudioBump, MS_PER_FRAME, RECORDING_WAVE_AMPLITUDE_DECAY_RATE, RECORDING_WAVE_SPEECH_BUMP_DURATION_MULT, RECORDING_WAVE_SPEECH_BUMP_DURATION_OFFSET, RECORDING_WAVE_SPEECH_BUMP_MAX_VOL_MULT, RECORDING_WAVE_SPEECH_BUMP_MAX_VOL_OFFSET, RECORDING_WAVE_SPEECH_RECEIVED_VOL_SPIKE, RECORDING_WAVE_SYLLABLE_BUMP_DURATION_MULT, RECORDING_WAVE_SYLLABLE_BUMP_DURATION_OFFSET, RECORDING_WAVE_SYLLABLE_BUMP_MAX_VOL_MULT, RECORDING_WAVE_SYLLABLE_BUMP_MAX_VOL_OFFSET, RECORDING_WAVE_TRANSCRIPT_MIN_SPIKE, RECORDING_WAVE_TRANSCRIPT_SPIKE_RANGE, triggerSyllableBumps, updateBumpsAndGetSum} from './audio_simulation_utils.js';

export class AudioProcessorService {
  private audioContext: AudioContext|null = null;
  private analyser: AnalyserNode|null = null;
  private levels: Uint8Array<ArrayBuffer>|Float32Array<ArrayBuffer>|null = null;
  private audioLevelsProcessor = new AudioLevelsProcessor();
  private mediaStream: MediaStream|null = null;
  private microphoneSource: MediaStreamAudioSourceNode|null = null;

  // Simulation mode is currently only enabled on Android (via
  // `ComposeboxVoiceSearchElement`), as opening a concurrent microphone stream
  // conflicts with Android speech recognition.
  private simulate: boolean = false;
  private isListening: boolean = false;
  private sessionId: number = 0;
  private startTimeMs: number = 0;
  private lastVolumeTimeMs: number = 0;
  private decayingAmplitude: number = 0;
  private pendingSpikeAmplitude: number = 0;
  private activeSimulatedBumps: Bump[] = [];
  private receivedSpeech: boolean = false;
  private lastSyllableCount: number = 0;

  setSimulate(simulate: boolean): void {
    this.simulate = simulate;
  }

  // Returning a boolean lets the UI know if it actually worked
  async startMonitoringLevels(): Promise<boolean> {
    if (this.simulate) {
      this.sessionId++;
      this.isListening = true;
      this.startTimeMs = performance.now();
      this.lastVolumeTimeMs = this.startTimeMs;
      this.decayingAmplitude = 0;
      this.pendingSpikeAmplitude = 0;
      this.activeSimulatedBumps = [];
      this.lastSyllableCount = 0;
      this.receivedSpeech = false;
      return true;
    }

    // Do not create another audio stream if one already exists or is starting.
    if (this.audioContext || this.isListening) {
      return false;
    }

    this.isListening = true;
    const sessionId = ++this.sessionId;
    let stream: MediaStream|null = null;
    let audioContext: AudioContext|null = null;
    try {
      stream = await navigator.mediaDevices.getUserMedia({audio: true});
      // If `sessionId` no longer matches (or `isListening` was cleared),
      // `stopListening()` was called while awaiting `getUserMedia()`.
      if (sessionId !== this.sessionId || !this.isListening) {
        stream.getTracks().forEach((track) => track.stop());
        return false;
      }

      audioContext = new AudioContext({sampleRate: 16000});
      if (audioContext.state === 'suspended') {
        await audioContext.resume();
      }
      // If `sessionId` no longer matches (or `isListening` was cleared),
      // `stopListening()` was called while awaiting `audioContext.resume()`.
      if (sessionId !== this.sessionId || !this.isListening) {
        stream.getTracks().forEach((track) => track.stop());
        await audioContext.close().catch(() => {});
        return false;
      }
      // Used to derive numbers like volume and frequency from the audio stream.
      const analyser = audioContext.createAnalyser();

      const microphoneSource = audioContext.createMediaStreamSource(stream);

      // Plug the mic straight into the analyser. Done.
      microphoneSource.connect(analyser);

      // Fast Fourier Transform - higher means more detail, but slower
      // calculations.
      analyser.fftSize = 1024;

      // Set the default smoothing to 0 to utilize the Chrome custom smoother.
      analyser.smoothingTimeConstant = 0;

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
      const bucketSize = analyser.frequencyBinCount;
      this.mediaStream = stream;
      this.audioContext = audioContext;
      this.analyser = analyser;
      this.microphoneSource = microphoneSource;
      this.levels = new Float32Array(bucketSize);
      return true;

    } catch (err) {
      // Only stop listening if the error belongs to this session; do not clear
      // `isListening` for a newer valid session.
      if (sessionId === this.sessionId) {
        this.isListening = false;
      }
      // Stop mic tracks before closing `audioContext` to avoid a delay.
      if (stream) {
        stream.getTracks().forEach((track) => track.stop());
      }
      if (audioContext) {
        await audioContext.close().catch(() => {});
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
        this.decayingAmplitude *=
            Math.pow(RECORDING_WAVE_AMPLITUDE_DECAY_RATE, elapsedFrames);
      }
      // Attack: snap up immediately if new volume or an unsampled speech spike
      // is louder, clamped to [0, 1] so overlapping bumps begin decaying
      // immediately once they pass.
      this.decayingAmplitude = clamp(
          Math.max(
              this.decayingAmplitude, rawInputLevel,
              this.pendingSpikeAmplitude),
          0, 1);
      this.pendingSpikeAmplitude = 0;

      return this.decayingAmplitude;
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
    const previousSyllableCount = this.lastSyllableCount;
    this.lastSyllableCount = triggerSyllableBumps(
        this.activeSimulatedBumps, transcript, previousSyllableCount,
        this.getCurrentVirtualFrame(),
        RECORDING_WAVE_SYLLABLE_BUMP_DURATION_MULT,
        RECORDING_WAVE_SYLLABLE_BUMP_DURATION_OFFSET,
        RECORDING_WAVE_SYLLABLE_BUMP_MAX_VOL_MULT,
        RECORDING_WAVE_SYLLABLE_BUMP_MAX_VOL_OFFSET);

    if (this.lastSyllableCount > previousSyllableCount) {
      this.pendingSpikeAmplitude = Math.max(
          this.pendingSpikeAmplitude,
          RECORDING_WAVE_TRANSCRIPT_MIN_SPIKE +
              Math.random() * RECORDING_WAVE_TRANSCRIPT_SPIKE_RANGE);
    }
  }

  updateReceivedSpeech(receivedSpeech: boolean): void {
    if (!this.simulate || !this.isListening) {
      return;
    }
    if (receivedSpeech && !this.receivedSpeech) {
      this.receivedSpeech = true;
      const currentVirtualFrame = this.getCurrentVirtualFrame();
      makeSimulatedAudioBump(
          this.activeSimulatedBumps, RECORDING_WAVE_SPEECH_BUMP_DURATION_MULT,
          RECORDING_WAVE_SPEECH_BUMP_DURATION_OFFSET, currentVirtualFrame,
          RECORDING_WAVE_SPEECH_BUMP_MAX_VOL_MULT,
          RECORDING_WAVE_SPEECH_BUMP_MAX_VOL_OFFSET);
      this.pendingSpikeAmplitude = Math.max(
          this.pendingSpikeAmplitude, RECORDING_WAVE_SPEECH_RECEIVED_VOL_SPIKE);
    } else if (!receivedSpeech) {
      this.receivedSpeech = false;
    }
  }

  private getCurrentVirtualFrame(): number {
    return (performance.now() - this.startTimeMs) / MS_PER_FRAME;
  }

  // Cleanup.
  async stopListening(): Promise<void> {
    this.sessionId++;
    this.isListening = false;
    if (this.simulate) {
      this.activeSimulatedBumps = [];
      this.decayingAmplitude = 0;
      this.pendingSpikeAmplitude = 0;
      this.lastSyllableCount = 0;
      this.receivedSpeech = false;
      return;
    }
    if (this.mediaStream) {
      this.mediaStream.getTracks().forEach((track) => track.stop());
      this.mediaStream = null;
    }
    if (this.audioContext) {
      const audioContext = this.audioContext;
      this.microphoneSource?.disconnect();
      this.audioContext = null;
      this.analyser = null;
      this.levels = null;
      this.microphoneSource = null;
      await audioContext.close().catch(() => {});
    }
  }
}

export const AudioProcessor = new AudioProcessorService();
