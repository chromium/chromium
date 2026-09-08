'use strict';

let senderInterval = null;
const time = performance.now();
const kAudioLevel = 0.5;
let opusFrameData = null;

function getEncodedFramePayload() {
  return new Promise((resolve, reject) => {
    const myEncoder = new AudioEncoder({
      output: (encodedChunk) => {
        // 1. Create a Uint8Array of the exact size required
        const chunkData = new Uint8Array(encodedChunk.byteLength);

        // 2. Copy the encoded audio chunk's data into the buffer
        encodedChunk.copyTo(chunkData);

        // 3. Resolve with the Uint8Array
        resolve(chunkData);

        myEncoder.close();  // Cleanup encoder resources
      },
      error: (err) => {
        reject(err);
        myEncoder.close();
      }
    });

    const myConfig = {
      codec: 'opus',
      sampleRate: 48000,
      numberOfChannels: 2,
      bitrate: 64000,
      opus: {
        frameDuration: 10000,
      }
    };

    myEncoder.configure(myConfig);

    // 10ms of silence at 48kHz (480 frames)
    const numberOfFrames = 480;
    const myAudioData =
        new Float32Array(numberOfFrames * myConfig.numberOfChannels);
    const myFrame = new AudioData({
      timestamp: 0,
      data: myAudioData,
      numberOfChannels: myConfig.numberOfChannels,
      numberOfFrames: numberOfFrames,
      sampleRate: myConfig.sampleRate,
      format: 'f32-planar',
    });

    myEncoder.encode(myFrame);
    myFrame.close();  // Release frame memory immediately

    myEncoder.flush().catch(reject);
  });
}

self.onrtcsenderencodedsource = async (event) => {
  const source = event.encodedSource;
  const writer = source.writable.getWriter();

  opusFrameData = await getEncodedFramePayload();

  function sendFrame() {
    try {
      const frame = new RTCEncodedAudioFrame({
        contentType: 'speech',
        rtpTimestampWithoutOffset: 101010,
        data: opusFrameData.buffer,
        payloadType: 111,
        mimeType: 'audio/opus',
        contributingSources: [1234],
        captureTime: time,
        audioLevel: kAudioLevel,
      });
      writer.write(frame);
    } catch (e) {
      self.postMessage({error: 'Sender error: ' + e.message});
    }
  }

  senderInterval = setInterval(sendFrame, 100);
};

self.onrtctransform = async (event) => {
  const transformer = event.transformer;
  const reader = transformer.readable.getReader();

  try {
    const frameOrDone = await reader.read();
    const frame = frameOrDone.value;
    const done = frameOrDone.done;

    if (done || !frame) {
      self.postMessage({error: 'No frame received.'});
      return;
    }

    const metadata = frame.getMetadata();
    const receivedBytes = new Uint8Array(frame.data);
    const dataMatches =
        (receivedBytes.byteLength === opusFrameData.byteLength) &&
        opusFrameData.every((val, idx) => val === receivedBytes[idx]);
    self.postMessage({
      payloadType: metadata.payloadType,
      mimeType: metadata.mimeType,
      dataLength: frame.data.byteLength,
      expectedDataLength: opusFrameData.byteLength,
      dataMatches: dataMatches,
      receivedBytes: Array.from(receivedBytes),
      sentBytes: Array.from(opusFrameData),
      contributingSources: metadata.contributingSources,
      captureTime: metadata.captureTime,
      // Use a 20ms tolerance to account for differences due to possible changes
      // during transmission
      correctCaptureTime: metadata.captureTime !== undefined &&
          Math.abs(time - metadata.captureTime) <= 20,
      audioLevel: metadata.audioLevel,
      expectedAudioLevel: kAudioLevel,
    });
  } catch (e) {
    self.postMessage({error: 'Receiver error: ' + e.message});
  }
};
