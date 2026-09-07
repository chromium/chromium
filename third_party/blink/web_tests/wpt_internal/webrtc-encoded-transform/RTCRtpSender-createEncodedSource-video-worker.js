'use strict';

let senderInterval = null;
const time = performance.now();
let vp8_FrameData = null;

function getEncodedFramePayload() {
  return new Promise((resolve, reject) => {
    const myEncoder = new VideoEncoder({
      output: (encodedChunk) => {
        // 1. Create a Uint8Array of the exact size required
        const chunkData = new Uint8Array(encodedChunk.byteLength);

        // 2. Copy the encoded video chunk's data into the buffer
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
      codec: 'vp8',
      width: 640,
      height: 480,
      bitrate: 100_000,
      framerate: 30
    };

    myEncoder.configure(myConfig);

    // Minimal frame generation (RGBA: 4 bytes per pixel)
    const myPixelData = new Uint8Array(myConfig.width * myConfig.height * 4);
    const myFrame = new VideoFrame(myPixelData, {
      timestamp: 0,
      codedWidth: myConfig.width,
      codedHeight: myConfig.height,
      format: 'RGBA'
    });

    myEncoder.encode(myFrame);
    myFrame.close();  // Release frame memory immediately

    myEncoder.flush().catch(reject);
  });
}

self.onrtcsenderencodedsource = async (event) => {
  const source = event.encodedSource;
  const writer = source.writable.getWriter();

  vp8_FrameData = await getEncodedFramePayload();

  function sendFrame() {
    try {
      const frame = new RTCEncodedVideoFrame({
        type: 'key',
        rtpTimestampWithoutOffset: 101010,
        data: vp8_FrameData.buffer,
        payloadType: 96,
        mimeType: 'video/VP8',
        width: 640,
        height: 480,
        contributingSources: [1234],
        captureTime: time,
      });
      writer.write(frame);
    } catch (e) {
      self.postMessage({error: 'Sender error: ' + e.message});
    }
  }

  senderInterval = setInterval(sendFrame, 500);
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
        (receivedBytes.byteLength === vp8_FrameData.byteLength) &&
        vp8_FrameData.every((val, idx) => val === receivedBytes[idx]);
    self.postMessage({
      type: frame.type,
      width: metadata.width,
      height: metadata.height,
      payloadType: metadata.payloadType,
      dataLength: frame.data.byteLength,
      expectedDataLength: vp8_FrameData.byteLength,
      dataMatches: dataMatches,
      receivedBytes: Array.from(receivedBytes),
      sentBytes: Array.from(vp8_FrameData),
      contributingSources: metadata.contributingSources,
      // Use a 3ms tolerance to account for differences due to possible changes
      // during transmission
      correctCaptureTime: metadata.captureTime !== undefined &&
          Math.abs(time - metadata.captureTime) <= 3,
    });
  } catch (e) {
    self.postMessage({error: 'Receiver error: ' + e.message});
  }
};
