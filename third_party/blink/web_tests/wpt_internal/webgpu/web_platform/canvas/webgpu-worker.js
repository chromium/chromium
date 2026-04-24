let queue;
let ctx;
let devicePromise = null;

self.onmessage = async function(e) {
  try {
    if (e.data.canvas) {
      if (!devicePromise) {
        devicePromise = (async () => {
          const adapter = await navigator.gpu.requestAdapter();
          const device = await adapter.requestDevice();
          queue = device.queue;
          return device;
        })();
      }
      const device = await devicePromise;

      const cvs = e.data.canvas;
      cvs.clientWidth = e.data.clientWidth;
      cvs.clientHeight = e.data.clientHeight;

      ctx = cvs.getContext("webgpu");
      ctx.configure({
        device: device,
        format: navigator.gpu.getPreferredCanvasFormat(),
        usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
        alphaMode: "premultiplied",
      });
    }

    if (e.data.elementImage) {
      if (devicePromise) await devicePromise;
      const args = e.data.args || [];
      const target = e.data.elementImage;
      const destination = { texture: ctx.getCurrentTexture() };

      if (args.length === 6) {
        // [sx, sy, swidth, sheight, destWidth, destHeight]
        queue.copyElementImageToTexture(target, args[0], args[1], args[2], args[3], args[4], args[5], destination);
      } else if (args.length === 4) {
        // [sx, sy, swidth, sheight]
        queue.copyElementImageToTexture(target, args[0], args[1], args[2], args[3], destination);
      } else if (args.length === 2) {
        // [destWidth, destHeight]
        queue.copyElementImageToTexture(target, args[0], args[1], destination);
      } else {
        queue.copyElementImageToTexture(target, destination);
      }
      self.postMessage('done');
    }
  } catch (err) {
    self.postMessage('error: ' + err.message + '\n' + err.stack);
  }
};
