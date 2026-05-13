function makeVideoFrame(timestamp) {
  const canvas = new OffscreenCanvas(100, 100);
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = 'black';
  ctx.fillRect(0, 0, 100, 100);
  return new VideoFrame(canvas, {timestamp});
}

function makeAudioData(timestamp) {
  const sampleRate = 30000;
  let frames = sampleRate / 10;
  let channels = 1;
  let data = new Float32Array(frames * channels);
  const hz = 100;
  for (let i = 0; i < data.length; i++) {
    const t = (i / sampleRate) * hz * (Math.PI * 2);
    data[i] = Math.sin(t);
  }
  return new AudioData({
    timestamp: timestamp,
    numberOfFrames: frames,
    numberOfChannels: channels,
    sampleRate: sampleRate,
    data: data,
    format: 'f32',
  });
}

const workerCode = `
  let reader;

  self.addEventListener('message', async (e) => {
    if (e.data.readable) {
      reader = e.data.readable.getReader();
      if (e.data.mode === 'read') {
        try {
          while (true) {
            const {done, value} = await reader.read();
            if (done) break;
            value.close();
            self.postMessage('read');
          }
        } catch (err) {
          self.postMessage('error: ' + err);
        }
      } else {
        // Mode not 'read', so just hold the reader to cause backpressure
        self.postMessage('ready');
      }
    } else if (e.data === 'cancel') {
      if (reader) {
        await reader.cancel();
        self.postMessage('cancelled');
      }
    }
  });
`;

function makeWorker() {
  const blob = new Blob([workerCode], {type: 'application/javascript'});
  return new Worker(URL.createObjectURL(blob));
}
