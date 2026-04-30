// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class AudioProcessorService {
  private audioContext: AudioContext|null = null;
  private analyser: AnalyserNode|null = null;
  private levels: Uint8Array<ArrayBuffer>|null = null;
  private mediaStream: MediaStream|null = null;
  private microphoneSource: MediaStreamAudioSourceNode|null = null;

  private lowEndClip = 4;
  private highEndClip = 4;

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
      this.analyser.fftSize = 256;
      this.analyser.smoothingTimeConstant = 0.2;

      // FrequencyBinCount is half of fftSize, so 16 in this case. 16000Hz
      // (human hearing range) is used by browser. Each bucket is 500 Hz. Bucket
      // [0, 4): low frequencies (just noise, so will be clipped) Bucket [4,
      // 12): frequencies commonly associated with human voice
      //   (will be used); 2000Hz to 6000Hz.
      // Bucket [12, 16): high frequencies (just noise, so will be clipped)
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

    // Clip unusable low and high frequencies that are often just noise.
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
