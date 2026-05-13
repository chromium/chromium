// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class AudioProcessorService {
  private audioContext: AudioContext|null = null;
  private analyser: AnalyserNode|null = null;
  private levels: Uint8Array<ArrayBuffer>|null = null;
  private mediaStream: MediaStream|null = null;
  private microphoneSource: MediaStreamAudioSourceNode|null = null;

  // Number of buckets to clip useless audio from low end and high end of
  // frequency spectrum. See comments on `startMonitoringLevels()` for more
  // details on frequency range cutoffs and bucket calculations.
  private lowEndClip = 6;
  private highEndClip = 128;

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
      this.analyser.smoothingTimeConstant = 0.2;

      // frequencyBinCount is half of fftSize, so 512 in this case.
      // Assuming a 16000Hz sample rate, the maximum measurable frequency
      // is 8000Hz.  Each bucket represents 15.625 Hz (8000Hz / 512).
      //   Bucket [0, 6): low frequencies (0 to ~93Hz; rumble, so will be
      //   clipped).
      //   Bucket [6, 384): frequencies containing human voice (up to 6000Hz;
      //   will be used).
      //   Bucket [384, 512): high frequencies (6000Hz to 8000Hz; noise,
      //   so it will be clipped).
      const bucketSize = this.analyser.frequencyBinCount;
      // Speech webkit requires data structure of values from 0-255.
      this.levels = new Uint8Array(bucketSize);
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
    this.analyser.getByteFrequencyData(this.levels);

    // Clip unusable low and high frequencies that are often just noise (like HPF/LPF).
    // See comments on startMonitoringLevels()` for frequency range details.
    const usableLevels = this.levels.slice(
        this.lowEndClip,
        this.levels.length - this.highEndClip,
    );

    if (usableLevels.length === 0) {
      return 0;
    }
    const sum = usableLevels.reduce((a, b) => a + b, 0);
    const averageAmplitude = sum / usableLevels.length;
    // Normalize the amplitude based on Uint8 to
    // 0 and 1, which the UI expects.
    return averageAmplitude / 255;
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
