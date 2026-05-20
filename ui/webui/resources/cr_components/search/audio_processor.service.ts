// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AudioLevelsProcessor} from './audio_levels.js';

export class AudioProcessorService {
  private audioContext: AudioContext|null = null;
  private analyser: AnalyserNode|null = null;
  private levels: Uint8Array<ArrayBuffer>|Float32Array<ArrayBuffer>|null = null;
  private audioLevelsProcessor = new AudioLevelsProcessor();
  private mediaStream: MediaStream|null = null;
  private microphoneSource: MediaStreamAudioSourceNode|null = null;



  // Returning a boolean lets the UI know if it actually worked
  async startMonitoringLevels(): Promise<boolean> {
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
    if (!this.analyser || !this.levels) {
      return 0;
    }

    const floatBuffer = this.levels as Float32Array<ArrayBuffer>;
    this.analyser.getFloatFrequencyData(floatBuffer);
    return this.audioLevelsProcessor.process(floatBuffer, performance.now());
  }

  // Cleanup.
  stopListening(): void {
    if (this.mediaStream) {
      this.mediaStream.getTracks().forEach((track) => track.stop());
      this.mediaStream = null;
    }
    if (this.audioContext) {
      this.audioContext.close();
      this.audioContext = null;
      this.analyser = null;
      this.microphoneSource = null;
    }
  }
}

export const AudioProcessor = new AudioProcessorService();
